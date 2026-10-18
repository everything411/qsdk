/*
 * hostapd_mqtt.h - hostapd-specific MQTT command/event handling
 *
 * This module owns all hostapd business logic that runs over MQTT: the
 * message and connection-state callbacks registered with mqtt_eloop, the
 * per-feature command dispatchers, and the global-connection lifecycle
 * (start/stop).  mqtt_eloop.c stays a generic mosquitto <-> eloop transport
 * with no knowledge of hostapd data types; this file is where that
 * transport is wired to hostapd's data model.
 *
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef HOSTAPD_MQTT_H
#define HOSTAPD_MQTT_H

#ifdef CONFIG_MQTT

struct hostapd_iface;
struct hapd_interfaces;

/*
 * hostapd_mqtt_init - create the global MQTT connection for this process.
 *
 * @iface: the interface whose config carries mqtt_enabled/mqtt_broker_*;
 *         the resulting connection is stored on iface->interfaces->mqtt_ctx
 *         and shared by every interface and BSS in the process
 *
 * No-op if mqtt_enabled=0 in iface->conf, or if the global connection has
 * already been created (idempotent — safe to call on every interface setup
 * completion).
 */
void hostapd_mqtt_init(struct hostapd_iface *iface);

/*
 * hostapd_mqtt_deinit - tear down the global MQTT connection.
 *
 * @interfaces: global process context holding the connection in mqtt_ctx
 *
 * No-op if mqtt_ctx is NULL.  Sets interfaces->mqtt_ctx back to NULL after
 * freeing it.
 */
void hostapd_mqtt_deinit(struct hapd_interfaces *interfaces);

#endif /* CONFIG_MQTT */
#endif /* HOSTAPD_MQTT_H */
