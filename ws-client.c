/*
 * lws-minimal-ws-client
 *
 * Copyright (C) 2018 Andy Green <andy@warmcat.com>
 *
 * This file is made available under the Creative Commons CC0 1.0
 * Universal Public Domain Dedication.
 *
 * This demonstrates the a minimal ws client using lws.
 *
 * It connects to https://libwebsockets.org/ and makes a
 * wss connection to the dumb-increment protocol there.  While
 * connected, it prints the numbers it is being sent by
 * dumb-increment protocol.
 */

#include <libwebsockets.h>
#include <string.h>
#include <signal.h>

#include "protocol.c"

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
	struct lws *client_wsi;
	uint32_t tail;
};

static int connect_client(struct per_session_data *vhd)
{
	vhd->i.context = vhd->context;
	vhd->i.port = 8082;
	vhd->i.address = "localhost";
	vhd->i.path = "/vehicle";
	vhd->i.host = vhd->i.address;
	vhd->i.origin = vhd->i.address;
	vhd->i.ssl_connection = 0;
	vhd->i.protocol = "lws-minimal-broker";
	vhd->i.pwsi = &vhd->client_wsi;

	return !lws_client_connect_via_info(&vhd->i);
}

static int callback_hailing(struct lws *wsi, enum lws_callback_reasons reason, void *user, void *in, size_t len)
{
	struct per_session_data *vhd = (struct per_session_data *)lws_protocol_vh_priv_get(
		lws_get_vhost(wsi),
		lws_get_protocol(wsi)
	);

	const struct msg *pmsg;
	struct msg amsg;
	int n, m, r = 0;
	const char * loginMsg = get_vehicle_login_message(vin);

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
				lwsl_err("connect success\n");
			} else {
				lwsl_err("connect fail\n");
			}

			lws_callback_vhost_protocols(wsi, LWS_CALLBACK_USER, in, len);
			break;
		case LWS_CALLBACK_USER:

			
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


lwsl_user("dropping!\n");
				n = (int)lws_ring_get_count_free_elements(vhd->ring);
			if (!n) {
				lwsl_user("dropping!\n");
				break;
			}

			amsg.len = strlen(loginMsg);
			/* notice we over-allocate by LWS_PRE */
			amsg.payload = malloc(LWS_PRE + len);
			if (!amsg.payload) {
				lwsl_user("OOM: dropping\n");
				break;
			}

			memcpy((char *)amsg.payload + LWS_PRE, (char *)loginMsg, amsg.len);
			lwsl_user("amsg.payload %s! %s %zu\n", amsg.payload, loginMsg, amsg.len);

			if (!lws_ring_insert(vhd->ring, &amsg, 1)) {
				_destroy_message(&amsg);
				lwsl_user("dropping!\n");
				break;
			}
			lws_callback_on_writable(wsi);

			if (n < 3)
				lws_rx_flow_control(wsi, 0);

			break;
			
		case LWS_CALLBACK_CLIENT_RECEIVE:
			lwsl_user("LWS_CALLBACK_CLIENT_RECEIVE: %4d (rpp %5d, first %d, last %d, bin %d)\n",
				(int)len, (int)lws_remaining_packet_payload(wsi),
				lws_is_first_fragment(wsi),
				lws_is_final_fragment(wsi),
				lws_frame_is_binary(wsi));
/* notice we over-allocate by LWS_PRE */
			
								
			// lwsl_hexdump_notice(in, len);
			break;

		case LWS_CALLBACK_CLOSED:
			lws_ring_destroy(vhd->ring);
			vhd->client_wsi = NULL;
			lws_cancel_service(lws_get_context(wsi));
			break;

		case LWS_CALLBACK_PROTOCOL_DESTROY:
			lwsl_user("LWS_CALLBACK_PROTOCOL_DESTROY\n");
			break;

		case LWS_CALLBACK_CLIENT_WRITEABLE:
			lwsl_user("LWS_CALLBACK_SERVER_WRITEABLE\n");
			do {
				pmsg = lws_ring_get_element(vhd->ring, &vhd->tail);
				if (!pmsg) {
					return 1;
				}

				m = lws_write(wsi, pmsg->payload + LWS_PRE, pmsg->len, LWS_WRITE_TEXT);
				if (m < (int)pmsg->len) {
					lwsl_err("ERROR %d writing to ws socket\n", m);
					return -1;
				}
				// lws_ring_consume_single_tail(vhd->ring, &vhd->tail, 1);
				lws_ring_consume(vhd->ring, &vhd->tail, NULL, 1);
				lws_ring_update_oldest_tail(vhd->ring, *(&vhd->tail));

			} while (lws_ring_get_element(vhd->ring, &vhd->tail) && !lws_send_pipe_choked(wsi));
			break;

		default:
			lwsl_user("default: %d\n", reason);
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
	lwsl_user("LWS minimal ws client tx\n");
	lwsl_user("  Run minimal-ws-broker and browse to that\n");

	memset(&info, 0, sizeof info); /* otherwise uninitialized garbage */
	info.port = CONTEXT_PORT_NO_LISTEN; /* we do not run any server */
	info.protocols = protocols;

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