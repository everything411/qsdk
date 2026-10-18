/*
 * mqtt_eloop.c - libmosquitto direct eloop integration for hostapd
 *
 *   mosquitto_socket()     → fd registered with eloop_register_read_sock()
 *   mosquitto_loop_read()  → called from eloop read handler
 *   mosquitto_loop_write() → called DIRECTLY after any outgoing data is queued;
 *                            eloop write handler used ONLY during the initial
 *                            connection phase and as a fallback when the kernel
 *                            TCP send buffer is full (rare)
 *   mosquitto_loop_misc()  → called from 1-second eloop timer
 *
 * All mosquitto callbacks (on_connect, on_disconnect, on_message) fire
 * inside mosquitto_loop_read() which runs in the hostapd main thread.
 * No mutex or thread synchronisation is needed.
 *
 * Reconnect flow:
 *   on_disconnect (rc != 0)
 *     → unregister old fd from eloop
 *     → schedule mqtt_reconnect_cb via eloop_register_timeout()
 *   mqtt_reconnect_cb
 *     → mosquitto_reconnect_async()
 *     → re-register new mosquitto_socket() fd with eloop
 *     → on_connect fires → re-subscribe all stored topics
 *
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */

#include "includes.h"

#include <mosquitto.h>

#include "common.h"
#include "eloop.h"
#include "mqtt_eloop.h"

/* ── Compile-time constants ───────────────────────────────────────────── */

/** Maximum number of topic subscriptions stored per client context. */
#define MQTT_ELOOP_MAX_SUBS		32

/** Initial reconnect backoff delay (seconds). */
#define MQTT_RECONNECT_DELAY_INIT	1

/** Maximum reconnect backoff delay (seconds). */
#define MQTT_RECONNECT_DELAY_MAX	30

/**
 * Interval for the periodic misc timer (seconds).
 * mosquitto_loop_misc() must be called at least once per keepalive interval
 * to send PINGREQ. 1 second is safe for any keepalive >= 2 s.
 */
#define MQTT_MISC_INTERVAL_SEC		1

/** Default broker settings used when caller passes 0 / NULL. */
#define MQTT_DEFAULT_HOST		"localhost"
#define MQTT_DEFAULT_PORT		1883
#define MQTT_DEFAULT_KEEPALIVE		60


/* ── Internal data structures ─────────────────────────────────────────── */

/** One stored subscription entry. */
struct mqtt_sub_entry {
	char *topic;	/**< Heap-allocated topic filter string. */
	int   qos;	/**< Requested QoS level (0, 1, or 2).  */
};

/**
 * struct mqtt_eloop_ctx - Full internal context (opaque to callers).
 *
 * The public header forward-declares this struct so callers can hold
 * a pointer without seeing the internals.
 */
struct mqtt_eloop_ctx {
	struct mosquitto *mosq;		/**< libmosquitto instance.          */

	char *broker_host;		/**< Heap-allocated broker hostname. */
	int   broker_port;		/**< Broker TCP port.                */
	int   keepalive;		/**< MQTT keepalive interval (s).    */

	bool  connected;		/**< True after CONNACK received.    */
	bool  shutting_down;		/**< True during deinit/disconnect.  */
	bool  in_mosquitto_cb;		/**< True while inside a mosquitto loop callback.
					 *   Guards mqtt_flush_write() against calling
					 *   mosquitto_loop_write() re-entrantly, which
					 *   can leave mosquitto's socket in a state that
					 *   prevents future loop_read() from processing
					 *   incoming messages. */

	/**
	 * fd currently registered with eloop.
	 * -1 when not registered. Must be updated after every reconnect
	 * because mosquitto_socket() returns a new fd each time.
	 */
	int   current_fd;

	/** Current reconnect backoff delay (seconds). Doubles on each
	 *  failed attempt, capped at MQTT_RECONNECT_DELAY_MAX. */
	int   reconnect_delay;

