/*
 * mqtt_eloop.h - libmosquitto direct eloop integration for hostapd
 *
 * Integrates libmosquitto with hostapd's eloop:
 * no background thread — mosquitto_socket() fd is registered directly
 * with eloop_register_read_sock(), and mosquitto_loop_read/write/misc()
 * are called from eloop callbacks.
 *
 * Threading model: ALL callbacks (on_message, on_connect, on_disconnect)
 * run in the hostapd main thread, called from mosquitto_loop_read() inside
 * the eloop read handler. No mutex or synchronization is required.
 *
 * Reconnect: on unexpected disconnect, the old socket fd is unregistered
 * from eloop, and mosquitto_reconnect_async() is retried via an eloop
 * timer with exponential backoff. The new fd returned by
 * mosquitto_socket() after reconnect is re-registered automatically.
 *
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef MQTT_ELOOP_H
#define MQTT_ELOOP_H

#include <stdbool.h>
#include <stddef.h>

/**
 * struct mqtt_eloop_ctx - Opaque MQTT eloop client context.
 *
 * Callers hold a pointer to this structure. The internals are defined
 * only in mqtt_eloop.c and must not be accessed directly.
 */
struct mqtt_eloop_ctx;

typedef void (*mqtt_eloop_msg_cb_t)(const char *topic, const void *payload,
				    int payloadlen, void *userdata);
typedef void (*mqtt_eloop_state_cb_t)(bool connected, void *userdata);

/**
 * mqtt_eloop_init - Allocate and configure a new MQTT eloop client.
 * @broker_host: MQTT broker hostname or IP address (NULL → "localhost")
 * @broker_port: MQTT broker TCP port (0 → 1883)
 * @client_id:   MQTT client identifier string (must be unique per broker)
 * @keepalive:   Keepalive interval in seconds (0 → 60)
 * @msg_cb:      Called for each received message (may be NULL)
 *               @topic:      MQTT topic string (NUL-terminated, valid only
 *                            during the callback)
 *               @payload:    Raw message payload bytes (may be NULL for
 *                            zero-length)
 *               @payloadlen: Length of payload in bytes
 *               @userdata:   Opaque pointer supplied here
 *
 *               Runs directly in the hostapd main thread, called from
 *               mosquitto_loop_read() inside the eloop read handler. It is
 *               safe to access all hostapd data structures from here
 *               without any locking. The payload pointer is only valid for
 *               the duration of this callback — copy it if you need to
 *               retain it.
 * @state_cb:    Called on connect/disconnect events (may be NULL)
 *               @connected: true  = CONNACK received, broker connection is up
 *                           false = disconnected (unexpected or graceful)
 *               @userdata:  Opaque pointer supplied here
 *
 *               Runs in the hostapd main thread. When connected=true, all
 *               subscriptions stored via mqtt_eloop_subscribe() have
 *               already been re-sent to the broker.
 * @userdata:    Opaque pointer passed back to both callbacks unchanged
 *
 * Initialises libmosquitto and creates the mosquitto instance. Does NOT
 * connect to the broker — call mqtt_eloop_connect() after this.
 *
 * Returns: Allocated context pointer on success, NULL on failure.
 */
struct mqtt_eloop_ctx *
mqtt_eloop_init(const char *broker_host,
		int broker_port,
		const char *client_id,
		int keepalive,
		mqtt_eloop_msg_cb_t msg_cb,
		mqtt_eloop_state_cb_t state_cb,
		void *userdata);

/**
 * mqtt_eloop_deinit - Disconnect, unregister from eloop, and free all resources.
 * @ctx: Context returned by mqtt_eloop_init() (may be NULL — no-op)
 *
 * Cancels all pending eloop timers, unregisters the mosquitto socket,
 * sends MQTT DISCONNECT, destroys the mosquitto instance, and frees ctx.
 * After this call, ctx is invalid and must not be used.
 */
void mqtt_eloop_deinit(struct mqtt_eloop_ctx *ctx);

/**
 * mqtt_eloop_connect - Initiate an asynchronous connection to the broker.
 * @ctx: Context returned by mqtt_eloop_init()
 *
 * Calls mosquitto_connect_async() which returns immediately without
 * blocking. The TCP handshake and MQTT CONNECT/CONNACK exchange are
 * driven by subsequent mosquitto_loop_read() calls from the eloop read
 * handler. Registers mosquitto_socket() with eloop_register_read_sock()
 * and starts the 1-second misc timer for keepalive/PINGREQ.
 *
 * Returns: 0 on success, -1 on failure.
 */
int mqtt_eloop_connect(struct mqtt_eloop_ctx *ctx);

/**
 * mqtt_eloop_publish - Publish a message to a topic.
 * @ctx:        Context returned by mqtt_eloop_init()
 * @topic:      MQTT topic string (NUL-terminated)
 * @payload:    Payload bytes (may be NULL for zero-length payload)
 * @payloadlen: Length of payload in bytes
 * @qos:        QoS level: 0 (fire-and-forget), 1 (at-least-once),
 *              2 (exactly-once)
 * @retain:     true = broker retains last message for new subscribers
 *
 * Fails immediately if not connected. For QoS > 0, mosquitto buffers
 * the packet internally and the write handler is armed automatically
 * to flush it via mosquitto_loop_write().
 *
 * Returns: 0 on success, -1 on failure (not connected or mosquitto error).
 */
int mqtt_eloop_publish(struct mqtt_eloop_ctx *ctx,
		       const char *topic,
		       const void *payload,
		       int payloadlen,
		       int qos,
		       bool retain);

/**
 * mqtt_eloop_subscribe - Subscribe to a topic filter.
 * @ctx:   Context returned by mqtt_eloop_init()
 * @topic: Topic filter string (wildcards + and # supported)
 * @qos:   Maximum QoS level for delivered messages (0, 1, or 2)
 *
 * The subscription is stored in an internal table and automatically
 * re-sent to the broker after every reconnect. Safe to call before
 * mqtt_eloop_connect() — the SUBSCRIBE packet will be sent once
 * CONNACK is received.
 *
 * Returns: 0 on success, -1 on failure (table full or mosquitto error).
 */
int mqtt_eloop_subscribe(struct mqtt_eloop_ctx *ctx,
			 const char *topic,
			 int qos);

#endif /* MQTT_ELOOP_H */
