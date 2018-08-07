/*
 * ws-client
 *
 * Copyright (C) 2018 yanpeipan <yanpeipan_82@qq.com>
 */

#include <libwebsockets.h>
#include <string.h>
#include <signal.h>

#include "ws-protocol.h"

#define RING_DEPTH 1024

static int interrupted;
static struct lws *client_wsi;
const char *vin = "0ebfb";

struct msg {
    void *payload;
    size_t len;
};

static void _destroy_message(void *_msg)
{
    struct msg *msg = _msg;

    free(msg->payload);

    msg->payload = NULL;
    msg->len = 0;
}

struct per_session_data {
	struct lws_context *context;
	struct lws_vhost *vhost;
	const struct lws_protocols *protocol;
	struct lws_ring *ring;
	struct lws_client_connect_info i;

	int is_login;

	struct lws *client_wsi;
	uint32_t tail;
};

static int connect_client(struct per_session_data *vhd)
{
	vhd->i.context = vhd->context;
	vhd->i.port = 443;
	vhd->i.address = "ht.wxapp.higoauto.com";
	vhd->i.path = "/vehicle";
	vhd->i.host = vhd->i.address;
	vhd->i.origin = vhd->i.address;
	vhd->i.ssl_connection = LCCSCF_USE_SSL;
	// vhd->i.protocol = "lws-minimal-broker";
	vhd->i.pwsi = &vhd->client_wsi;

	lwsl_user("connecting to %s:%d%s\n", vhd->i.address, vhd->i.port, vhd->i.path);

	return !lws_client_connect_via_info(&vhd->i);
}

static int insert_message(const char *msg, void * d)
{
	lwsl_user("insert_message %s\n", msg);
	struct per_session_data *vhd = (struct per_session_data *)d;

	int n = (int)lws_ring_get_count_free_elements(vhd->ring);
	if (!n) {
		lwsl_user("lws_ring_get_count_free_elements %d\n", n);
		return -1;
	}
	struct msg amsg;

	amsg.len = strlen(msg);
			/* notice we over-allocate by LWS_PRE */
	amsg.payload = malloc(LWS_PRE + amsg.len);
	if (!amsg.payload) {
		lwsl_user("OOM: dropping\n");
		return -1;
	}

	memcpy((char *)amsg.payload + LWS_PRE, (char *)msg, amsg.len);

	if (!lws_ring_insert(vhd->ring, &amsg, 1)) {
		_destroy_message(&amsg);
		lwsl_err("lws_ring_insert!\n");
		return -1;
	}

	lws_callback_on_writable(vhd->client_wsi);

	if (n < 3)
		lws_rx_flow_control(vhd->client_wsi, 0);
	
	return 0;
}