	/** Stored subscriptions — re-applied on every connect. */
	struct mqtt_sub_entry subs[MQTT_ELOOP_MAX_SUBS];
	int   num_subs;

	/** User-supplied callbacks. */
	mqtt_eloop_msg_cb_t   msg_cb;
	mqtt_eloop_state_cb_t state_cb;
	void                 *userdata;

	/** Message counters. */
	unsigned long tx_count;
	unsigned long rx_count;
};


/* ── Forward declarations ─────────────────────────────────────────────── */

static void mqtt_eloop_read_cb(int sock, void *eloop_ctx, void *sock_ctx);
static void mqtt_eloop_misc_cb(void *eloop_ctx, void *user_ctx);
static void mqtt_reconnect_cb(void *eloop_ctx, void *user_ctx);
static void mqtt_register_fd(struct mqtt_eloop_ctx *ctx);
static void mqtt_unregister_fd(struct mqtt_eloop_ctx *ctx);
static void mqtt_flush_write(struct mqtt_eloop_ctx *ctx);
static void mqtt_resubscribe_all(struct mqtt_eloop_ctx *ctx);


/* ── mosquitto callbacks ──────────────────────────────────────────────── */
/*
 * All of these are called from inside mosquitto_loop_read(), which is
 * invoked from mqtt_eloop_read_cb() — i.e., in the hostapd main thread.
 * It is safe to access all hostapd data structures here.
 */

/**
 * on_connect - Called after CONNACK is received from the broker.
 * @rc: 0 = success; non-zero = refused (see MQTT spec §3.2.2.3).
 *
 * Re-subscribes all stored topics and notifies the caller via state_cb.
 */
static void on_connect(struct mosquitto *mosq, void *userdata, int rc)
{
	struct mqtt_eloop_ctx *ctx = userdata;

	if (rc != 0) {
		wpa_printf(MSG_WARNING,
			   "MQTT: broker refused connection, rc=%d", rc);
		return;
	}

	wpa_printf(MSG_INFO, "MQTT: connected to %s:%d",
		   ctx->broker_host, ctx->broker_port);

	ctx->connected = true;
	ctx->reconnect_delay = MQTT_RECONNECT_DELAY_INIT; /* reset backoff */

	/* Re-subscribe to all stored topic filters. */
	mqtt_resubscribe_all(ctx);

	if (ctx->state_cb)
		ctx->state_cb(true, ctx->userdata);
}

/**
 * on_disconnect - Called when the connection to the broker is lost.
 * @rc: 0 = client called mosquitto_disconnect(); non-zero = unexpected.
 *
 * For unexpected disconnects, unregisters the now-invalid socket fd from
 * eloop and schedules a reconnect attempt via an eloop timer.
 */
static void on_disconnect(struct mosquitto *mosq, void *userdata, int rc)
{
	struct mqtt_eloop_ctx *ctx = userdata;

	ctx->connected = false;

	if (ctx->shutting_down) {
		wpa_printf(MSG_DEBUG, "MQTT: disconnected (shutdown)");
		return;
	}

	wpa_printf(MSG_WARNING,
		   "MQTT: unexpected disconnect from %s:%d, rc=%d",
		   ctx->broker_host, ctx->broker_port, rc);

	/*
	 * The socket fd is now invalid — unregister it from eloop before
	 * mosquitto closes it internally.
	 */
	mqtt_unregister_fd(ctx);

	if (ctx->state_cb)
		ctx->state_cb(false, ctx->userdata);

	/* Schedule first reconnect attempt. */
	wpa_printf(MSG_INFO, "MQTT: reconnect in %d s",
		   ctx->reconnect_delay);
	eloop_register_timeout(ctx->reconnect_delay, 0,
			       mqtt_reconnect_cb, ctx, NULL);
}

/**
 * on_message - Called when a message arrives on a subscribed topic.
 * @msg: Pointer to the received message (valid only during this callback).
 *
 * Increments the receive counter and invokes the user-supplied msg_cb.
 */
