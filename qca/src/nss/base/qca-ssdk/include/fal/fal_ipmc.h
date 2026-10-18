/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: ISC
 */

/**
 * @defgroup fal_ipmc FAL_IPMC
 * @{
 */
#ifndef _FAL_IPMC_H_
#define _FAL_IPMC_H_

#ifdef __cplusplus
extern "C" {
#endif                          /* __cplusplus */

#include "sw.h"
#include "fal_type.h"

typedef struct {
	a_bool_t mc_dmac_check_en;
	fal_vlan_match_mode_t vlan_mode;  /* 0 vid, 1 vsi */
	fal_fwd_cmd_t mismatch_action;
	a_uint8_t hash_mode[2]; /* 0 CRC10, 1 XOR, 2 CRC16 */
} fal_ipmc_global_cfg_t;

typedef enum {
	FAL_IPMC_OP_MODE_HASH = 0,
	FAL_IPMC_OP_MODE_INDEX = 1,
	FAL_IPMC_OP_MODE_MAX,
} fal_ipmc_op_mode_t;

typedef enum {
	FAL_IPMC_OP_TYPE_ADD = 0,
	FAL_IPMC_OP_TYPE_DEL = 1,
	FAL_IPMC_OP_TYPE_GET = 2,
	FAL_IPMC_OP_TYPE_FLUSH = 3,
} fal_ipmc_op_type_t;

typedef enum {
	FAL_IPMC_OP_RSLT_OK = 0,
	FAL_IPMC_OP_RSLT_FAIL = 1,
} fal_ipmc_op_rslt_t;

typedef enum {
	FAL_IPMC_NEXT_MODE_GIP = 0, /* next gipv4 only entry */
	FAL_IPMC_NEXT_MODE_SIP_GIP = 1, /* next sgipv4 entry */
	FAL_IPMC_NEXT_MODE_GIPV6 = 2 , /* next gipv6 only entry */
	FAL_IPMC_NEXT_MODE_SIPV6_GIPV6 = 3, /* next sgipv6 entry */
	FAL_IPMC_NEXT_MODE_ALL = 4, /* next all entry */
} fal_ipmc_next_mode_t;

typedef enum {
	FAL_IPMC_KEY_TYPE_GIP = 0,
	FAL_IPMC_KEY_TYPE_SIP_GIP = 1,
	FAL_IPMC_KEY_TYPE_GIPV6 = 2,
	FAL_IPMC_KEY_TYPE_SIPV6_GIPV6 = 3,
} fal_ipmc_key_type_t;

typedef struct {
	a_uint32_t entry_id; /* entry index */
	fal_ipmc_key_type_t key_type; /* 0 GIP, 1 SIP+GIP, 2 GIPv6, 3 SIPv6+GIPV6 */
	a_bool_t vlan_valid;
	fal_vlan_match_mode_t vlan_mode; /* 0 vid, 1 vsi */
	a_uint16_t vlan_id; /* vid or vsi value */
	union {
		fal_ip4_addr_t ip4_addr;
		fal_ip6_addr_t ip6_addr;
	} sip; /* matched src ip */
	union {
		fal_ip4_addr_t ip4_addr;
		fal_ip6_addr_t ip6_addr;
	} gip; /* matched group ip */

	a_bool_t src_port_check_en; /* enable src port check or not */
	fal_port_t  src_port; /* src port, port or gem port; used for src port check */
	a_bool_t vlan_fmt_check_en;  /* enable vlan tag check or not */
	a_uint8_t vlan_fmt; /*0 = untagged; 1 = tagged or priority tag; used for VLAN check */
	fal_fwd_cmd_t fwd_cmd; /*forward type*/
	fal_dest_info_t dst_info;
	a_uint8_t syn_toggle; /* syn toggle */
	a_bool_t ucast_fwd_en;
} fal_ipmc_entry_t;

typedef struct {
	a_uint8_t ucast_fwd_en_port; /* enable unicast forward for multicast traffic to this port */
	a_uint8_t dest_vp; /* converted port vp and enqueue vp*/
	a_uint8_t service_code; /* service code */
	a_uint32_t bypass_bitmap; /* l2 post bypass bitmap */
} fal_ipmc_ucast_fwd_t;

sw_error_t fal_ipmc_status_set(a_uint32_t dev_id, a_bool_t enable);
sw_error_t fal_ipmc_status_get(a_uint32_t dev_id, a_bool_t *enable);
sw_error_t fal_ipmc_global_cfg_set(a_uint32_t dev_id, fal_ipmc_global_cfg_t *cfg);
sw_error_t fal_ipmc_global_cfg_get(a_uint32_t dev_id, fal_ipmc_global_cfg_t *cfg);
sw_error_t fal_ipmc_entry_add(a_uint32_t dev_id, fal_ipmc_op_mode_t add_mode,
		fal_ipmc_entry_t *ipmc_entry);
sw_error_t fal_ipmc_entry_del(a_uint32_t dev_id, fal_ipmc_op_mode_t del_mode,
		fal_ipmc_entry_t *ipmc_entry);
sw_error_t fal_ipmc_entry_get(a_uint32_t dev_id, fal_ipmc_op_mode_t get_mode,
		fal_ipmc_entry_t *ipmc_entry);
sw_error_t fal_ipmc_entry_getnext(a_uint32_t dev_id, fal_ipmc_next_mode_t next_mode,
		fal_ipmc_entry_t *ipmc_entry);
sw_error_t fal_ipmc_entry_flush(a_uint32_t dev_id);
sw_error_t fal_ipmc_ucast_fwd_set(a_uint32_t dev_id, fal_ipmc_ucast_fwd_t *ucast_fwd);
sw_error_t fal_ipmc_ucast_fwd_get(a_uint32_t dev_id, fal_ipmc_ucast_fwd_t *ucast_fwd);
#ifdef __cplusplus
}
#endif                          /* __cplusplus */
#endif                          /* _FAL_IPMC_H_ */
/**
 * @}
 */
