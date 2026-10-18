/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef HOSTAPD_IF_MQTT_H
#define HOSTAPD_IF_MQTT_H

#include "hostapd_if_common.h"

struct hapd_interfaces;
struct mqtt_tlv_message;

#ifdef CONFIG_QCN_EXTN

void hostapd_mqtt_hif_cmd(struct hapd_interfaces *interfaces,
			  uint16_t msg_type,
			  struct mqtt_tlv_message *msg);
void hostapd_if_mqtt_init(void *arg);

#ifdef CONFIG_MQTT_TEST_APP_FORK
int hostapd_if_start_mqtt_hif_client(void);
void hostapd_if_stop_mqtt_hif_client(void);
#endif /* CONFIG_MQTT_TEST_APP_FORK */

#else /* CONFIG_QCN_EXTN */

static inline void
hostapd_mqtt_hif_cmd(struct hapd_interfaces *interfaces,
		     uint16_t msg_type,
		     struct mqtt_tlv_message *msg) {}
static inline void
hostapd_if_mqtt_init(void *arg) {}
static inline int hostapd_if_start_mqtt_hif_client(void) { return 0; }
static inline void hostapd_if_stop_mqtt_hif_client(void) {}

#endif /* CONFIG_QCN_EXTN */

#endif /* HOSTAPD_IF_MQTT_H */