static void on_message(struct mosquitto *mosq, void *userdata,
		       const struct mosquitto_message *msg)
{
	struct mqtt_eloop_ctx *ctx = userdata;

	if (!msg || !msg->topic)
		return;

	ctx->rx_count++;

	wpa_printf(MSG_DEBUG, "MQTT: rx topic='%s' payloadlen=%d",
		   msg->topic, msg->payloadlen);

	if (ctx->msg_cb)
		ctx->msg_cb(msg->topic,
			    msg->payload,
			    msg->payloadlen,
			    ctx->userdata);
}

/**
 * on_publish - Called when a QoS 1/2 publish has been acknowledged.
 * @mid: Message ID of the acknowledged publish.
 */
static void on_publish(struct mosquitto *mosq, void *userdata, int mid)
{
	wpa_printf(MSG_DEBUG, "MQTT: publish ack mid=%d", mid);
}

/**
 * on_subscribe - Called when a SUBACK is received from the broker.
 * @mid:         Message ID of the SUBSCRIBE request.
 * @qos_count:   Number of QoS entries in granted_qos[].
 * @granted_qos: Array of granted QoS levels (one per topic filter).
 */
static void on_subscribe(struct mosquitto *mosq, void *userdata, int mid,
			 int qos_count, const int *granted_qos)
{
	wpa_printf(MSG_DEBUG, "MQTT: subscribe ack mid=%d granted_qos=%d",
		   mid, qos_count > 0 ? granted_qos[0] : -1);
}


/* ── eloop callbacks ──────────────────────────────────────────────────── */

/**
 * mqtt_eloop_read_cb - eloop read handler for the mosquitto socket.
 *
 * Called by eloop whenever mosquitto_socket() has data available to read.
 * Drives incoming network data; on_message (and on_connect/on_disconnect
 * during handshake) fire here in the hostapd main thread.
 *
 * After processing reads, immediately flushes any outgoing packets that
 * mosquitto queued in response (e.g., PUBACK for QoS 1, PINGRESP).
 */
static void mqtt_eloop_read_cb(int sock, void *eloop_ctx, void *sock_ctx)
{
	struct mqtt_eloop_ctx *ctx = eloop_ctx;

	/*
	 * Set in_mosquitto_cb so that any mqtt_flush_write() calls triggered
	 * from within the on_connect / on_message / on_disconnect callbacks
	 * (which fire inside mosquitto_loop_read) skip the mosquitto_loop_write()
	 * call. Calling mosquitto_loop_write() re-entrantly inside loop_read()
	 * leaves mosquitto's socket state in a way that prevents subsequent
	 * loop_read() calls from reading incoming messages.
	 */
	ctx->in_mosquitto_cb = true;
	mosquitto_loop_read(ctx->mosq, 1 /* max_packets */);
	ctx->in_mosquitto_cb = false;

	/*
	 * Now safely flush any packets queued by the callbacks above
	 * (SUBSCRIBE from on_connect, PUBLISH from state_cb, PUBACK for
	 * received QoS 1 messages, etc.) — we are outside all callbacks here.
	 */
	mqtt_flush_write(ctx);
}


/**
 * mqtt_eloop_misc_cb - Periodic 1-second eloop timer callback.
 *
 * Calls mosquitto_loop_misc() which handles:
 *   - Sending PINGREQ when the keepalive interval expires
 *   - Checking for PINGRESP timeout (triggers disconnect if missing)
 *   - Retransmitting unacknowledged QoS 1/2 packets
 *   - Internal housekeeping
 *
 * Re-arms itself for the next second unless shutting down.
 */
static void mqtt_eloop_misc_cb(void *eloop_ctx, void *user_ctx)
{
	struct mqtt_eloop_ctx *ctx = eloop_ctx;

	if (ctx->shutting_down)
		return;

	mosquitto_loop_misc(ctx->mosq);

	/*
	 * Immediately flush any packets misc() generated (e.g., PINGREQ,
	 * QoS retransmissions). Direct call avoids one-iteration delay.
	 */
	mqtt_flush_write(ctx);

	/* Re-arm the 1-second timer. */
	eloop_register_timeout(MQTT_MISC_INTERVAL_SEC, 0,
			       mqtt_eloop_misc_cb, ctx, NULL);
}