static int callback_hailing(struct lws *wsi, enum lws_callback_reasons reason, void *user, void *in, size_t len)
{
	struct per_session_data *vhd = (struct per_session_data *)lws_protocol_vh_priv_get(
		lws_get_vhost(wsi),
		lws_get_protocol(wsi)
	);

	const struct msg *pmsg;
	struct msg amsg;
	int n, m, r = 0, flags;
	void *retval;

	switch (reason) {
		case LWS_CALLBACK_PROTOCOL_INIT:
			lwsl_user("LWS_CALLBACK_PROTOCOL_INIT\n");
			vhd = lws_protocol_vh_priv_zalloc(
				lws_get_vhost(wsi),
				lws_get_protocol(wsi),
				sizeof(struct per_session_data)
			);
			if (!vhd)
				return -1;
			vhd->ring = lws_ring_create(sizeof(struct msg), 8, _destroy_message);
			if (!vhd->ring) {
				lwsl_err("!ring");
				return 1;
			}

			vhd->context = lws_get_context(wsi);
			vhd->vhost = lws_get_vhost(wsi);

			if (connect_client(vhd)) {
				lws_timed_callback_vh_protocol(vhd->vhost, vhd->protocol, LWS_CALLBACK_USER, 1);
			} else {
				lwsl_err("connect fail\n");
			}
			break;

		case LWS_CALLBACK_USER:
			lwsl_notice("%s: LWS_CALLBACK_USER\n", __func__);
			if (connect_client(vhd))
				lws_timed_callback_vh_protocol(
					vhd->vhost,
					vhd->protocol,
					LWS_CALLBACK_USER,
					1
				);
			break;

		case LWS_CALLBACK_CLIENT_CONNECTION_ERROR:
			lwsl_err("CLIENT_CONNECTION_ERROR: %s\n",
				in ? (char *)in : "(null)");
			vhd->client_wsi = NULL;
			break;

		case LWS_CALLBACK_CLIENT_ESTABLISHED:
			lwsl_user("LWS_CALLBACK_CLIENT_ESTABLISHED\n");
			vhd->ring = lws_ring_create(sizeof(struct msg), RING_DEPTH, _destroy_message);
			if (!vhd->ring)
				return 1;
			vhd->tail = 0;

			insert_message(get_login_msg(vin), vhd);
			break;
			
		case LWS_CALLBACK_CLIENT_RECEIVE:
			lwsl_user("LWS_CALLBACK_CLIENT_RECEIVE: %s\n", in);
			Message msg = parse_msg((char *) in);
			if (msg->code != CODE_OK) {
				lwsl_err("code: %d\n", msg->code);
				return -1;
			}
			switch (msg->cmd) {
				case CMD_VEHICLE_LOGIN:
					insert_message(get_info_msg(40.046055,116.284325), vhd);
					break;
				case CMD_VEHICLE_HAILING:
					lwsl_user("CMD_VEHICLE_HAILING: %f %f\n", ((Info)(msg->body))->lat, ((Info)(msg->body))->lng);
					break;
				case CMD_VEHICLE_ABOARD:
					insert_message(get_hailing_end_msg(CODE_OK), vhd);
					break;
				case CMD_VEHICLE_PARKING:
					insert_message(get_parking_end_msg(CODE_OK), vhd);
					break;
			}

			break;

		case LWS_CALLBACK_CLOSED:
			lws_ring_destroy(vhd->ring);
			vhd->client_wsi = NULL;
			lws_cancel_service(lws_get_context(wsi));
			break;

		case LWS_CALLBACK_WSI_DESTROY:
			lwsl_user("LWS_CALLBACK_WSI_DESTROY\n");
			break;

		case LWS_CALLBACK_PROTOCOL_DESTROY:
			lwsl_user("LWS_CALLBACK_PROTOCOL_DESTROY\n");
			if (vhd->ring)
				lws_ring_destroy(vhd->ring);
			break;

		case LWS_CALLBACK_CLIENT_WRITEABLE:
			lwsl_user("LWS_CALLBACK_SERVER_WRITEABLE\n");
			do {
				pmsg = lws_ring_get_element(vhd->ring, &vhd->tail);
				if (!pmsg) {
					// lwsl_user("!pmsg\n");
					break;
				}
				m = lws_write(wsi, pmsg->payload + LWS_PRE, pmsg->len, LWS_WRITE_TEXT);
				if (m < (int)pmsg->len) {
					lwsl_err("ERROR %d writing to ws socket\n", m);
					return -1;
				}
				// lws_ring_consume_single_tail(vhd->ring, &vhd->tail, 1);
				lws_ring_consume_single_tail(vhd->ring, &vhd->tail, 1);

				if (lws_ring_get_element(vhd->ring, &vhd->tail) ) {
					break;
				}
				if (!lws_send_pipe_choked(wsi)) {
					break;
				}

			} while (1);

			/* more to do for us? */
			if (lws_ring_get_element(vhd->ring, &vhd->tail))
			/* come back as soon as we can write more */
				lws_callback_on_writable(wsi);
			if ((int)lws_ring_get_count_free_elements(vhd->ring) > RING_DEPTH - 5)
				lws_rx_flow_control(wsi, 1);
			break;

		default:
			break;

		}

	return lws_callback_http_dummy(wsi, reason, user, in, len);
}

static const struct lws_protocols protocols[] = {
	{
		"car-hailing-protocol",
		callback_hailing,
		sizeof(struct per_session_data),
		1024,
		0, NULL, 0
	},
	{ NULL, NULL, 0, 0 }
};

static void sigint_handler(int sig)
{
	interrupted = 1;
}

int main(int argc, const char **argv)
{
	struct lws_context_creation_info info;
	struct lws_context *context;
	const char *p;
	int n = 0, logs = LLL_USER | LLL_ERR | LLL_WARN | LLL_NOTICE
			/* for LLL_ verbosity above NOTICE to be built into lws,
			 * lws must have been configured and built with
			 * -DCMAKE_BUILD_TYPE=DEBUG instead of =RELEASE */
			/* | LLL_INFO */ /* | LLL_PARSER */ /* | LLL_HEADER */
			/* | LLL_EXT */ /* | LLL_CLIENT */ /* | LLL_LATENCY */
			/* | LLL_DEBUG */;

	signal(SIGINT, sigint_handler);

	lws_set_log_level(logs, NULL);
	lwsl_user("WebSocket Client\n");
	lwsl_user("libwebsockets version: %s\n", LWS_LIBRARY_VERSION);

	memset(&info, 0, sizeof info); /* otherwise uninitialized garbage */
	info.options = LWS_SERVER_OPTION_DO_SSL_GLOBAL_INIT;
	info.port = CONTEXT_PORT_NO_LISTEN; /* we do not run any server */
	info.protocols = protocols;
	// info.client_ssl_ca_filepath = "./214768363430417.pfx";
	// info.ssl_private_key_password = "214768363430417";

	context = lws_create_context(&info);
	if (!context) {
		lwsl_err("lws init failed\n");
		return 1;
	}

	while (n >= 0 && !interrupted)
		n = lws_service(context, 1000);

	lws_context_destroy(context);
	lwsl_user("Completed\n");

	return 0;
}