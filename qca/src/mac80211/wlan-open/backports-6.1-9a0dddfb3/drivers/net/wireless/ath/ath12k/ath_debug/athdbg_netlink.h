/* SPDX-License-Identifier: BSD-3-Clause-Clear */
/* Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries. */
#ifndef ATHDBG_NETLINK_H
#define ATHDBG_NETLINK_H

#include "../core.h"
#include "../athdbg_if.h"

/* Generic Netlink family constants - shared between kernel and userspace */
#define WLAN_FW_SS_HANDLER_FAMILY_NAME   "WLAN_FW_SS_HDL"
#define WLAN_FW_SS_HANDLER_GENL_VERSION        1

/* Commands */
enum nl_fw_anomaly_genl_cmd {
	NL_FW_ANAMOLY_CMD_UNSPEC,
	NL_FW_ANAMOLY_CMD_REGISTER,   /* userspace -> kernel: register portid */
	NL_FW_ANAMOLY_CMD_WMI_EVENT,  /* kernel -> userspace: WMI TLV event  */
	__NL_FW_ANAMOLY_CMD_MAX,
};

/* Attributes */
enum nl_fw_anomaly_genl_attr {
	NL_FW_ANAMOLY_ATTR_UNSPEC,
	NL_FW_ANAMOLY_ATTR_EVENT_ID,  /* NLA_U32:    WMI event ID */
	NL_FW_ANAMOLY_ATTR_WMI_TLV,   /* NLA_BINARY: raw TLV bytes */
	NL_FW_ANAMOLY_ATTR_HW_LINK_ID,
	NL_FW_ANAMOLY_ATTR_RADIO_IDX,
	__NL_FW_ANAMOLY_ATTR_MAX,
};

int  athdbg_netlink_init(void);
void athdbg_netlink_exit(void);
void athdbg_netlink_send(struct ath12k_base *ab, u32 event_id,
			 const struct athdbg_wmi_event_info *info);

#endif /* ATHDBG_NETLINK_H */