/**
 * mqtt_reconnect_cb - eloop timer callback that attempts to reconnect.
 *
 * Uses mosquitto_reconnect_async() which reuses the same broker host,
 * port, client ID, and credentials as the original connect call.
 *
 * IMPORTANT: mosquitto_socket() returns a NEW fd after reconnect_async().
 * The old fd was already unregistered in on_disconnect. This function
 * registers the new fd with eloop.
 *
 * On failure, doubles the backoff delay (capped at MQTT_RECONNECT_DELAY_MAX)
 * and schedules another attempt.
 */
static void mqtt_reconnect_cb(void *eloop_ctx, void *user_ctx)
{
	struct mqtt_eloop_ctx *ctx = eloop_ctx;
	int rc;

	if (ctx->shutting_down)
		return;

	wpa_printf(MSG_INFO, "MQTT: attempting reconnect to %s:%d",
		   ctx->broker_host, ctx->broker_port);

	/*
	 * mosquitto_reconnect_async() reuses the original host/port/credentials.
	 * Returns immediately; the TCP handshake is driven by loop_read.
	 */
	rc = mosquitto_reconnect_async(ctx->mosq);
	if (rc == MOSQ_ERR_SUCCESS) {
		/*
		 * The socket fd may have changed — register the new one for reads
		 * and attempt to flush the MQTT CONNECT packet synchronously.
		 * Any unsent data is retried by the periodic misc timer.
		 */
		mqtt_register_fd(ctx);
		mqtt_flush_write(ctx);
		wpa_printf(MSG_DEBUG,
			   "MQTT: reconnect_async initiated, new fd=%d",
			   ctx->current_fd);
	} else {
		/* Exponential backoff: double delay, cap at max. */
		int next_delay = ctx->reconnect_delay * 2;
		if (next_delay > MQTT_RECONNECT_DELAY_MAX)
			next_delay = MQTT_RECONNECT_DELAY_MAX;
		ctx->reconnect_delay = next_delay;

		wpa_printf(MSG_WARNING,
			   "MQTT: reconnect_async failed rc=%d, retry in %d s",
			   rc, ctx->reconnect_delay);
		eloop_register_timeout(ctx->reconnect_delay, 0,
				       mqtt_reconnect_cb, ctx, NULL);
	}
}


/* ── Internal helpers ─────────────────────────────────────────────────── */

/**
 * mqtt_register_fd - Register the current mosquitto socket with eloop.
 *
 * Must be called after mosquitto_connect_async() and after every
 * successful mosquitto_reconnect_async(). Updates ctx->current_fd.
 */
static void mqtt_register_fd(struct mqtt_eloop_ctx *ctx)
{
	int fd = mosquitto_socket(ctx->mosq);

	if (fd < 0) {
		wpa_printf(MSG_ERROR,
			   "MQTT: mosquitto_socket() returned %d (not connected?)",
			   fd);
		return;
	}

	ctx->current_fd = fd;
	eloop_register_read_sock(fd, mqtt_eloop_read_cb, ctx, NULL);
	wpa_printf(MSG_DEBUG, "MQTT: registered fd=%d with eloop", fd);
}

/**
 * mqtt_unregister_fd - Unregister the current mosquitto socket from eloop.
 *
 * Must be called before reconnect because the fd becomes invalid when
 * mosquitto closes the TCP connection on disconnect.
 * Safe to call when current_fd is already -1 (no-op).
 */
static void mqtt_unregister_fd(struct mqtt_eloop_ctx *ctx)
{
	if (ctx->current_fd < 0)
		return;

	eloop_unregister_read_sock(ctx->current_fd);
	wpa_printf(MSG_DEBUG, "MQTT: unregistered fd=%d from eloop",
		   ctx->current_fd);
	ctx->current_fd = -1;
}

