/*
 * Copyright (c) 2016-2019, 2021, The Linux Foundation. All rights reserved.
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: ISC
 */

/**
 * @defgroup fal_flow
 * @{
 */
#ifndef _FAL_FLOW_H_
#define _FAL_FLOW_H_

#ifdef __cplusplus
extern "C" {
#endif                          /* __cplusplus */

#include "sw.h"
#include "fal_type.h"
#include "fal_ip.h"

#define FAL_VPN_ID_FROM_INTF(vpn_id)	(0x20 | (vpn_id))
#define FAL_VPN_ID_FROM_VSI(vsi)	(0x40 | (vsi))
#define FAL_VPN_ID_FROM_CASCADE(data)	(0x80 | (data))
#define FAL_VPN_ID_FROM_VLAN(vid)	(0x1000 | (vid))
#define FAL_VPN_ID_FROM_VLAN_UNTAG	4095

#define FAL_FLOW_COOKIE_48BIT_LOW		GENMASK_ULL(15, 0)
#define FAL_FLOW_COOKIE_48BIT_MIDDLE		GENMASK_ULL(23, 16)
#define FAL_FLOW_COOKIE_48BIT_HIGH_16BIT_L	GENMASK_ULL(39, 24)
#define FAL_FLOW_COOKIE_48BIT_HIGH_8BIT_H	GENMASK_ULL(47, 40)

#define FAL_FLOW_COOKIE_40BIT_LOW		GENMASK_ULL(15, 0)
#define FAL_FLOW_COOKIE_40BIT_HIGH_16BIT_L	GENMASK_ULL(31, 16)
#define FAL_FLOW_COOKIE_40BIT_HIGH_8BIT_H	GENMASK_ULL(39, 32)

#define FAL_FLOW_COOKIE_16BIT			GENMASK_ULL(15, 0)

#define FAL_FLOW_TREE_ID_24BIT			GENMASK_ULL(23, 0)
#define FAL_FLOW_TREE_ID_24BIT_L		GENMASK_ULL(15, 0)
#define FAL_FLOW_TREE_ID_24BIT_H		GENMASK_ULL(23, 16)


typedef enum {
	FAL_FLOW_L3_UNICAST = 0,
	FAL_FLOW_L2_UNICAST,
	FAL_FLOW_MCAST,
} fal_flow_pkt_type_t;

typedef enum {
	FAL_FLOW_LAN_TO_LAN_DIR = 0,
	FAL_FLOW_LAN_TO_WAN_DIR,
	FAL_FLOW_WAN_TO_LAN_DIR,
	FAL_FLOW_WAN_TO_WAN_DIR,
	FAL_FLOW_UNKOWN_DIR_DIR,
} fal_flow_direction_t;

typedef enum {
	FAL_FLOW_FORWARD = 0,
	FAL_FLOW_SNAT,
	FAL_FLOW_DNAT,
	FAL_FLOW_ROUTE,
	FAL_FLOW_BRIDGE,
	FAL_FLOW_DROP,
	FAL_FLOW_RDT_TO_CPU,
} fal_flow_fwd_type_t;

/* FLOW entry type field */
typedef enum {
	FAL_FLOW_IP4_5TUPLE_ADDR =	0x1,
	FAL_FLOW_IP6_5TUPLE_ADDR =	0x2,
	FAL_FLOW_IP4_3TUPLE_ADDR =	0x4,
	FAL_FLOW_IP6_3TUPLE_ADDR =	0x8,
	FAL_FLOW_IP4_6TUPLE_ADDR =	0x10,
	FAL_FLOW_IP6_6TUPLE_ADDR =	0x20,
} fal_flow_entry_type_t;

typedef enum {
	FAL_FLOW_3TUPLE = 0,
	FAL_FLOW_TCP,
	FAL_FLOW_UDP,
	FAL_FLOW_UDP_LITE,
	FAL_FLOW_IPSEC,
	FAL_FLOW_NATT,
	FAL_FLOW_KEY_GEN0,
	FAL_FLOW_KEY_GEN1,
} fal_flow_protocol_type_t;

#define FAL_FLOW_OP_MODE_KEY         0x0
#define FAL_FLOW_OP_MODE_INDEX     0x1
#define FAL_FLOW_OP_MODE_FLUSH     0x2

#define FAL_FLOW_PROTOCOL_OTHER    0
#define FAL_FLOW_PROTOCOL_TCP	        1
#define FAL_FLOW_PROTOCOL_UDP	  2
#define FAL_FLOW_PROTOCOL_UDPLITE  3

#define FAL_FLOW_PKT_CNT_MASK	0xFFFFFFFFULL
#define FAL_FLOW_BYTE_CNT_MASK	0xFFFFFFFFFFULL

#define FAL_FLOW_TREE_ID_MAX		3
#define FAL_FLOW_COOKIE_16B_MAX		2
#define FAL_FLOW_COOKIE_40B_MAX		5
#define FAL_FLOW_COOKIE_48B_MAX		6

typedef enum {
	FAL_FLOW_QOS_TYPE_TREE_ID = 0,
	FAL_FLOW_QOS_TYPE_COOKIE_16B,
	FAL_FLOW_QOS_TYPE_COOKIE_40B,
	FAL_FLOW_QOS_TYPE_COOKIE_48B,
} fal_flow_cookie_type_t;

typedef struct {
	fal_flow_cookie_type_t type; /* added for ipq53xx */
	a_uint8_t qos;	/* added for ipq95xx */
	a_bool_t qos_valid; /* added for ipq95xx */
	union {
		a_uint8_t tree_id[FAL_FLOW_TREE_ID_MAX];
		a_uint8_t cookie_16b[FAL_FLOW_COOKIE_16B_MAX];
		a_uint8_t cookie_40b[FAL_FLOW_COOKIE_40B_MAX];
		a_uint8_t cookie_48b[FAL_FLOW_COOKIE_48B_MAX];
	};
} fal_flow_qos_t;

typedef struct {
	fal_fwd_cmd_t miss_action; /* flow mismatch action*/
	a_bool_t frag_bypass_en; /*0 for disable and 1 for enable*/
	a_bool_t tcp_spec_bypass_en; /*0 for disable and 1 for enable*/
	a_bool_t all_bypass_en; /*0 for disable and 1 for enable*/
	a_uint8_t key_sel; /*0 for source ip address and 1 for destination ip address*/
} fal_flow_mgmt_t;

typedef struct {
	a_uint32_t entry_id; /*entry index*/ 
	fal_flow_entry_type_t entry_type; /*1:ipv4 5 tuple, 2:ipv6 5 tuple, 4:ipv4 3 tuple, 8:ipv6 3 tuple*/
	a_uint8_t host_addr_type; /*0:source ip index, 1:destination ip index*/
	a_uint16_t host_addr_index; /*host table entry index*/
	fal_flow_protocol_type_t protocol; /*1:tcp, 2:udp, 3:udp-lite, 0:other*/
	a_uint8_t age; /*aging value*/
	a_bool_t src_intf_valid; /*source interface check valid*/
	a_uint8_t src_intf_index; /*source l3 interface*/
	fal_flow_fwd_type_t fwd_type; /*forward type*/
	a_uint16_t snat_nexthop; /*nexthop index for snat*/
	a_uint16_t snat_srcport; /*new source l4 port*/
	a_uint16_t dnat_nexthop; /*nexthop index for dnat*/
	a_uint16_t dnat_dstport; /*new destination l4 port*/
	a_uint16_t route_nexthop; /*nexthop index for route*/
	a_bool_t port_valid; /*route port valid*/
	fal_port_t route_port; /*port for route*/
	fal_port_t bridge_port; /*port for l2 bridge*/
	a_bool_t deacclr_en; /*0 for disable and 1 for enable*/
	a_bool_t copy_tocpu_en; /*0 for disable and 1 for enable*/
	a_uint8_t syn_toggle; /*update by host*/
	a_uint8_t pri_profile; /*flow qos index*/
	a_uint8_t sevice_code; /*service code for bypass*/
	a_uint8_t ip_type; /*0 for ipv4 and 1 for ipv6*/
	union {
		fal_ip4_addr_t ipv4;
		fal_ip6_addr_t ipv6;
	} flow_ip;
	a_uint16_t src_port; /*l4 source port*/
	a_uint16_t dst_port; /*l4 destination port*/
	a_uint32_t pkt_counter; /*flow packet counter*/
	a_uint64_t byte_counter; /*flow byte counter*/
	a_bool_t pmtu_check_l3; /* compare pmtu with paket length of L3 or L2, added for ipq95xx */
	a_uint32_t pmtu; /*path mtu length, added for ipq95xx */
	a_uint32_t vpn_id; /*vpn id, added for ipq95xx */
	a_bool_t vlan_fmt_valid; /*egress with vlan format of bridge, added for ipq95xx */
	a_bool_t svlan_fmt; /*egress with svlan tag of bridge, added for ipq95xx */
	a_bool_t cvlan_fmt; /*egress with cvlan tag of bridge, added for ipq95xx */
	fal_flow_qos_t flow_qos; /* egress with wifi qos, wifi tree id or flow cookie */
	a_bool_t bridge_nexthop_valid; /*bridge nexthop valid, added for ipq53xx */
	a_uint16_t bridge_nexthop; /*nexthop index for bridge, added for ipq53xx */
	a_bool_t invalid; /* added for host data path */
	a_bool_t policer_valid; /* flow based policer valid or not, added for ipq53xx */
	a_uint32_t policer_index; /* flow based policer index, added for ipq53xx */
	a_bool_t route_en; /* routing or bridge. */
	a_bool_t udf0_en; /* flow key gen0 enabled or not. */
	a_bool_t udf1_en; /* flow key gen1 enabled or not. */
	a_uint32_t match_more; /* SPI value for NATT, UDF0+UDF1 for flow key sel. */
	a_uint32_t sampling_id; /* STC index. */
	a_uint32_t nat_action; /* NAT action for routing. */
	a_bool_t gro_en; /* GRO enabled or not for ipq96xx. */
} fal_flow_entry_t;

enum {
	FAL_FLOW_KEY_ROUTING = BIT(0),
	FAL_FLOW_KEY_IPSEC_AH = BIT(1),
	FAL_FLOW_KEY_IPSEC_ESP = BIT(2),
	FAL_FLOW_KEY_IPSEC_NATT = BIT(3),
};

typedef struct {
	fal_fwd_cmd_t src_if_check_action; /*source inferface check fail action*/
	a_bool_t src_if_check_deacclr_en; /*0 for disable and 1 for enable*/
	a_bool_t service_loop_en; /*0 for disable and 1 for enable*/
	fal_fwd_cmd_t service_loop_action; /*0 for disable and 1 for enable*/
	a_bool_t service_loop_deacclr_en; /*0 for disable and 1 for enable*/ 
	fal_fwd_cmd_t flow_deacclr_action; /*flow de acceleration action*/
	fal_fwd_cmd_t sync_mismatch_action; /*sync toggle mismatch action*/
	a_bool_t sync_mismatch_deacclr_en; /*0 for disable and 1 for enable*/
	a_uint8_t hash_mode_0; /*0 crc10, 1 xor, 2 crc16*/
	a_uint8_t hash_mode_1; /*0 crc10, 1 xor, 2 crc16*/
	a_bool_t flow_mismatch_copy_escape_en; /*0 for disable and 1 for enable*/
	fal_fwd_cmd_t ptmu_fail_action; /* mtu check fail action
					 * added for ipq95xx */
	a_bool_t ptmu_fail_deacclr_en; /* mtu check fail deaccelerate or not
					     * added for ipq95xx */
	fal_fwd_cmd_t ptmu_fail_df_action; /* mtu check fail and
					    * fragmentaion not allowed action
					    * added for ipq95xx */
	a_bool_t ptmu_fail_df_deacclr_en; /* mtu check fail and
					     * dont fragment not allowed deaccelerate or not
					     * added for ipq95xx */
	a_bool_t l2_vpn_en; /* enable vsi as flow key or not
			     * added for ipq95xx */
	a_bool_t l3_vpn_en; /* enable vrf(vpn_id configured by ip globalctrl) as flow key or not
			     * added for ipq95xx */
	a_uint8_t flow_cookie_pri; /* flow cookie priority compared with IPO, added for ipq53xx */
	a_uint8_t flow_key_en_bitmap; /* Enable flow key for routing, ipsec_ah, ipsec_esp and ipsec_natt. */
} fal_flow_global_cfg_t;

typedef struct {
	fal_flow_entry_t flow_entry;
	fal_host_entry_t host_entry;
} fal_flow_host_entry_t;

typedef struct {
	a_uint16_t age_time; /* age value*/
	a_uint16_t unit; /*0:second 1:cycle 2:million cycle*/
} fal_flow_age_timer_t;

typedef struct {
/*input of fal_flow_npt66_iid_cal to calculate*/
	fal_ip6_addr_t 	sip; 		//source ip, used when is_dnat is false
	fal_ip6_addr_t 	dip;	 	//destination ip, used when is_dnat is true
	fal_ip6_addr_t 	tip; 		//translated ip
	a_uint32_t 		prefix_len;	//ipv6 prefix length
	a_uint32_t 		tip_prefix_len;	//ipv6 tip prefix length
	a_bool_t 		is_dnat;	//is dnat or not
} fal_flow_npt66_iid_calc_t;

typedef struct {
/*result of fal_flow_npt66_iid_cal*/
	a_uint32_t iid_offset;
	a_uint32_t iid;
	a_bool_t   is_dnat;
} fal_flow_npt66_iid_t;

enum {
	FAL_FLOW_KEY_L3_TYPE,
	FAL_FLOW_KEY_L4_TYPE,
	FAL_FLOW_KEY_APP_TYPE,
	FAL_FLOW_KEY_SIP,
	FAL_FLOW_KEY_DIP,
	FAL_FLOW_KEY_IP_PROTOCOL,
	FAL_FLOW_KEY_SPORT,
	FAL_FLOW_KEY_DPORT,
	FAL_FLOW_KEY_UDF0,
	FAL_FLOW_KEY_UDF1,
};

typedef struct {
	/* Valid or not. */
	a_bool_t valid;
	/* flow key including l3 type, l4 type, app type, sip, dip,
	 * ip protocol, sport, dport, udf0 and udf1.
	 */
	a_uint16_t key_bmp;
	/* 0x1=IPv4; 0x2=ARP; 0x3=IPv6. */
	a_uint16_t l3_type;
	/* 0x1=TCP; 0x2=UDP; 0x3=UDP-Lite; 0x4=ICMP. */
	a_uint16_t l4_type;
	/* APP type from the mapped tunnel type. */
	a_uint16_t app_type;
	/* UDF0 id used to select one UDF from total four UDFs. */
	a_uint8_t udf0_idx;
	/* UDF0 mask for udf0. */
	a_uint16_t udf0_mask;
	/* UDF1 id used to select one UDF from total four UDFs. */
	a_uint8_t udf1_idx;
	/* UDF1 mask for udf1. */
	a_uint16_t udf1_mask;
} fal_flow_key_t;

/* APP entry IP version */
enum {
	FAL_FLOW_IP_VER_V4 = 1,
	FAL_FLOW_IP_VER_V6,
	FAL_FLOW_IP_VER_V4_OR_V6,
};

/* APP entry UDP type */
enum {
	FAL_FLOW_L4_TYPE_UDP = 1,
	FAL_FLOW_L4_TYPE_UDP_LITE,
	FAL_FLOW_L4_TYPE_UDP_OR_UDP_LITE,
};

/* APP entry port type */
enum {
	FAL_FLOW_L4_PORT_TYPE_DST = 1,
	FAL_FLOW_L4_PORT_TYPE_SRC,
	FAL_FLOW_L4_PORT_TYPE_DST_OR_SRC,
};

/* APP entry */
typedef struct {
	a_uint8_t ip_ver;        /* 1 ipv4, 2 ipv6, 3 ipv4 or ipv6 */
	a_uint8_t udp_type;      /* 1 udp, 2 udp-lite, 3 udp or udp-lite*/
	a_uint8_t l4_port_type;  /* 1 dst port, 2 src port, 3 dst or src port*/
	a_uint16_t l4_port;      /* l4 port value*/
} fal_flow_app_entry_t;

typedef enum {
	FAL_FLOW_EIP_LOOKUP_MODE_TRANSFORM = 0,
	FAL_FLOW_EIP_LOOKUP_MODE_FLOW,
} fal_flow_eip_lookup_mode_t;

sw_error_t fal_flow_key_set(a_uint32_t dev_id, fal_flow_protocol_type_t key_type,
		fal_flow_key_t *flow_key);

sw_error_t fal_flow_key_get(a_uint32_t dev_id, fal_flow_protocol_type_t key_type,
		fal_flow_key_t *flow_key);

sw_error_t fal_flow_sampling_id_get(a_uint32_t dev_id, a_uint32_t flow_index,
		a_uint32_t *sampling_id);

sw_error_t fal_flow_sampling_id_set(a_uint32_t dev_id, a_uint32_t flow_index,
		a_uint32_t sampling_id);

sw_error_t fal_flow_gro_en_get(a_uint32_t dev_id, a_uint32_t flow_index,
		a_bool_t *enable);

sw_error_t fal_flow_gro_en_set(a_uint32_t dev_id, a_uint32_t flow_index,
		a_bool_t enable);

sw_error_t
fal_flow_counter_get(a_uint32_t dev_id, a_uint32_t flow_index, fal_entry_counter_t *flow_counter);

sw_error_t
fal_flow_counter_cleanup(a_uint32_t dev_id, a_uint32_t flow_index);

sw_error_t
fal_flow_entry_en_set(a_uint32_t dev_id, a_uint32_t flow_index, a_bool_t enable);

sw_error_t
fal_flow_entry_en_get(a_uint32_t dev_id, a_uint32_t flow_index, a_bool_t *enable);

sw_error_t
fal_flow_qos_set(a_uint32_t dev_id, a_uint32_t flow_index, fal_flow_qos_t *flow_qos);

sw_error_t
fal_flow_qos_get(a_uint32_t dev_id, a_uint32_t flow_index, fal_flow_qos_t *flow_qos);

sw_error_t
fal_flow_status_set(a_uint32_t dev_id, a_bool_t enable);

sw_error_t
fal_flow_status_get(a_uint32_t dev_id, a_bool_t *enable);

#if !defined(IN_FLOW_MINI)
sw_error_t
fal_flow_age_timer_set(a_uint32_t dev_id, fal_flow_age_timer_t *age_timer);

sw_error_t
fal_flow_age_timer_get(a_uint32_t dev_id, fal_flow_age_timer_t *age_timer);
#endif

sw_error_t
fal_flow_mgmt_set(
		a_uint32_t dev_id,
		fal_flow_pkt_type_t type,
		fal_flow_direction_t dir,
		fal_flow_mgmt_t *mgmt);

sw_error_t
fal_flow_mgmt_get(
		a_uint32_t dev_id,
		fal_flow_pkt_type_t type,
		fal_flow_direction_t dir,
		fal_flow_mgmt_t *mgmt);

sw_error_t
fal_flow_entry_add(
		a_uint32_t dev_id,
		a_uint32_t add_mode, /*index or hash*/
		fal_flow_entry_t *flow_entry);

sw_error_t
fal_flow_entry_del(
		a_uint32_t dev_id,
		a_uint32_t del_mode,
		fal_flow_entry_t *flow_entry);

sw_error_t
fal_flow_entry_get(
		a_uint32_t dev_id,
		a_uint32_t get_mode,
		fal_flow_entry_t *flow_entry);

sw_error_t
fal_flow_entry_next(
		a_uint32_t dev_id,
		a_uint32_t next_mode,
		fal_flow_entry_t *flow_entry);

sw_error_t
fal_flow_host_add(
		a_uint32_t dev_id,
		a_uint32_t add_mode,
		fal_flow_host_entry_t *flow_host_entry);

sw_error_t
fal_flow_host_del(
		a_uint32_t dev_id,
		a_uint32_t del_mode,
		fal_flow_host_entry_t *flow_host_entry);

sw_error_t
fal_flow_host_get(
		a_uint32_t dev_id,
		a_uint32_t get_mode,
		fal_flow_host_entry_t *flow_host_entry);

sw_error_t
fal_flow_global_cfg_get(
		a_uint32_t dev_id,
		fal_flow_global_cfg_t *cfg);

sw_error_t
fal_flow_global_cfg_set(
		a_uint32_t dev_id,
		fal_flow_global_cfg_t *cfg);

sw_error_t
fal_flow_npt66_prefix_add(a_uint32_t dev_id, a_uint32_t l3_if_index, fal_ip6_addr_t *ip6, a_uint32_t prefix_len);
sw_error_t
fal_flow_npt66_prefix_get(a_uint32_t dev_id, a_uint32_t l3_if_index, fal_ip6_addr_t *ip6, a_uint32_t *prefix_len);
sw_error_t
fal_flow_npt66_prefix_del(a_uint32_t dev_id, a_uint32_t l3_if_index);
sw_error_t
fal_flow_npt66_iid_cal(a_uint32_t dev_id, fal_flow_npt66_iid_calc_t *iid_cal, fal_flow_npt66_iid_t *iid_result);
sw_error_t
fal_flow_npt66_iid_add(a_uint32_t dev_id, a_uint32_t flow_index, fal_flow_npt66_iid_t *iid_entry);
sw_error_t
fal_flow_npt66_iid_get(a_uint32_t dev_id, a_uint32_t flow_index, fal_flow_npt66_iid_t *iid_entry);
sw_error_t
fal_flow_npt66_iid_del(a_uint32_t dev_id, a_uint32_t flow_index);
sw_error_t
fal_flow_npt66_status_get(a_uint32_t dev_id, a_bool_t *enable);
sw_error_t
fal_flow_npt66_status_set(a_uint32_t dev_id, a_bool_t enable);

sw_error_t
fal_flow_eip_lookup_mode_set(a_uint32_t dev_id, fal_flow_eip_lookup_mode_t mode);

sw_error_t
fal_flow_eip_lookup_mode_get(a_uint32_t dev_id, fal_flow_eip_lookup_mode_t *mode);

sw_error_t
fal_flow_fwd_type_set(a_uint32_t dev_id, a_uint32_t flow_index, fal_flow_fwd_type_t fwd_type);

sw_error_t
fal_flow_fwd_type_get(a_uint32_t dev_id, a_uint32_t flow_index, fal_flow_fwd_type_t *fwd_type);

sw_error_t fal_flow_app_entry_add(a_uint32_t dev_id, fal_flow_app_entry_t *entry);
sw_error_t fal_flow_app_entry_get(a_uint32_t dev_id, fal_flow_app_entry_t *entry);
sw_error_t fal_flow_app_entry_del(a_uint32_t dev_id, fal_flow_app_entry_t *entry);
#ifdef __cplusplus
}
#endif                          /* __cplusplus */
#endif                          /* _FAL_FLOW_H_ */

/**
 * @}
 */

