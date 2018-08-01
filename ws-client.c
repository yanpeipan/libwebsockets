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

static int interrupted, rx_seen, test;
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
};

static int connect_client(struct per_session_data *vhd)
{
	vhd->i.context = vhd->context;
	vhd->i.port = 7681;
	vhd->i.address = "localhost";
	vhd->i.path = "/publisher";
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

	switch (reason) {
		case LWS_CALLBACK_PROTOCOL_INIT:
			vhd = lws_protocol_vh_priv_zalloc(
				lws_get_vhost(wsi),
				lws_get_protocol(wsi),
				sizeof(struct per_session_data)
			);
			if (!vhd)
				return -1;

			vhd->context = lws_get_context(wsi);
			vhd->vhost = lws_get_vhost(wsi);

			if (connect_client(vhd)) {

			} else {
				lwsl_err("connect_client\n");
			}

	case LWS_CALLBACK_CLIENT_CONNECTION_ERROR:
		lwsl_err("CLIENT_CONNECTION_ERROR: %s\n",
			 in ? (char *)in : "(null)");
		client_wsi = NULL;
		break;
	case LWS_CALLBACK_CLIENT_ESTABLISHED:
		vhd->ring = lws_ring_create(sizeof(struct msg), RING_DEPTH, _destroy_message);
		lwsl_user("%s: established\n", __func__);
		if (!vhd->ring)
			return 1;

		const char * loginMsg = get_vehicle_login_message(vin);
		amsg.len = sizeof(loginMsg);
		/* notice we over-allocate by LWS_PRE */
		amsg.payload = malloc(LWS_PRE + sizeof(loginMsg));
		if (!amsg.payload) {
			lwsl_user("OOM: dropping\n");
			break;
		}

		memcpy((char *)amsg.payload + LWS_PRE, loginMsg, amsg.len);
		if (!lws_ring_insert(vhd->ring, &amsg, 1)) {
			_destroy_message(&amsg);
			lwsl_user("dropping!\n");
			break;
		}
		lws_callback_on_writable(wsi);

		break;

	case LWS_CALLBACK_CLIENT_RECEIVE:
		lwsl_user("RX: %s\n", (const char *)in);
		Message *msg = decode((const char *)in);
		lwsl_user("RX: %d\n", msg->cmd);
		break;

	case LWS_CALLBACK_CLOSED:
		client_wsi = NULL;
		break;
	case LWS_CALLBACK_SERVER_WRITEABLE:
		lwsl_user("LWS_CALLBACK_SERVER_WRITEABLE\n");
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
	struct lws_client_connect_info i;
	struct lws_context *context;
	const char *p;
	int n = 0, logs = LLL_USER | LLL_ERR | LLL_WARN | LLL_NOTICE
		/* for LLL_ verbosity above NOTICE to be built into lws, lws
		 * must have been configured with -DCMAKE_BUILD_TYPE=DEBUG
		 * instead of =RELEASE */
		/* | LLL_INFO */ /* | LLL_PARSER */ /* | LLL_HEADER */
		/* | LLL_EXT */ /* | LLL_CLIENT */ /* | LLL_LATENCY */
		/* | LLL_DEBUG */;

	signal(SIGINT, sigint_handler);

	lws_set_log_level(logs, NULL);
	lwsl_user("WS Client %s \n", LWS_LIBRARY_VERSION);

	memset(&info, 0, sizeof info); /* otherwise uninitialized garbage */
	// info.options = LWS_SERVER_OPTION_DO_SSL_GLOBAL_INIT;
	info.port = CONTEXT_PORT_NO_LISTEN; /* we do not run any server */
	info.protocols = protocols;

	context = lws_create_context(&info);
	if (!context) {
		lwsl_err("lws init failed\n");
		return 1;
	}

	while (n >= 0 && client_wsi && !interrupted)
		n = lws_service(context, 1000);

	lws_context_destroy(context);

	lwsl_user("Completed %s\n", rx_seen > 10 ? "OK" : "Failed");

	return rx_seen > 10;
}