/**
 * mqtt_flush_write - Flush mosquitto's outgoing send buffer synchronously.
 *
 * Calls mosquitto_loop_write() directly so outgoing packets (PUBLISH,
 * SUBSCRIBE, PUBACK, PINGREQ, …) are sent to the broker in the same
 * call stack where possible. If the kernel TCP send buffer is full the
 * data stays in mosquitto's internal queue and will be retried on the
 * next call, which happens at most MQTT_MISC_INTERVAL_SEC seconds later
 * via the periodic misc timer.
 *
 * Must NOT be called from within a mosquitto callback (on_connect,
 * on_message, etc.) because calling mosquitto_loop_write() re-entrantly
 * inside mosquitto_loop_read() leaves mosquitto's socket state in a way
 * that prevents subsequent loop_read() calls from processing incoming
 * messages. The in_mosquitto_cb flag prevents this.
 */
static void mqtt_flush_write(struct mqtt_eloop_ctx *ctx)
{
	if (ctx->current_fd < 0)
		return;

	if (ctx->in_mosquitto_cb)
		return; /* defer: mqtt_eloop_read_cb flushes after loop_read returns */

	mosquitto_loop_write(ctx->mosq, 1 /* max_packets */);
}

/**
 * mqtt_resubscribe_all - Queue SUBSCRIBE for every stored topic filter.
 *
 * Called from on_connect() to restore all subscriptions after a connect
 * or reconnect. Only queues the SUBSCRIBE packets; the actual flush to
 * the socket is done by the caller (mqtt_eloop_read_cb) after
 * mosquitto_loop_read() returns, so we never call mosquitto_loop_write()
 * from within a mosquitto callback.
 */
static void mqtt_resubscribe_all(struct mqtt_eloop_ctx *ctx)
{
	int i, rc;

	for (i = 0; i < ctx->num_subs; i++) {
		rc = mosquitto_subscribe(ctx->mosq, NULL,
					 ctx->subs[i].topic,
					 ctx->subs[i].qos);
		if (rc != MOSQ_ERR_SUCCESS) {
			wpa_printf(MSG_WARNING,
				   "MQTT: resubscribe '%s' failed rc=%d",
				   ctx->subs[i].topic, rc);
		} else {
			wpa_printf(MSG_DEBUG,
				   "MQTT: resubscribed to '%s' qos=%d",
				   ctx->subs[i].topic, ctx->subs[i].qos);
		}
	}
}


/* ── Public API implementation ────────────────────────────────────────── */

