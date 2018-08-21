/*
 * ws-client
 *
 * Copyright (C) 2018 yanpeipan <yanpeipan_82@qq.com>
 */

#include <libwebsockets.h>
#include <string.h>
#include <signal.h>
#include <pthread.h>

#include "ws-protocol.h"

#define RING_DEPTH 1024
#define TEST 1

static int interrupted;
static struct lws *client_wsi;
const char *vin = "0ebfb";

pthread_t pthread_spam_t;
struct geo {
    double lat;
    double lng;
} path[50] = {
 { 40.045961369356654, 116.28454125826974},
 { 40.045961369356654, 116.28454125826974},
 { 40.04584989536292, 116.28440992457523},
 { 40.04584989536292, 116.28440992457523},
 { 40.04580994072857, 116.28434991711421},
 { 40.045359916517135, 116.28462992198828},
 { 40.04506991615041, 116.28470996188011},
 { 40.044699935233865, 116.28468992944929},
 { 40.044349964448344, 116.28462992198828},
 { 40.04334996924771, 116.28451996819751},
 { 40.04334996924771, 116.28451996819751},
 { 40.04328993193273, 116.28454997192802},
 { 40.04353723306462, 116.28641002355529},
 { 40.04353723306462, 116.28641002355529},
 { 40.04354995570704, 116.28651997734607},
 { 40.04354995570704, 116.28651997734607},
 { 40.042169982178955, 116.28681992481945},
 { 40.040619971630456, 116.28708995839386},
 { 40.040619971630456, 116.28708995839386},
 { 40.04051997417694, 116.28695997217226},
 { 40.04048998866711, 116.28687993228043},
 { 40.04041997657677, 116.2865099162149},
 { 40.04041997657677, 116.2865099162149},
 { 40.04015994086532, 116.28655995237624},
 { 40.03756991454572, 116.2869199971421},
 { 40.03514995055951, 116.28729998450727},
 { 40.03442975654792, 116.28745539305143},
 { 40.03398529723735, 116.28761376603602},
 { 40.03398529723735, 116.28761376603602},
 { 40.03395998594214, 116.28699994720237},
 { 40.033939970753686, 116.28354996735374},
 { 40.033889967147225, 116.28184999551007},
 { 40.03382861466667, 116.28086795724145},
 { 40.033679910331195, 116.27931998034386},
 { 40.03358994473862, 116.2787399381649},
 { 40.03329989430985, 116.27709997378221},
 { 40.03300998021086, 116.27553995945982},
 { 40.03241996854218, 116.27271996811992},
 { 40.03241996854218, 116.27271996811992},
 { 40.032299943394, 116.27264998935927},
 { 40.032299943394, 116.27264998935927},
 { 40.03204998776564, 116.27284995434152},
 { 40.031099955429575, 116.27366993653285},
 { 40.030269732439095, 116.27441311276743},
 { 40.02830989836842, 116.27609996920793},
 { 40.02727995176468, 116.2770199338904},
 { 40.02545994932868, 116.27857994821281},
 { 40.02428997125841, 116.27961992781722},
 { 40.02428997125841, 116.27961992781722},
 { 40.02246995784657, 116.28125998203146},
};

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
	pthread_mutex_t lock_ring;
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

static void *thread_spam(void *d)
{

	struct per_session_data *vhd =
			(struct per_session_data *)d;
	struct msg amsg;
	int len = 128, index = 1, n;

	pthread_cleanup_push(pthread_mutex_unlock, (void *) &vhd->lock_ring);
	pthread_mutex_lock(&vhd->lock_ring); /* --------- ring lock { */
	for (int i = 0; i < ARRAY_SIZE(path); i++) {
		insert_message(get_info_msg(path[i].lat, path[i].lng), vhd);
		sleep(1);
	}
	pthread_mutex_unlock(&vhd->lock_ring); /* } ring lock ------- */
	pthread_cleanup_pop(0);

	pthread_exit(NULL);
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
			pthread_mutex_init(&vhd->lock_ring, NULL);

			vhd->context = lws_get_context(wsi);
			vhd->vhost = lws_get_vhost(wsi);

			if (connect_client(vhd)) {
				lws_timed_callback_vh_protocol(vhd->vhost, vhd->protocol, LWS_CALLBACK_USER, 1);
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

			#if TEST
			insert_message(get_login_msg(vin), vhd);
			#endif

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

					#if TEST
					insert_message(get_info_msg(path[0].lat, path[0].lng), vhd);
					#endif

					break;
				case CMD_VEHICLE_HAILING:

					#if TEST
					lwsl_user("CMD_VEHICLE_HAILING: %f %f\n", ((Info)(msg->body))->lat, ((Info)(msg->body))->lng);
					if (pthread_create(&pthread_spam_t, NULL, thread_spam, vhd)) {
						lwsl_err("thread creation failed\n");
						r = 1;
					}
					#endif

					break;

				case CMD_USER_ABOARD:

					break;

				case CMD_USER_GETOFF:

					#if TEST
					insert_message(get_hailing_end_msg(CODE_OK), vhd);
					if (pthread_cancel(pthread_spam_t)) {
						lwsl_user("pthread_cancel fail\n");
					}
					insert_message(get_info_msg(path[0].lat, path[0].lng), vhd);
					#endif

					break;

				case CMD_VEHICLE_PARKING:

					#if TEST
					insert_message(get_parking_end_msg(CODE_OK), vhd);
					#endif

					break;
			}

			break;

		case LWS_CALLBACK_CLOSED:
			vhd->client_wsi = NULL;
			lws_cancel_service(lws_get_context(wsi));
			break;

		case LWS_CALLBACK_WSI_DESTROY:
			lwsl_user("LWS_CALLBACK_WSI_DESTROY\n");

			#if TEST
			pthread_mutex_destroy(&vhd->lock_ring);
			#endif

			sleep(3);
			if (connect_client(vhd)) {
				lws_timed_callback_vh_protocol(vhd->vhost, vhd->protocol, LWS_CALLBACK_USER, 1);
			}
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

		case LWS_CALLBACK_CHANGE_MODE_POLL_FD:
		case LWS_CALLBACK_LOCK_POLL:
		case LWS_CALLBACK_UNLOCK_POLL:
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