struct mqtt_eloop_ctx *
mqtt_eloop_init(const char *broker_host,
		int broker_port,
		const char *client_id,
		int keepalive,
		mqtt_eloop_msg_cb_t msg_cb,
		mqtt_eloop_state_cb_t state_cb,
		void *userdata)
{
	struct mqtt_eloop_ctx *ctx;
	int rc;

	ctx = os_zalloc(sizeof(*ctx));
	if (!ctx)
		return NULL;

	ctx->broker_host = os_strdup(broker_host ? broker_host :
				     MQTT_DEFAULT_HOST);
	if (!ctx->broker_host)
		goto fail;

	ctx->broker_port     = broker_port > 0 ? broker_port :
				MQTT_DEFAULT_PORT;
	ctx->keepalive       = keepalive > 0 ? keepalive :
				MQTT_DEFAULT_KEEPALIVE;
	ctx->msg_cb          = msg_cb;
	ctx->state_cb        = state_cb;
	ctx->userdata        = userdata;
	ctx->current_fd      = -1;
	ctx->reconnect_delay = MQTT_RECONNECT_DELAY_INIT;

	/* Initialise the libmosquitto library (reference-counted). */
	rc = mosquitto_lib_init();
	if (rc != MOSQ_ERR_SUCCESS) {
		wpa_printf(MSG_ERROR,
			   "MQTT: mosquitto_lib_init() failed rc=%d", rc);
		goto fail;
	}
	wpa_printf(MSG_DEBUG, "MQTT: mosquitto library initialised");

	/*
	 * Create a new mosquitto client instance.
	 *   client_id     — unique identifier for this client
	 *   clean_session — true: no persistent session on broker
	 *   userdata      — ctx pointer returned in every callback
	 */
	ctx->mosq = mosquitto_new(client_id, true /* clean_session */, ctx);
	if (!ctx->mosq) {
		wpa_printf(MSG_ERROR,
			   "MQTT: mosquitto_new() failed (out of memory?)");
		goto fail;
	}

	/*
	 * Register all mosquitto event callbacks.
	 * These fire inside mosquitto_loop_read() in the main thread.
	 */
	wpa_printf(MSG_DEBUG,
		   "MQTT: client instance created, registering callbacks");
	mosquitto_connect_callback_set  (ctx->mosq, on_connect);
	mosquitto_disconnect_callback_set(ctx->mosq, on_disconnect);
	mosquitto_message_callback_set  (ctx->mosq, on_message);
	mosquitto_publish_callback_set  (ctx->mosq, on_publish);
	mosquitto_subscribe_callback_set(ctx->mosq, on_subscribe);

	wpa_printf(MSG_DEBUG,
		   "MQTT: initialised client_id='%s' broker=%s:%d keepalive=%ds",
		   client_id ? client_id : "(null)",
		   ctx->broker_host, ctx->broker_port, ctx->keepalive);

	return ctx;

fail:
	mqtt_eloop_deinit(ctx);
	return NULL;
}


void mqtt_eloop_deinit(struct mqtt_eloop_ctx *ctx)
{
	int i;

	if (!ctx)
		return;

	wpa_printf(MSG_INFO,
		   "MQTT: deinit broker=%s:%d (tx=%lu rx=%lu)",
		   ctx->broker_host, ctx->broker_port,
		   ctx->tx_count, ctx->rx_count);

	ctx->shutting_down = true;

	/*
	 * Cancel all pending eloop timers before touching the mosquitto
	 * instance, so no callbacks fire after we start tearing down.
	 */
	eloop_cancel_timeout(mqtt_eloop_misc_cb, ctx, NULL);
	eloop_cancel_timeout(mqtt_reconnect_cb,  ctx, NULL);

	/* Unregister the socket fd (read + write) from eloop. */
	mqtt_unregister_fd(ctx);

	if (ctx->mosq) {
		/*
		 * Send MQTT DISCONNECT to the broker (best-effort).
		 * mosquitto_destroy() frees all internal resources.
		 */
		mosquitto_disconnect(ctx->mosq);
		mosquitto_destroy(ctx->mosq);
		ctx->mosq = NULL;
	}

	/*
	 * mosquitto_lib_cleanup() is reference-counted — safe to call even
	 * if multiple clients were initialised.
	 */
	mosquitto_lib_cleanup();

	/* Free stored subscription strings. */
	for (i = 0; i < ctx->num_subs; i++)
		os_free(ctx->subs[i].topic);

	os_free(ctx->broker_host);
	os_free(ctx);
}


int mqtt_eloop_connect(struct mqtt_eloop_ctx *ctx)
{
	int rc;

	if (!ctx || !ctx->mosq)
		return -1;

	wpa_printf(MSG_INFO,
		   "MQTT: connecting to %s:%d (keepalive=%ds)",
		   ctx->broker_host, ctx->broker_port, ctx->keepalive);

	/*
	 * mosquitto_connect_async() returns immediately without blocking.
	 *
	 * Internally, mosquitto creates the TCP socket and initiates the
	 * non-blocking connect(). The TCP handshake and MQTT CONNECT/CONNACK
	 * exchange are completed when mosquitto_loop_read() is called after
	 * the socket becomes readable — i.e., from mqtt_eloop_read_cb().
	 */
	rc = mosquitto_connect_async(ctx->mosq,
				     ctx->broker_host,
				     ctx->broker_port,
				     ctx->keepalive);
	if (rc != MOSQ_ERR_SUCCESS) {
		wpa_printf(MSG_ERROR,
			   "MQTT: mosquitto_connect_async() failed rc=%d (%s), errno=%d (%s)",
			   rc, mosquitto_strerror(rc), errno, strerror(errno));
		return -1;
	}

	/*
	 * Register the socket fd with eloop for read events so that incoming
	 * data (CONNACK, PUBLISH, SUBACK, …) is processed as soon as it arrives.
	 * Write monitoring is not needed: mosquitto's internal state machine
	 * ensures the MQTT CONNECT packet is only sent once the TCP handshake
	 * completes, and mqtt_flush_write() below attempts the send synchronously.
	 * Any unsent data is retried by the periodic misc timer.
	 */
	mqtt_register_fd(ctx);
	mqtt_flush_write(ctx);

	wpa_printf(MSG_DEBUG,
		   "MQTT: connect_async submitted to %s:%d fd=%d, awaiting CONNACK",
		   ctx->broker_host, ctx->broker_port, ctx->current_fd);

	/*
	 * Start the 1-second periodic misc timer.
	 * mosquitto_loop_misc() must be called regularly to send PINGREQ
	 * and handle keepalive timeouts.
	 */
	eloop_register_timeout(MQTT_MISC_INTERVAL_SEC, 0,
			       mqtt_eloop_misc_cb, ctx, NULL);

	return 0;
}


void mqtt_eloop_disconnect(struct mqtt_eloop_ctx *ctx)
{
	if (!ctx)
		return;

	/*
	 * Set shutting_down so on_disconnect does not schedule a reconnect
	 * when it fires in response to mosquitto_disconnect() below.
	 */
	ctx->shutting_down = true;

	/* Cancel pending timers. */
	eloop_cancel_timeout(mqtt_reconnect_cb,  ctx, NULL);
	eloop_cancel_timeout(mqtt_eloop_misc_cb, ctx, NULL);

	/* Unregister socket from eloop. */
	mqtt_unregister_fd(ctx);

	/* Send MQTT DISCONNECT packet to the broker. */
	if (ctx->mosq)
		mosquitto_disconnect(ctx->mosq);

	ctx->connected     = false;
	ctx->shutting_down = false; /* Allow reconnect via mqtt_eloop_connect() */
}


int mqtt_eloop_publish(struct mqtt_eloop_ctx *ctx,
		       const char *topic,
		       const void *payload,
		       int payloadlen,
		       int qos,
		       bool retain)
{
	int rc;

	if (!ctx || !ctx->mosq || !topic)
		return -1;

	if (!ctx->connected) {
		wpa_printf(MSG_DEBUG,
			   "MQTT: publish to '%s' skipped — not connected",
			   topic);
		return -1;
	}

	/*
	 * mosquitto_publish() serialises the PUBLISH packet into mosquitto's
	 * internal send buffer. Immediately flush it to the socket via
	 * mqtt_flush_write() — no eloop iteration delay.
	 */
	wpa_printf(MSG_DEBUG,
		   "MQTT: publish topic='%s' len=%d qos=%d retain=%d tx#=%lu",
		   topic, payloadlen, qos, (int)retain, ctx->tx_count + 1);
	rc = mosquitto_publish(ctx->mosq,
			       NULL,       /* mid — not needed */
			       topic,
			       payloadlen,
			       payload,
			       qos,
			       retain);
	if (rc != MOSQ_ERR_SUCCESS) {
		wpa_printf(MSG_WARNING,
			   "MQTT: mosquitto_publish('%s') failed rc=%d",
			   topic, rc);
		return -1;
	}

	ctx->tx_count++;
	wpa_printf(MSG_DEBUG,
		   "MQTT: publishing to '%s' len=%d qos=%d retain=%d",
		   topic, payloadlen, qos, (int)retain);

	/*
	 * Flush immediately. If the kernel send buffer is full (rare),
	 * mqtt_flush_write() registers the eloop write handler as a fallback.
	 */
	mqtt_flush_write(ctx);

	return 0;
}


int mqtt_eloop_subscribe(struct mqtt_eloop_ctx *ctx,
			 const char *topic,
			 int qos)
{
	int i;
	char *topic_copy;

	if (!ctx || !topic)
		return -1;

	/* Update QoS if topic is already in the table. */
	for (i = 0; i < ctx->num_subs; i++) {
		if (os_strcmp(ctx->subs[i].topic, topic) == 0) {
			ctx->subs[i].qos = qos;
			goto send_subscribe;
		}
	}

	/* Add new entry. */
	if (ctx->num_subs >= MQTT_ELOOP_MAX_SUBS) {
		wpa_printf(MSG_ERROR,
			   "MQTT: subscription table full (max %d)",
			   MQTT_ELOOP_MAX_SUBS);
		return -1;
	}

	topic_copy = os_strdup(topic);
	if (!topic_copy)
		return -1;

	ctx->subs[ctx->num_subs].topic = topic_copy;
	ctx->subs[ctx->num_subs].qos   = qos;
	ctx->num_subs++;
	wpa_printf(MSG_DEBUG,
		   "MQTT: subscription stored '%s' qos=%d (%d/%d slots used)",
		   topic, qos, ctx->num_subs, MQTT_ELOOP_MAX_SUBS);

send_subscribe:
	if (ctx->connected) {
		int rc = mosquitto_subscribe(ctx->mosq, NULL, topic, qos);
		if (rc != MOSQ_ERR_SUCCESS) {
			wpa_printf(MSG_WARNING,
				   "MQTT: subscribe('%s') failed rc=%d",
				   topic, rc);
			return -1;
		}
		mqtt_flush_write(ctx);
		wpa_printf(MSG_DEBUG,
			   "MQTT: subscribed to '%s' qos=%d", topic, qos);
	} else {
		wpa_printf(MSG_DEBUG,
			   "MQTT: queued subscribe '%s' (not yet connected)",
			   topic);
	}

	return 0;
}


int mqtt_eloop_unsubscribe(struct mqtt_eloop_ctx *ctx, const char *topic)
{
	int i, found = -1;

	if (!ctx || !topic)
		return -1;

	for (i = 0; i < ctx->num_subs; i++) {
		if (os_strcmp(ctx->subs[i].topic, topic) == 0) {
			found = i;
			break;
		}
	}

	if (found < 0) {
		wpa_printf(MSG_DEBUG,
			   "MQTT: unsubscribe '%s' — not in table", topic);
		return -1;
	}

	os_free(ctx->subs[found].topic);

	/* Compact the subscription array. */
	for (i = found; i < ctx->num_subs - 1; i++)
		ctx->subs[i] = ctx->subs[i + 1];
	ctx->num_subs--;

	if (ctx->connected) {
		int rc = mosquitto_unsubscribe(ctx->mosq, NULL, topic);
		if (rc != MOSQ_ERR_SUCCESS) {
			wpa_printf(MSG_WARNING,
				   "MQTT: unsubscribe('%s') failed rc=%d",
				   topic, rc);
			return -1;
		}
		mqtt_flush_write(ctx);
		wpa_printf(MSG_DEBUG,
			   "MQTT: unsubscribed from '%s'", topic);
	}

	return 0;
}


bool mqtt_eloop_is_connected(const struct mqtt_eloop_ctx *ctx)
{
	return ctx != NULL && ctx->connected;
}


void mqtt_eloop_get_stats(const struct mqtt_eloop_ctx *ctx,
			  unsigned long *tx_count,
			  unsigned long *rx_count)
{
	if (!ctx)
		return;
	if (tx_count)
		*tx_count = ctx->tx_count;
	if (rx_count)
		*rx_count = ctx->rx_count;
}
