/*
 * Copyright (c) 2016-2017, 2019, 2021, The Linux Foundation. All rights reserved.
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: ISC
 */

/**
 * @defgroup
 * @{
 */
#include "hsl_reg.h"
#include "adpt_hppe.h"
#include "adpt.h"
#include "adpt_appe_flow.h"
#if defined(JHPPE)
#include "adpt_jhppe_flow.h"
#endif

#define FLOW_ENTRY_TYPE_IPV4 0
#define FLOW_ENTRY_TYPE_IPV6 1
#define FLOW_TUPLE_TYPE_3    0

#if defined(MRPPE)
static DECLARE_BITMAP(flow_cookie_48bit, EG_FLOW_TREE_MAP_TBL_NUM);
#endif

sw_error_t
adpt_hppe_ip_flow_host_data_rd_add(a_uint32_t dev_id, fal_host_entry_t * host_entry)

{
	a_uint8_t mode = 0, type = 0;
	sw_error_t rv = SW_OK;
	ADPT_DEV_ID_CHECK(dev_id);
	ADPT_NULL_POINT_CHECK(host_entry);

	mode = host_entry->flags >> 24;
	type = host_entry->flags & 0xff;

	if ((type & FAL_IP_IP4_ADDR) == FAL_IP_IP4_ADDR) {
		union host_tbl_u entry;
		aos_mem_zero(&entry, sizeof(entry));

		entry.bf.valid= host_entry->status;
		entry.bf.key_type = 0;
		entry.bf.fwd_cmd = host_entry->action;
		entry.bf.syn_toggle = host_entry->syn_toggle;
		entry.bf.dst_info = host_entry->dst_info;
		entry.bf.lan_wan = host_entry->lan_wan;
		entry.bf.ip_addr = host_entry->ip4_addr;
		rv = hppe_flow_host_ipv4_data_rd_add(dev_id, (a_uint32_t)mode, &host_entry->entry_id, &entry);
	} else if ((type & FAL_IP_IP6_ADDR) == FAL_IP_IP6_ADDR) {
		union host_ipv6_tbl_u entry;
		aos_mem_zero(&entry, sizeof(entry));

		entry.bf.valid= host_entry->status;
		entry.bf.key_type = 2;
		entry.bf.fwd_cmd = host_entry->action;
		entry.bf.syn_toggle = host_entry->syn_toggle;
		entry.bf.dst_info = host_entry->dst_info;
		entry.bf.lan_wan = host_entry->lan_wan;
		entry.bf.ipv6_addr_0 = host_entry->ip6_addr.ul[3];
		entry.bf.ipv6_addr_1 = host_entry->ip6_addr.ul[3] >> 10 | \
							host_entry->ip6_addr.ul[2] << 22;
		entry.bf.ipv6_addr_2 = host_entry->ip6_addr.ul[2] >> 10 | \
							host_entry->ip6_addr.ul[1] << 22;
		entry.bf.ipv6_addr_3 = host_entry->ip6_addr.ul[1] >> 10 | \
							host_entry->ip6_addr.ul[0] << 22;
		entry.bf.ipv6_addr_4 = host_entry->ip6_addr.ul[0] >> 10;
		rv = hppe_flow_host_ipv6_data_rd_add(dev_id, (a_uint32_t)mode, &host_entry->entry_id, &entry);
	}

	return SW_OK;
}

sw_error_t
adpt_hppe_ip_flow_host_data_add(a_uint32_t dev_id, fal_host_entry_t * host_entry)
{
	a_uint8_t mode = 0, type = 0;
	sw_error_t rv = SW_OK;
	ADPT_DEV_ID_CHECK(dev_id);
	ADPT_NULL_POINT_CHECK(host_entry);

	mode = host_entry->flags >> 24;
	type = host_entry->flags & 0xff;

	if ((type & FAL_IP_IP4_ADDR) == FAL_IP_IP4_ADDR) {
		union host_tbl_u entry;
		aos_mem_zero(&entry, sizeof(entry));

		entry.bf.valid= host_entry->status;
		entry.bf.key_type = 0;
		entry.bf.fwd_cmd = host_entry->action;
		entry.bf.syn_toggle = host_entry->syn_toggle;
		entry.bf.dst_info = host_entry->dst_info;
		entry.bf.lan_wan = host_entry->lan_wan;
		entry.bf.ip_addr = host_entry->ip4_addr;
		rv = hppe_flow_host_ipv4_data_add(dev_id, (a_uint32_t)mode, &host_entry->entry_id, &entry);
	} else if ((type & FAL_IP_IP6_ADDR) == FAL_IP_IP6_ADDR) {
		union host_ipv6_tbl_u entry;
		aos_mem_zero(&entry, sizeof(entry));

		entry.bf.valid= host_entry->status;
		entry.bf.key_type = 2;
		entry.bf.fwd_cmd = host_entry->action;
		entry.bf.syn_toggle = host_entry->syn_toggle;
		entry.bf.dst_info = host_entry->dst_info;
		entry.bf.lan_wan = host_entry->lan_wan;
		entry.bf.ipv6_addr_0 = host_entry->ip6_addr.ul[3];
		entry.bf.ipv6_addr_1 = host_entry->ip6_addr.ul[3] >> 10 | \
							host_entry->ip6_addr.ul[2] << 22;
		entry.bf.ipv6_addr_2 = host_entry->ip6_addr.ul[2] >> 10 | \
							host_entry->ip6_addr.ul[1] << 22;
		entry.bf.ipv6_addr_3 = host_entry->ip6_addr.ul[1] >> 10 | \
							host_entry->ip6_addr.ul[0] << 22;
		entry.bf.ipv6_addr_4 = host_entry->ip6_addr.ul[0] >> 10;
		rv = hppe_flow_host_ipv6_data_add(dev_id, (a_uint32_t)mode, &host_entry->entry_id, &entry);
	}

	return SW_OK;
}

sw_error_t
adpt_hppe_ip_flow_host_data_get(a_uint32_t dev_id, a_uint32_t get_mode,
                    fal_host_entry_t *host_entry)
{
	a_uint8_t mode = 0, type = 0;
	sw_error_t rv = SW_OK;
	ADPT_DEV_ID_CHECK(dev_id);
	ADPT_NULL_POINT_CHECK(host_entry);

	mode = host_entry->flags >> 24;
	type = host_entry->flags & 0xff;

	if (get_mode & FAL_IP_ENTRY_ID_EN)
		mode = 1;
	else if (get_mode & FAL_IP_ENTRY_IPADDR_EN)
		mode  = 0;

	if ((type & FAL_IP_IP4_ADDR) == FAL_IP_IP4_ADDR) {
		union host_tbl_u entry;
		aos_mem_zero(&entry, sizeof(entry));

		entry.bf.key_type = 0;
		entry.bf.ip_addr = host_entry->ip4_addr;
		rv = hppe_flow_host_ipv4_data_get(dev_id, (a_uint32_t)mode, &host_entry->entry_id, &entry);
		host_entry->ip4_addr = entry.bf.ip_addr;
		host_entry->lan_wan = entry.bf.lan_wan;
		host_entry->dst_info= entry.bf.dst_info;
		host_entry->syn_toggle = entry.bf.syn_toggle;
		host_entry->action = entry.bf.fwd_cmd;
		host_entry->status = entry.bf.valid;
	} else if ((type & FAL_IP_IP6_ADDR) == FAL_IP_IP6_ADDR) {
		union host_ipv6_tbl_u entry;
		aos_mem_zero(&entry, sizeof(entry));

		entry.bf.key_type = 2;
		entry.bf.ipv6_addr_0 = host_entry->ip6_addr.ul[3];
		entry.bf.ipv6_addr_1 = host_entry->ip6_addr.ul[3] >> 10 | \
							host_entry->ip6_addr.ul[2] << 22;
		entry.bf.ipv6_addr_2 = host_entry->ip6_addr.ul[2] >> 10 | \
							host_entry->ip6_addr.ul[1] << 22;
		entry.bf.ipv6_addr_3 = host_entry->ip6_addr.ul[1] >> 10 | \
							host_entry->ip6_addr.ul[0] << 22;
		entry.bf.ipv6_addr_4 = host_entry->ip6_addr.ul[0] >> 10;
		rv = hppe_flow_host_ipv6_data_get(dev_id, (a_uint32_t)mode, &host_entry->entry_id, &entry);
		host_entry->ip6_addr.ul[3] = entry.bf.ipv6_addr_0 | entry.bf.ipv6_addr_1 << 10;
		host_entry->ip6_addr.ul[2] = entry.bf.ipv6_addr_1 >> 22 | entry.bf.ipv6_addr_2 << 10;
		host_entry->ip6_addr.ul[1] = entry.bf.ipv6_addr_2 >> 22 | entry.bf.ipv6_addr_3 << 10;
		host_entry->ip6_addr.ul[0] = entry.bf.ipv6_addr_3 >> 22 | entry.bf.ipv6_addr_4 << 10;
		host_entry->lan_wan = entry.bf.lan_wan;
		host_entry->dst_info= entry.bf.dst_info;
		host_entry->syn_toggle = entry.bf.syn_toggle;
		host_entry->action = entry.bf.fwd_cmd;
		host_entry->status = entry.bf.valid;
	}
	return rv;
}

sw_error_t
adpt_hppe_ip_flow_host_data_del(a_uint32_t dev_id, a_uint32_t del_mode,
                    fal_host_entry_t * host_entry)
{
	a_uint8_t mode = 0, type = 0;
	sw_error_t rv = SW_OK;
	ADPT_DEV_ID_CHECK(dev_id);
	ADPT_NULL_POINT_CHECK(host_entry);

	mode = host_entry->flags >> 24;
	type = host_entry->flags & 0xff;

	if (del_mode & FAL_IP_ENTRY_ID_EN)
		mode = 1;
	else if (del_mode & FAL_IP_ENTRY_IPADDR_EN)
		mode  = 0;
	else if (del_mode & FAL_IP_ENTRY_ALL_EN) {
		return hppe_flow_host_flush_common(dev_id);
	}

	if ((type & FAL_IP_IP4_ADDR) == FAL_IP_IP4_ADDR) {
		union host_tbl_u entry;
		aos_mem_zero(&entry, sizeof(entry));

		entry.bf.valid= 1;
		entry.bf.key_type = 0;
		entry.bf.fwd_cmd = host_entry->action;
		entry.bf.syn_toggle = host_entry->syn_toggle;
		entry.bf.dst_info = host_entry->dst_info;
		entry.bf.lan_wan = host_entry->lan_wan;
		entry.bf.ip_addr = host_entry->ip4_addr;
		rv = hppe_flow_host_ipv4_data_del(dev_id, (a_uint32_t)mode, &host_entry->entry_id, &entry);
	} else if ((type & FAL_IP_IP6_ADDR) == FAL_IP_IP6_ADDR) {
		union host_ipv6_tbl_u entry;
		aos_mem_zero(&entry, sizeof(entry));

		entry.bf.valid= 1;
		entry.bf.key_type = 2;
		entry.bf.fwd_cmd = host_entry->action;
		entry.bf.syn_toggle = host_entry->syn_toggle;
		entry.bf.dst_info = host_entry->dst_info;
		entry.bf.lan_wan = host_entry->lan_wan;
		entry.bf.ipv6_addr_0 = host_entry->ip6_addr.ul[3];
		entry.bf.ipv6_addr_1 = host_entry->ip6_addr.ul[3] >> 10 | \
							host_entry->ip6_addr.ul[2] << 22;
		entry.bf.ipv6_addr_2 = host_entry->ip6_addr.ul[2] >> 10 | \
							host_entry->ip6_addr.ul[1] << 22;
		entry.bf.ipv6_addr_3 = host_entry->ip6_addr.ul[1] >> 10 | \
							host_entry->ip6_addr.ul[0] << 22;
		entry.bf.ipv6_addr_4 = host_entry->ip6_addr.ul[0] >> 10;
		rv = hppe_flow_host_ipv6_data_del(dev_id, (a_uint32_t)mode, &host_entry->entry_id, &entry);
	}
	return rv;
}

static sw_error_t adpt_flow_cookie_convert(fal_flow_qos_t *flow_qos,
					   union eg_flow_tree_map_tbl_u *eg_treemap,
					   fal_flow_cookie_type_t type,
					   a_bool_t to_hsl)
{
	a_uint64_t tmp = 0;

	switch (type) {
	case FAL_FLOW_QOS_TYPE_TREE_ID:
		if (to_hsl == A_TRUE) {
			memcpy(&tmp, flow_qos->tree_id, sizeof(flow_qos->tree_id));
#if defined(JHPPE)
			eg_treemap->bf.tree_id_0 = FIELD_GET(FAL_FLOW_TREE_ID_24BIT_L, tmp);
			eg_treemap->bf.tree_id_1 = FIELD_GET(FAL_FLOW_TREE_ID_24BIT_H, tmp);
#else
			eg_treemap->bf.tree_id = FIELD_GET(FAL_FLOW_TREE_ID_24BIT, tmp);
#endif
#if defined(MPPE)
			eg_treemap->bf.type = 0;
#endif
		} else {
#if defined(JHPPE)
			tmp = FIELD_PREP(FAL_FLOW_TREE_ID_24BIT_L, eg_treemap->bf.tree_id_0);
			tmp |= FIELD_PREP(FAL_FLOW_TREE_ID_24BIT_H, eg_treemap->bf.tree_id_1);
#else
			tmp = FIELD_PREP(FAL_FLOW_TREE_ID_24BIT, eg_treemap->bf.tree_id);
#endif
			memcpy(flow_qos->tree_id, &tmp, sizeof(flow_qos->tree_id));
		}
		break;
#if defined(MRPPE)
	case FAL_FLOW_QOS_TYPE_COOKIE_40B:
		if (to_hsl == A_TRUE) {
			memcpy(&tmp, flow_qos->cookie_40b, sizeof(flow_qos->cookie_40b));
			eg_treemap->bf1.flow_cookie = FIELD_GET(FAL_FLOW_COOKIE_40BIT_LOW, tmp);
			eg_treemap->bf1.flow_cookie_ext_0 = FIELD_GET(FAL_FLOW_COOKIE_40BIT_HIGH_16BIT_L, tmp);
			eg_treemap->bf1.flow_cookie_ext_1 = FIELD_GET(FAL_FLOW_COOKIE_40BIT_HIGH_8BIT_H, tmp);
			eg_treemap->bf1.type = 1;
		} else {
			tmp = FIELD_PREP(FAL_FLOW_COOKIE_40BIT_LOW, eg_treemap->bf1.flow_cookie);
			tmp |= FIELD_PREP(FAL_FLOW_COOKIE_40BIT_HIGH_16BIT_L, eg_treemap->bf1.flow_cookie_ext_0);
			tmp |= FIELD_PREP(FAL_FLOW_COOKIE_40BIT_HIGH_8BIT_H, eg_treemap->bf1.flow_cookie_ext_1);
			memcpy(flow_qos->cookie_40b, &tmp, sizeof(flow_qos->cookie_40b));
		}
		break;
	case FAL_FLOW_QOS_TYPE_COOKIE_48B:
		if (to_hsl == A_TRUE) {
			memcpy(&tmp, flow_qos->cookie_48b, sizeof(flow_qos->cookie_48b));
			eg_treemap->bf1.flow_cookie = FIELD_GET(FAL_FLOW_COOKIE_48BIT_LOW, tmp);
			eg_treemap->bf1.wifi_qos = FIELD_GET(FAL_FLOW_COOKIE_48BIT_MIDDLE, tmp);
			eg_treemap->bf1.flow_cookie_ext_0 = FIELD_GET(FAL_FLOW_COOKIE_48BIT_HIGH_16BIT_L, tmp);
			eg_treemap->bf1.flow_cookie_ext_1 = FIELD_GET(FAL_FLOW_COOKIE_48BIT_HIGH_8BIT_H, tmp);

#if !defined(JHPPE)
			/* wifi qos is valid only when the flag is true on only Marina. */
			eg_treemap->bf1.wifi_qos_flag = true;
#endif
			eg_treemap->bf1.type = 1;
		} else {
			tmp = FIELD_PREP(FAL_FLOW_COOKIE_48BIT_LOW, eg_treemap->bf1.flow_cookie);
			tmp |= FIELD_PREP(FAL_FLOW_COOKIE_48BIT_MIDDLE, eg_treemap->bf1.wifi_qos);
			tmp |= FIELD_PREP(FAL_FLOW_COOKIE_48BIT_HIGH_16BIT_L, eg_treemap->bf1.flow_cookie_ext_0);
			tmp |= FIELD_PREP(FAL_FLOW_COOKIE_48BIT_HIGH_8BIT_H, eg_treemap->bf1.flow_cookie_ext_1);
			memcpy(flow_qos->cookie_48b, &tmp, sizeof(flow_qos->cookie_48b));

			/* qos & qos_valid is already assigned before this function called. */
		}
		break;
#elif defined(MPPE)
	case FAL_FLOW_QOS_TYPE_COOKIE_16B:
		if (to_hsl == A_TRUE) {
			memcpy(&tmp, flow_qos->cookie_16b, sizeof(flow_qos->cookie_16b));
			eg_treemap->bf2.flow_cookie = FIELD_GET(FAL_FLOW_COOKIE_16BIT, tmp);
			eg_treemap->bf2.type = 1;
		} else {
			tmp = FIELD_PREP(FAL_FLOW_COOKIE_16BIT, eg_treemap->bf2.flow_cookie);
			memcpy(flow_qos->cookie_16b, &tmp, sizeof(flow_qos->cookie_16b));
		}
		break;
#endif
	default:
		break;
	}

	return SW_OK;
}

sw_error_t
adpt_hppe_flow_qos_set(a_uint32_t dev_id, a_uint32_t flow_index, fal_flow_qos_t *flow_qos)
{
	sw_error_t rv = SW_OK;
	union eg_flow_tree_map_tbl_u eg_treemap;

	ADPT_DEV_ID_CHECK(dev_id);
	ADPT_NULL_POINT_CHECK(flow_qos);

	if (flow_index >= EG_FLOW_TREE_MAP_TBL_NUM)
		return SW_OUT_OF_RANGE;

	aos_mem_zero(&eg_treemap, sizeof(eg_treemap));

	rv = hppe_eg_flow_tree_map_tbl_get(dev_id, flow_index, &eg_treemap);
	SW_RTN_ON_ERROR(rv);

	eg_treemap.bf.wifi_qos_flag = flow_qos->qos_valid;
	eg_treemap.bf.wifi_qos = flow_qos->qos;

	adpt_flow_cookie_convert(flow_qos, &eg_treemap, flow_qos->type, A_TRUE);

	rv = hppe_eg_flow_tree_map_tbl_set(dev_id, flow_index, &eg_treemap);
	SW_RTN_ON_ERROR(rv);

#if defined(MRPPE)
	if (flow_qos->type == FAL_FLOW_QOS_TYPE_COOKIE_48B)
		set_bit(flow_index, flow_cookie_48bit);
	else
		clear_bit(flow_index, flow_cookie_48bit);
#endif
	return rv;
}

sw_error_t
adpt_hppe_flow_qos_get(a_uint32_t dev_id, a_uint32_t flow_index, fal_flow_qos_t *flow_qos)
{
	sw_error_t rv = SW_OK;
	union eg_flow_tree_map_tbl_u eg_treemap;

	ADPT_DEV_ID_CHECK(dev_id);
	ADPT_NULL_POINT_CHECK(flow_qos);

	if (flow_index >= EG_FLOW_TREE_MAP_TBL_NUM)
		return SW_OUT_OF_RANGE;

	aos_mem_zero(&eg_treemap, sizeof(eg_treemap));
	aos_mem_zero(flow_qos, sizeof(*flow_qos));

	rv = hppe_eg_flow_tree_map_tbl_get(dev_id, flow_index, &eg_treemap);
	SW_RTN_ON_ERROR(rv);

	flow_qos->qos_valid = eg_treemap.bf.wifi_qos_flag;
	flow_qos->qos = eg_treemap.bf.wifi_qos;

	/* For legacy chip HPPE/APPE, only tree id supported. */
	flow_qos->type = FAL_FLOW_QOS_TYPE_TREE_ID;

#if defined(MPPE)
	/* For MPPE, only 16B flow cookie supported. */
	if (eg_treemap.bf.type)
		flow_qos->type = FAL_FLOW_QOS_TYPE_COOKIE_16B;
#endif

#if defined(MRPPE)
	/* For MRPPE hardware config, 48 bit cookie and 40 bit cookie + 8 bit
	 * qos can't be distinguished, Need to distinguish it from the software
	 * cached bitmap.
	 */
	if (flow_qos->type) {
		if (test_bit(flow_index, flow_cookie_48bit))
			flow_qos->type = FAL_FLOW_QOS_TYPE_COOKIE_48B;
		else
			flow_qos->type = FAL_FLOW_QOS_TYPE_COOKIE_40B;
	}
#endif

	adpt_flow_cookie_convert(flow_qos, &eg_treemap, flow_qos->type, A_FALSE);

	return rv;
}

sw_error_t adpt_ppe_flow_key_ipv4_5tuple_convert(fal_flow_entry_t *flow_entry,
		union in_flow_tbl_u *entry, a_bool_t to_hsl)
{
	if (to_hsl) {
		entry->bf.valid= !flow_entry->invalid;
		entry->bf.entry_type = FLOW_ENTRY_TYPE_IPV4;
		entry->bf.protocol_type = flow_entry->protocol;
		entry->bf.host_addr_index_type = flow_entry->host_addr_type;
		entry->bf.host_addr_index = flow_entry->host_addr_index;
		entry->bf.ip_addr_0 = flow_entry->flow_ip.ipv4;
		entry->bf.ip_addr_1 = flow_entry->flow_ip.ipv4 >> SW_FIELD_OFFSET_IN_WORD(IN_FLOW_TBL_IP_ADDR_OFFSET);
		entry->bf.l4_sport = flow_entry->src_port;
		entry->bf.l4_dport_0 = flow_entry->dst_port;
		entry->bf.l4_dport_1 = flow_entry->dst_port >> SW_FIELD_OFFSET_IN_WORD(IN_FLOW_TBL_L4_DPORT_OFFSET);
		entry->bf.vpn_id = flow_entry->vpn_id;
#if defined(JHPPE)
		entry->bf.routing = flow_entry->route_en;
#endif
	} else {
		flow_entry->host_addr_type = entry->bf.host_addr_index_type;
		flow_entry->host_addr_index = entry->bf.host_addr_index;
		flow_entry->protocol = entry->bf.protocol_type;
		flow_entry->invalid = !entry->bf.valid;
		flow_entry->flow_ip.ipv4 = entry->bf.ip_addr_0;
		flow_entry->flow_ip.ipv4 |= entry->bf.ip_addr_1 << SW_FIELD_OFFSET_IN_WORD(IN_FLOW_TBL_IP_ADDR_OFFSET);
		flow_entry->src_port = entry->bf.l4_sport;
		flow_entry->dst_port = entry->bf.l4_dport_0;
		flow_entry->dst_port |= entry->bf.l4_dport_1 << SW_FIELD_OFFSET_IN_WORD(IN_FLOW_TBL_L4_DPORT_OFFSET);
		flow_entry->vpn_id = entry->bf.vpn_id;
		flow_entry->entry_type = FAL_FLOW_IP4_5TUPLE_ADDR;
#if defined(JHPPE)
		flow_entry->route_en = entry->bf.routing;
#endif
	}

	return SW_OK;
}

sw_error_t adpt_ppe_flow_key_ipv6_5tuple_convert(fal_flow_entry_t *flow_entry,
		union in_flow_ipv6_5tuple_tbl_u *entry, a_bool_t to_hsl)
{
	if (to_hsl) {
		entry->bf.valid= flow_entry->invalid ? 0 : 1;
		entry->bf.entry_type = FLOW_ENTRY_TYPE_IPV6;
		entry->bf.host_addr_index_type = flow_entry->host_addr_type;
		entry->bf.host_addr_index = flow_entry->host_addr_index;
		entry->bf.protocol_type = flow_entry->protocol;
		entry->bf.ip_addr_0 = flow_entry->flow_ip.ipv6.ul[3];
		entry->bf.ip_addr_1 = flow_entry->flow_ip.ipv6.ul[3] >> SW_FIELD_OFFSET_IN_WORD(IN_FLOW_IPV6_5TUPLE_TBL_IP_ADDR_OFFSET) | \
				     flow_entry->flow_ip.ipv6.ul[2] << (32 - SW_FIELD_OFFSET_IN_WORD(IN_FLOW_IPV6_5TUPLE_TBL_IP_ADDR_OFFSET));
		entry->bf.ip_addr_2 = flow_entry->flow_ip.ipv6.ul[2] >> SW_FIELD_OFFSET_IN_WORD(IN_FLOW_IPV6_5TUPLE_TBL_IP_ADDR_OFFSET) |\
				     flow_entry->flow_ip.ipv6.ul[1] << (32 - SW_FIELD_OFFSET_IN_WORD(IN_FLOW_IPV6_5TUPLE_TBL_IP_ADDR_OFFSET));
		entry->bf.ip_addr_3 = flow_entry->flow_ip.ipv6.ul[1] >> SW_FIELD_OFFSET_IN_WORD(IN_FLOW_IPV6_5TUPLE_TBL_IP_ADDR_OFFSET) |\
				     flow_entry->flow_ip.ipv6.ul[0] << (32 - SW_FIELD_OFFSET_IN_WORD(IN_FLOW_IPV6_5TUPLE_TBL_IP_ADDR_OFFSET));
		entry->bf.ip_addr_4 = flow_entry->flow_ip.ipv6.ul[0] >> SW_FIELD_OFFSET_IN_WORD(IN_FLOW_IPV6_5TUPLE_TBL_IP_ADDR_OFFSET);
#if defined(JHPPE)
		entry->bf.l4_dport = flow_entry->dst_port;
		entry->bf.l4_sport_0 = flow_entry->src_port;
		entry->bf.l4_sport_1 = flow_entry->src_port >> SW_FIELD_OFFSET_IN_WORD(IN_FLOW_IPV6_5TUPLE_TBL_L4_SPORT_OFFSET);
#else
		entry->bf.l4_sport = flow_entry->src_port;
		entry->bf.l4_dport_0 = flow_entry->dst_port;
		entry->bf.l4_dport_1 = flow_entry->dst_port >> SW_FIELD_OFFSET_IN_WORD(IN_FLOW_IPV6_5TUPLE_TBL_L4_DPORT_OFFSET);
#endif
#if defined(JHPPE)
		entry->bf.routing = flow_entry->route_en;
#endif
		entry->bf.vpn_id = flow_entry->vpn_id;
	} else {
		flow_entry->invalid = !entry->bf.valid;
		flow_entry->host_addr_type = entry->bf.host_addr_index_type;
		flow_entry->host_addr_index = entry->bf.host_addr_index;
		flow_entry->protocol = entry->bf.protocol_type;
#if defined(JHPPE)
		flow_entry->dst_port = entry->bf.l4_dport;
		flow_entry->src_port = entry->bf.l4_sport_0;
		flow_entry->src_port |= entry->bf.l4_sport_1 << SW_FIELD_OFFSET_IN_WORD(IN_FLOW_IPV6_5TUPLE_TBL_L4_SPORT_OFFSET);
#else
		flow_entry->src_port = entry->bf.l4_sport;
		flow_entry->dst_port = entry->bf.l4_dport_0;
		flow_entry->dst_port |= entry->bf.l4_dport_1 << SW_FIELD_OFFSET_IN_WORD(IN_FLOW_IPV6_5TUPLE_TBL_L4_DPORT_OFFSET);
#endif
		flow_entry->flow_ip.ipv6.ul[3] = entry->bf.ip_addr_0 |\
						 entry->bf.ip_addr_1 << SW_FIELD_OFFSET_IN_WORD(IN_FLOW_IPV6_5TUPLE_TBL_IP_ADDR_OFFSET);
		flow_entry->flow_ip.ipv6.ul[2] = entry->bf.ip_addr_1 >> (32 - SW_FIELD_OFFSET_IN_WORD(IN_FLOW_IPV6_5TUPLE_TBL_IP_ADDR_OFFSET)) |\
						 entry->bf.ip_addr_2 << SW_FIELD_OFFSET_IN_WORD(IN_FLOW_IPV6_5TUPLE_TBL_IP_ADDR_OFFSET);
		flow_entry->flow_ip.ipv6.ul[1] = entry->bf.ip_addr_2 >> (32 - SW_FIELD_OFFSET_IN_WORD(IN_FLOW_IPV6_5TUPLE_TBL_IP_ADDR_OFFSET)) |\
						 entry->bf.ip_addr_3 << SW_FIELD_OFFSET_IN_WORD(IN_FLOW_IPV6_5TUPLE_TBL_IP_ADDR_OFFSET);
		flow_entry->flow_ip.ipv6.ul[0] = entry->bf.ip_addr_3 >> (32 - SW_FIELD_OFFSET_IN_WORD(IN_FLOW_IPV6_5TUPLE_TBL_IP_ADDR_OFFSET)) |\
						 entry->bf.ip_addr_4 << SW_FIELD_OFFSET_IN_WORD(IN_FLOW_IPV6_5TUPLE_TBL_IP_ADDR_OFFSET);
		flow_entry->vpn_id = entry->bf.vpn_id;
		flow_entry->entry_type = FAL_FLOW_IP6_5TUPLE_ADDR;
	}

	return SW_OK;
}

sw_error_t adpt_ppe_flow_key_ipv4_3tuple_convert(fal_flow_entry_t *flow_entry,
		union in_flow_3tuple_tbl_u *entry, a_bool_t to_hsl)
{
	if (to_hsl) {
		entry->bf.valid= !flow_entry->invalid;
		entry->bf.entry_type = FLOW_ENTRY_TYPE_IPV4;
		entry->bf.host_addr_index_type = flow_entry->host_addr_type;
		entry->bf.host_addr_index = flow_entry->host_addr_index;
		entry->bf.protocol_type = flow_entry->protocol;
#if defined(JHPPE)
		entry->bf.routing = flow_entry->route_en;
#endif
		entry->bf.ip_addr_0 = flow_entry->flow_ip.ipv4;
		entry->bf.ip_addr_1 = flow_entry->flow_ip.ipv4 >> SW_FIELD_OFFSET_IN_WORD(IN_FLOW_3TUPLE_TBL_IP_ADDR_OFFSET);
		entry->bf.ip_protocol = flow_entry->ip_type;
#if defined(JHPPE)
		entry->bf.vpn_id_0 = flow_entry->vpn_id;
		entry->bf.vpn_id_1 = flow_entry->vpn_id >> SW_FIELD_OFFSET_IN_WORD(IN_FLOW_3TUPLE_TBL_VPN_ID_OFFSET);
#else
		entry->bf.vpn_id = flow_entry->vpn_id;
#endif
	} else {
		flow_entry->invalid = !entry->bf.valid;
		flow_entry->host_addr_type = entry->bf.host_addr_index_type;
		flow_entry->host_addr_index = entry->bf.host_addr_index;
		flow_entry->protocol = entry->bf.protocol_type;
		flow_entry->flow_ip.ipv4 = entry->bf.ip_addr_0;
		flow_entry->flow_ip.ipv4 |= entry->bf.ip_addr_1 << SW_FIELD_OFFSET_IN_WORD(IN_FLOW_3TUPLE_TBL_IP_ADDR_OFFSET);
		flow_entry->ip_type = entry->bf.ip_protocol;
#if defined(JHPPE)
		flow_entry->vpn_id = entry->bf.vpn_id_0;
		flow_entry->vpn_id |= entry->bf.vpn_id_1 << SW_FIELD_OFFSET_IN_WORD(IN_FLOW_3TUPLE_TBL_VPN_ID_OFFSET);
#else
		flow_entry->vpn_id = entry->bf.vpn_id;
#endif
		flow_entry->entry_type = FAL_FLOW_IP4_3TUPLE_ADDR;
	}

	return SW_OK;
}

sw_error_t adpt_ppe_flow_key_ipv6_3tuple_convert(fal_flow_entry_t *flow_entry,
		union in_flow_ipv6_3tuple_tbl_u *entry, a_bool_t to_hsl)
{
	if (to_hsl) {
		entry->bf.valid= !flow_entry->invalid;
		entry->bf.entry_type = FLOW_ENTRY_TYPE_IPV6;
		entry->bf.host_addr_index_type = flow_entry->host_addr_type;
		entry->bf.host_addr_index = flow_entry->host_addr_index;
		entry->bf.protocol_type = flow_entry->protocol;
		entry->bf.ip_addr_0 = flow_entry->flow_ip.ipv6.ul[3];
		entry->bf.ip_addr_1 = flow_entry->flow_ip.ipv6.ul[3] >> SW_FIELD_OFFSET_IN_WORD(IN_FLOW_IPV6_3TUPLE_TBL_IP_ADDR_OFFSET) | \
				     flow_entry->flow_ip.ipv6.ul[2] << (32 - SW_FIELD_OFFSET_IN_WORD(IN_FLOW_IPV6_3TUPLE_TBL_IP_ADDR_OFFSET));
		entry->bf.ip_addr_2 = flow_entry->flow_ip.ipv6.ul[2] >> SW_FIELD_OFFSET_IN_WORD(IN_FLOW_IPV6_3TUPLE_TBL_IP_ADDR_OFFSET) |\
				     flow_entry->flow_ip.ipv6.ul[1] << (32 - SW_FIELD_OFFSET_IN_WORD(IN_FLOW_IPV6_3TUPLE_TBL_IP_ADDR_OFFSET));
		entry->bf.ip_addr_3 = flow_entry->flow_ip.ipv6.ul[1] >> SW_FIELD_OFFSET_IN_WORD(IN_FLOW_IPV6_3TUPLE_TBL_IP_ADDR_OFFSET) |\
				     flow_entry->flow_ip.ipv6.ul[0] << (32 - SW_FIELD_OFFSET_IN_WORD(IN_FLOW_IPV6_3TUPLE_TBL_IP_ADDR_OFFSET));
		entry->bf.ip_addr_4 = flow_entry->flow_ip.ipv6.ul[0] >> SW_FIELD_OFFSET_IN_WORD(IN_FLOW_IPV6_3TUPLE_TBL_IP_ADDR_OFFSET);
		entry->bf.ip_protocol = flow_entry->ip_type;
#if defined(JHPPE)
		entry->bf.routing = flow_entry->route_en;
#endif
		entry->bf.vpn_id = flow_entry->vpn_id;
	} else {
		flow_entry->invalid = !entry->bf.valid;
		flow_entry->host_addr_type = entry->bf.host_addr_index_type;
		flow_entry->host_addr_index = entry->bf.host_addr_index;
		flow_entry->protocol = entry->bf.protocol_type;
		flow_entry->flow_ip.ipv6.ul[3] = entry->bf.ip_addr_0 |\
						 entry->bf.ip_addr_1 << SW_FIELD_OFFSET_IN_WORD(IN_FLOW_IPV6_3TUPLE_TBL_IP_ADDR_OFFSET);
		flow_entry->flow_ip.ipv6.ul[2] = entry->bf.ip_addr_1 >> (32 - SW_FIELD_OFFSET_IN_WORD(IN_FLOW_IPV6_3TUPLE_TBL_IP_ADDR_OFFSET)) |\
						 entry->bf.ip_addr_2 << SW_FIELD_OFFSET_IN_WORD(IN_FLOW_IPV6_3TUPLE_TBL_IP_ADDR_OFFSET);
		flow_entry->flow_ip.ipv6.ul[1] = entry->bf.ip_addr_2 >> (32 - SW_FIELD_OFFSET_IN_WORD(IN_FLOW_IPV6_3TUPLE_TBL_IP_ADDR_OFFSET)) |\
						 entry->bf.ip_addr_3 << SW_FIELD_OFFSET_IN_WORD(IN_FLOW_IPV6_3TUPLE_TBL_IP_ADDR_OFFSET);
		flow_entry->flow_ip.ipv6.ul[0] = entry->bf.ip_addr_3 >> (32 - SW_FIELD_OFFSET_IN_WORD(IN_FLOW_IPV6_3TUPLE_TBL_IP_ADDR_OFFSET)) |\
						 entry->bf.ip_addr_4 << SW_FIELD_OFFSET_IN_WORD(IN_FLOW_IPV6_3TUPLE_TBL_IP_ADDR_OFFSET);
		flow_entry->ip_type = entry->bf.ip_protocol;
		flow_entry->vpn_id = entry->bf.vpn_id;
		flow_entry->entry_type = FAL_FLOW_IP6_3TUPLE_ADDR;
#if defined(JHPPE)
		flow_entry->route_en = entry->bf.routing;
#endif

	}

	return SW_OK;
}

#if defined(JHPPE)
sw_error_t adpt_ppe_flow_key_ip_6tuple_convert(fal_flow_entry_t *flow_entry,
		union in_flow_6tuple_tbl_u *entry, a_uint32_t type, a_bool_t to_hsl)
{
	if (to_hsl) {
		entry->bf.valid= !flow_entry->invalid;
		entry->bf.host_addr_index_type = flow_entry->host_addr_type;
		entry->bf.host_addr_index = flow_entry->host_addr_index;
		if (type & FAL_FLOW_IP6_6TUPLE_ADDR) {
			entry->bf.entry_type = FLOW_ENTRY_TYPE_IPV6;
			entry->bf.ip_addr_0 = flow_entry->flow_ip.ipv6.ul[3];
			entry->bf.ip_addr_1 = flow_entry->flow_ip.ipv6.ul[3] >> SW_FIELD_OFFSET_IN_WORD(IN_FLOW_6TUPLE_TBL_IP_ADDR_OFFSET) | \
					     flow_entry->flow_ip.ipv6.ul[2] << (32 - SW_FIELD_OFFSET_IN_WORD(IN_FLOW_6TUPLE_TBL_IP_ADDR_OFFSET));
			entry->bf.ip_addr_2 = flow_entry->flow_ip.ipv6.ul[2] >> SW_FIELD_OFFSET_IN_WORD(IN_FLOW_6TUPLE_TBL_IP_ADDR_OFFSET) |\
					     flow_entry->flow_ip.ipv6.ul[1] << (32 - SW_FIELD_OFFSET_IN_WORD(IN_FLOW_6TUPLE_TBL_IP_ADDR_OFFSET));
			entry->bf.ip_addr_3 = flow_entry->flow_ip.ipv6.ul[1] >> SW_FIELD_OFFSET_IN_WORD(IN_FLOW_6TUPLE_TBL_IP_ADDR_OFFSET) |\
					     flow_entry->flow_ip.ipv6.ul[0] << (32 - SW_FIELD_OFFSET_IN_WORD(IN_FLOW_6TUPLE_TBL_IP_ADDR_OFFSET));
			entry->bf.ip_addr_4 = flow_entry->flow_ip.ipv6.ul[0] >> SW_FIELD_OFFSET_IN_WORD(IN_FLOW_6TUPLE_TBL_IP_ADDR_OFFSET);
		} else {
			entry->bf.entry_type = FLOW_ENTRY_TYPE_IPV4;
			entry->bf.ip_addr_0 = flow_entry->flow_ip.ipv4;
			entry->bf.ip_addr_1 = flow_entry->flow_ip.ipv4 >> SW_FIELD_OFFSET_IN_WORD(IN_FLOW_6TUPLE_TBL_IP_ADDR_OFFSET);
		}
		entry->bf.protocol_type = flow_entry->protocol;
		entry->bf.ip_protocol = flow_entry->ip_type;
		entry->bf.l4_sport = flow_entry->src_port;
		entry->bf.l4_dport_0 = flow_entry->dst_port;
		entry->bf.l4_dport_1 = flow_entry->dst_port >> SW_FIELD_OFFSET_IN_WORD(IN_FLOW_6TUPLE_TBL_L4_DPORT_OFFSET);
		entry->bf.vpn_id_0 = flow_entry->vpn_id;
		entry->bf.vpn_id_1 = flow_entry->vpn_id >> SW_FIELD_OFFSET_IN_WORD(IN_FLOW_6TUPLE_TBL_VPN_ID_OFFSET);;
		entry->bf.routing = flow_entry->route_en;
		entry->bf.match_more_0 = flow_entry->match_more;
		entry->bf.match_more_1 = flow_entry->match_more >> SW_FIELD_OFFSET_IN_WORD(IN_FLOW_6TUPLE_TBL_MATCH_MORE_OFFSET);
		entry->bf.udf0_valid = flow_entry->udf0_en;
		entry->bf.udf1_valid = flow_entry->udf1_en;
	} else {
		flow_entry->invalid = !entry->bf.valid;
		flow_entry->host_addr_type = entry->bf.host_addr_index_type;
		flow_entry->host_addr_index = entry->bf.host_addr_index;

		if (type & FAL_FLOW_IP6_6TUPLE_ADDR) {
			flow_entry->flow_ip.ipv6.ul[3] = entry->bf.ip_addr_0 |\
							 entry->bf.ip_addr_1 << SW_FIELD_OFFSET_IN_WORD(IN_FLOW_6TUPLE_TBL_IP_ADDR_OFFSET);
			flow_entry->flow_ip.ipv6.ul[2] = entry->bf.ip_addr_1 >> (32 - SW_FIELD_OFFSET_IN_WORD(IN_FLOW_6TUPLE_TBL_IP_ADDR_OFFSET)) |\
							 entry->bf.ip_addr_2 << SW_FIELD_OFFSET_IN_WORD(IN_FLOW_6TUPLE_TBL_IP_ADDR_OFFSET);
			flow_entry->flow_ip.ipv6.ul[1] = entry->bf.ip_addr_2 >> (32 - SW_FIELD_OFFSET_IN_WORD(IN_FLOW_6TUPLE_TBL_IP_ADDR_OFFSET)) |\
							 entry->bf.ip_addr_3 << SW_FIELD_OFFSET_IN_WORD(IN_FLOW_6TUPLE_TBL_IP_ADDR_OFFSET);
			flow_entry->flow_ip.ipv6.ul[0] = entry->bf.ip_addr_3 >> (32 - SW_FIELD_OFFSET_IN_WORD(IN_FLOW_6TUPLE_TBL_IP_ADDR_OFFSET)) |\
							 entry->bf.ip_addr_4 << SW_FIELD_OFFSET_IN_WORD(IN_FLOW_6TUPLE_TBL_IP_ADDR_OFFSET);
			flow_entry->entry_type = FAL_FLOW_IP6_6TUPLE_ADDR;
		} else {
			flow_entry->flow_ip.ipv4 = entry->bf.ip_addr_0;
			flow_entry->flow_ip.ipv4 |= entry->bf.ip_addr_1 << SW_FIELD_OFFSET_IN_WORD(IN_FLOW_6TUPLE_TBL_IP_ADDR_OFFSET);
			flow_entry->entry_type = FAL_FLOW_IP4_6TUPLE_ADDR;
		}

		flow_entry->protocol = entry->bf.protocol_type;
		flow_entry->ip_type = entry->bf.ip_protocol;
		flow_entry->src_port = entry->bf.l4_sport;
		flow_entry->dst_port = entry->bf.l4_dport_0;
		flow_entry->dst_port |= entry->bf.l4_dport_1 << SW_FIELD_OFFSET_IN_WORD(IN_FLOW_6TUPLE_TBL_L4_DPORT_OFFSET);
		flow_entry->vpn_id = entry->bf.vpn_id_0;
		flow_entry->vpn_id |= entry->bf.vpn_id_1 << SW_FIELD_OFFSET_IN_WORD(IN_FLOW_6TUPLE_TBL_VPN_ID_OFFSET);
		flow_entry->route_en = entry->bf.routing;
		flow_entry->match_more = entry->bf.match_more_0;
		flow_entry->match_more = entry->bf.match_more_1 << SW_FIELD_OFFSET_IN_WORD(IN_FLOW_6TUPLE_TBL_MATCH_MORE_OFFSET);
		flow_entry->udf0_en = entry->bf.udf0_valid;
		flow_entry->udf1_en = entry->bf.udf1_valid;
	}

	return SW_OK;
}
#endif


sw_error_t adpt_ppe_flow_ipv4_5tuple_convert(fal_flow_entry_t *flow_entry,
		union in_flow_tbl_u *entry, a_bool_t to_hsl)
{
	if (to_hsl) {

		entry->bf.src_l3_if_valid = flow_entry->src_intf_valid;
		entry->bf.src_l3_if = flow_entry->src_intf_index;
		entry->bf.age = flow_entry->age;
#if defined(JHPPE)
		entry->bf.fwd_type_0 = flow_entry->fwd_type;
		entry->bf.fwd_type_1 = flow_entry->fwd_type >> SW_FIELD_OFFSET_IN_WORD(IN_FLOW_TBL_FWD_TYPE_OFFSET);
		entry->bf.pri_profile = flow_entry->pri_profile;
		entry->bf.counter_id = flow_entry->sampling_id;
#else
		entry->bf.fwd_type = flow_entry->fwd_type;
		entry->bf.pri_profile_0 = flow_entry->pri_profile;
		entry->bf.pri_profile_1 = flow_entry->pri_profile >> SW_FIELD_OFFSET_IN_WORD(IN_FLOW_TBL_PRI_PROFILE_OFFSET);
#endif
		if (flow_entry->fwd_type == FAL_FLOW_SNAT) {
			entry->bf.next_hop1 = flow_entry->snat_nexthop;
			entry->bf.l4_port1 = flow_entry->snat_srcport;
		} else if (flow_entry->fwd_type == FAL_FLOW_DNAT) {
			entry->bf1.next_hop2 = flow_entry->dnat_nexthop;
			entry->bf1.l4_port2 = flow_entry->dnat_dstport;
		} else if (flow_entry->fwd_type == FAL_FLOW_ROUTE) {
			entry->bf2.next_hop3 = flow_entry->route_nexthop;
			entry->bf2.port_vp_valid1= flow_entry->port_valid;
			entry->bf2.port_vp1 = flow_entry->route_port;
#if defined(JHPPE)
			entry->bf2.nat_action1 = flow_entry->nat_action;
#endif
		} else if (flow_entry->fwd_type == FAL_FLOW_BRIDGE) {
			entry->bf3.port_vp2 = flow_entry->bridge_port;
			entry->bf3.vlan_fmt_valid = flow_entry->vlan_fmt_valid;
			entry->bf3.svlan_fmt = flow_entry->svlan_fmt;
			entry->bf3.cvlan_fmt = flow_entry->cvlan_fmt;
#if defined(MPPE)
			entry->bf3.next_hop4_valid = flow_entry->bridge_nexthop_valid;
			entry->bf3.next_hop4 = flow_entry->bridge_nexthop;
#endif
		}
		entry->bf.de_acce = flow_entry->deacclr_en;
		entry->bf.copy_to_cpu_en = flow_entry->copy_tocpu_en;
		entry->bf.syn_toggle = flow_entry->syn_toggle;
		entry->bf.service_code = flow_entry->sevice_code;
		entry->bf.pmtu_0 = flow_entry->pmtu;
		entry->bf.pmtu_1 = flow_entry->pmtu >> SW_FIELD_OFFSET_IN_WORD(IN_FLOW_TBL_PMTU_OFFSET);
		entry->bf.pmtu_check_type = flow_entry->pmtu_check_l3;
	} else {

		flow_entry->src_intf_valid = entry->bf.src_l3_if_valid;
		flow_entry->src_intf_index = entry->bf.src_l3_if;
		flow_entry->age = entry->bf.age;
#if defined(JHPPE)
		flow_entry->fwd_type = entry->bf.fwd_type_0;
		flow_entry->fwd_type |= entry->bf.fwd_type_1 << SW_FIELD_OFFSET_IN_WORD(IN_FLOW_TBL_FWD_TYPE_OFFSET);
		flow_entry->pri_profile = entry->bf.pri_profile;
		flow_entry->sampling_id = entry->bf.counter_id;
#else
		flow_entry->fwd_type = entry->bf.fwd_type;
		flow_entry->pri_profile = entry->bf.pri_profile_0;
		flow_entry->pri_profile |= entry->bf.pri_profile_1 << SW_FIELD_OFFSET_IN_WORD(IN_FLOW_TBL_PRI_PROFILE_OFFSET);
#endif
		if (flow_entry->fwd_type == FAL_FLOW_SNAT) {
			flow_entry->snat_nexthop = entry->bf.next_hop1;
			flow_entry->snat_srcport = entry->bf.l4_port1;
		} else if (flow_entry->fwd_type == FAL_FLOW_DNAT) {
			flow_entry->dnat_nexthop = entry->bf1.next_hop2;
			flow_entry->dnat_dstport = entry->bf1.l4_port2;
		} else if (flow_entry->fwd_type == FAL_FLOW_ROUTE) {
			flow_entry->route_nexthop = entry->bf2.next_hop3;
			flow_entry->port_valid = entry->bf2.port_vp_valid1;
			flow_entry->route_port = entry->bf2.port_vp1;
#if defined(JHPPE)
			flow_entry->nat_action = entry->bf2.nat_action1;
#endif
		} else if (flow_entry->fwd_type == FAL_FLOW_BRIDGE) {
			flow_entry->bridge_port = entry->bf3.port_vp2;
			flow_entry->vlan_fmt_valid = entry->bf3.vlan_fmt_valid;
			flow_entry->svlan_fmt = entry->bf3.svlan_fmt;
			flow_entry->cvlan_fmt = entry->bf3.cvlan_fmt;
#if defined(MPPE)
			flow_entry->bridge_nexthop_valid = entry->bf3.next_hop4_valid;
			flow_entry->bridge_nexthop = entry->bf3.next_hop4;
#endif
		}
		flow_entry->deacclr_en = entry->bf.de_acce;
		flow_entry->copy_tocpu_en = entry->bf.copy_to_cpu_en;
		flow_entry->syn_toggle = entry->bf.syn_toggle;
		flow_entry->sevice_code = entry->bf.service_code;
		flow_entry->pmtu = entry->bf.pmtu_0;
		flow_entry->pmtu |= entry->bf.pmtu_1 << SW_FIELD_OFFSET_IN_WORD(IN_FLOW_TBL_PMTU_OFFSET);
		flow_entry->pmtu_check_l3 = entry->bf.pmtu_check_type;
	}

	return SW_OK;
}

sw_error_t adpt_ppe_flow_ipv6_5tuple_convert(fal_flow_entry_t *flow_entry,
		union in_flow_ipv6_5tuple_tbl_u *entry, a_bool_t to_hsl)
{
	if (to_hsl) {
		entry->bf.age = flow_entry->age;
		entry->bf.src_l3_if_valid = flow_entry->src_intf_valid;
		entry->bf.src_l3_if = flow_entry->src_intf_index;
#if defined(JHPPE)
		entry->bf.fwd_type_0 = flow_entry->fwd_type;
		entry->bf.fwd_type_1 = flow_entry->fwd_type >> SW_FIELD_OFFSET_IN_WORD(IN_FLOW_IPV6_5TUPLE_TBL_FWD_TYPE_OFFSET);
		entry->bf.pri_profile = flow_entry->pri_profile;
		entry->bf.counter_id_0 = flow_entry->sampling_id;
		entry->bf.counter_id_1 = flow_entry->sampling_id >> SW_FIELD_OFFSET_IN_WORD(IN_FLOW_IPV6_5TUPLE_TBL_COUNTER_ID_OFFSET);
#else
		entry->bf.fwd_type = flow_entry->fwd_type;
		entry->bf.pri_profile_0 = flow_entry->pri_profile;
		entry->bf.pri_profile_1 = flow_entry->pri_profile >> SW_FIELD_OFFSET_IN_WORD(IN_FLOW_IPV6_5TUPLE_TBL_PRI_PROFILE_OFFSET);
#endif
		if (flow_entry->fwd_type == FAL_FLOW_SNAT) {
			entry->bf.next_hop1 = flow_entry->snat_nexthop;
			entry->bf.l4_port1 = flow_entry->snat_srcport;
		} else if (flow_entry->fwd_type == FAL_FLOW_DNAT) {
			entry->bf1.next_hop2 = flow_entry->dnat_nexthop;
			entry->bf1.l4_port2 = flow_entry->dnat_dstport;
		} else if (flow_entry->fwd_type == FAL_FLOW_ROUTE) {
			entry->bf2.next_hop3 = flow_entry->route_nexthop;
			entry->bf2.port_vp_valid1= flow_entry->port_valid;
			entry->bf2.port_vp1 = flow_entry->route_port;
#if defined(JHPPE)
			entry->bf2.nat_action1 = flow_entry->nat_action;
#endif
		} else if (flow_entry->fwd_type == FAL_FLOW_BRIDGE) {
			entry->bf3.port_vp2 = flow_entry->bridge_port;
			entry->bf3.vlan_fmt_valid = flow_entry->vlan_fmt_valid;
			entry->bf3.svlan_fmt = flow_entry->svlan_fmt;
			entry->bf3.cvlan_fmt = flow_entry->cvlan_fmt;
#if defined(MPPE)
			entry->bf3.next_hop4_valid = flow_entry->bridge_nexthop_valid;
			entry->bf3.next_hop4 = flow_entry->bridge_nexthop;
#endif
		}
		entry->bf.de_acce = flow_entry->deacclr_en;
		entry->bf.copy_to_cpu_en = flow_entry->copy_tocpu_en;
		entry->bf.syn_toggle = flow_entry->syn_toggle;
		entry->bf.service_code = flow_entry->sevice_code;
		entry->bf.pmtu_0 = flow_entry->pmtu;
		entry->bf.pmtu_1 = flow_entry->pmtu >> SW_FIELD_OFFSET_IN_WORD(IN_FLOW_IPV6_5TUPLE_TBL_PMTU_OFFSET);
		entry->bf.pmtu_check_type = flow_entry->pmtu_check_l3;
	} else {
		flow_entry->age = entry->bf.age;
		flow_entry->src_intf_valid = entry->bf.src_l3_if_valid;
		flow_entry->src_intf_index = entry->bf.src_l3_if;
#if defined(JHPPE)
		flow_entry->fwd_type = entry->bf.fwd_type_0;
		flow_entry->fwd_type |= entry->bf.fwd_type_1 << SW_FIELD_OFFSET_IN_WORD(IN_FLOW_IPV6_5TUPLE_TBL_FWD_TYPE_OFFSET);
		flow_entry->pri_profile = entry->bf.pri_profile;
		flow_entry->sampling_id = entry->bf.counter_id_0;
		flow_entry->sampling_id |= entry->bf.counter_id_1 << SW_FIELD_OFFSET_IN_WORD(IN_FLOW_IPV6_5TUPLE_TBL_COUNTER_ID_OFFSET);
#else
		flow_entry->fwd_type = entry->bf.fwd_type;
		flow_entry->pri_profile = entry->bf.pri_profile_0;
		flow_entry->pri_profile |= entry->bf.pri_profile_1 << SW_FIELD_OFFSET_IN_WORD(IN_FLOW_IPV6_5TUPLE_TBL_PRI_PROFILE_OFFSET);
#endif
		if (flow_entry->fwd_type == FAL_FLOW_SNAT) {
			flow_entry->snat_nexthop = entry->bf.next_hop1;
			flow_entry->snat_srcport = entry->bf.l4_port1;
		} else if (flow_entry->fwd_type == FAL_FLOW_DNAT) {
			flow_entry->dnat_nexthop = entry->bf1.next_hop2;
			flow_entry->dnat_dstport = entry->bf1.l4_port2;
		} else if (flow_entry->fwd_type == FAL_FLOW_ROUTE) {
			flow_entry->route_nexthop = entry->bf2.next_hop3;
			flow_entry->port_valid = entry->bf2.port_vp_valid1;
			flow_entry->route_port = entry->bf2.port_vp1;
#if defined(JHPPE)
			flow_entry->nat_action = entry->bf2.nat_action1;
#endif
		} else if (flow_entry->fwd_type == FAL_FLOW_BRIDGE) {
			flow_entry->bridge_port = entry->bf3.port_vp2;
			flow_entry->vlan_fmt_valid = entry->bf3.vlan_fmt_valid;
			flow_entry->svlan_fmt = entry->bf3.svlan_fmt;
			flow_entry->cvlan_fmt = entry->bf3.cvlan_fmt;
#if defined(MPPE)
			flow_entry->bridge_nexthop_valid = entry->bf3.next_hop4_valid;
			flow_entry->bridge_nexthop = entry->bf3.next_hop4;
#endif
		}
		flow_entry->deacclr_en = entry->bf.de_acce;
		flow_entry->copy_tocpu_en = entry->bf.copy_to_cpu_en;
		flow_entry->syn_toggle = entry->bf.syn_toggle;
		flow_entry->sevice_code = entry->bf.service_code;
		flow_entry->pmtu = entry->bf.pmtu_0;
		flow_entry->pmtu |= entry->bf.pmtu_1 << SW_FIELD_OFFSET_IN_WORD(IN_FLOW_IPV6_5TUPLE_TBL_PMTU_OFFSET);
		flow_entry->pmtu_check_l3 = entry->bf.pmtu_check_type;
	}

	return SW_OK;
}

sw_error_t adpt_ppe_flow_ipv4_3tuple_convert(fal_flow_entry_t *flow_entry,
		union in_flow_3tuple_tbl_u *entry, a_bool_t to_hsl)
{
	if (to_hsl) {
		entry->bf.age = flow_entry->age;
		entry->bf.src_l3_if_valid = flow_entry->src_intf_valid;
		entry->bf.src_l3_if = flow_entry->src_intf_index;
#if defined(JHPPE)
		entry->bf.fwd_type_0 = flow_entry->fwd_type;
		entry->bf.fwd_type_1 = flow_entry->fwd_type >> SW_FIELD_OFFSET_IN_WORD(IN_FLOW_3TUPLE_TBL_FWD_TYPE_OFFSET);
		entry->bf.pri_profile = flow_entry->pri_profile;
		entry->bf.counter_id = flow_entry->sampling_id;
#else
		entry->bf.fwd_type = flow_entry->fwd_type;
		entry->bf.pri_profile_0 = flow_entry->pri_profile;
		entry->bf.pri_profile_1 = flow_entry->pri_profile >> SW_FIELD_OFFSET_IN_WORD(IN_FLOW_3TUPLE_TBL_PRI_PROFILE_OFFSET);
#endif
		if (flow_entry->fwd_type == FAL_FLOW_SNAT) {
			entry->bf.next_hop1 = flow_entry->snat_nexthop;
		} else if (flow_entry->fwd_type == FAL_FLOW_DNAT) {
			entry->bf1.next_hop2 = flow_entry->dnat_nexthop;
		} else if (flow_entry->fwd_type == FAL_FLOW_ROUTE) {
			entry->bf2.next_hop3 = flow_entry->route_nexthop;
			entry->bf2.port_vp_valid1= flow_entry->port_valid;
			entry->bf2.port_vp1 = flow_entry->route_port;
#if defined(JHPPE)
			entry->bf2.nat_action1 = flow_entry->nat_action;
#endif
		} else if (flow_entry->fwd_type == FAL_FLOW_BRIDGE) {
			entry->bf3.port_vp2 = flow_entry->bridge_port;
			entry->bf3.vlan_fmt_valid = flow_entry->vlan_fmt_valid;
			entry->bf3.svlan_fmt = flow_entry->svlan_fmt;
			entry->bf3.cvlan_fmt = flow_entry->cvlan_fmt;
#if defined(MPPE)
			entry->bf3.next_hop4_valid = flow_entry->bridge_nexthop_valid;
			entry->bf3.next_hop4 = flow_entry->bridge_nexthop;
#endif
		}
		entry->bf.de_acce = flow_entry->deacclr_en;
		entry->bf.copy_to_cpu_en = flow_entry->copy_tocpu_en;
		entry->bf.syn_toggle = flow_entry->syn_toggle;
		entry->bf.service_code = flow_entry->sevice_code;
#if defined(JHPPE)
		entry->bf.pmtu = flow_entry->pmtu;
#else
		entry->bf.pmtu_0 = flow_entry->pmtu;
		entry->bf.pmtu_1 = flow_entry->pmtu >> SW_FIELD_OFFSET_IN_WORD(IN_FLOW_3TUPLE_TBL_PMTU_OFFSET);
#endif
		entry->bf.pmtu_check_type = flow_entry->pmtu_check_l3;
	} else {
		flow_entry->age = entry->bf.age;
		flow_entry->src_intf_valid = entry->bf.src_l3_if_valid;
		flow_entry->src_intf_index = entry->bf.src_l3_if;
#if defined(JHPPE)
		flow_entry->fwd_type = entry->bf.fwd_type_0;
		flow_entry->fwd_type |= entry->bf.fwd_type_1 << SW_FIELD_OFFSET_IN_WORD(IN_FLOW_3TUPLE_TBL_FWD_TYPE_OFFSET);
		flow_entry->pri_profile = entry->bf.pri_profile;
		flow_entry->sampling_id = entry->bf.counter_id;
#else
		flow_entry->fwd_type = entry->bf.fwd_type;
		flow_entry->pri_profile = entry->bf.pri_profile_0;
		flow_entry->pri_profile |= entry->bf.pri_profile_1 << SW_FIELD_OFFSET_IN_WORD(IN_FLOW_3TUPLE_TBL_PRI_PROFILE_OFFSET);
#endif
		if (flow_entry->fwd_type == FAL_FLOW_SNAT) {
			flow_entry->snat_nexthop = entry->bf.next_hop1;
		} else if (flow_entry->fwd_type == FAL_FLOW_DNAT) {
			flow_entry->dnat_nexthop = entry->bf1.next_hop2;
		} else if (flow_entry->fwd_type == FAL_FLOW_ROUTE) {
			flow_entry->route_nexthop = entry->bf2.next_hop3;
			flow_entry->port_valid = entry->bf2.port_vp_valid1;
			flow_entry->route_port = entry->bf2.port_vp1;
#if defined(JHPPE)
			flow_entry->nat_action = entry->bf2.nat_action1;
#endif
		} else if (flow_entry->fwd_type == FAL_FLOW_BRIDGE) {
			flow_entry->bridge_port = entry->bf3.port_vp2;
			flow_entry->vlan_fmt_valid = entry->bf3.vlan_fmt_valid;
			flow_entry->svlan_fmt = entry->bf3.svlan_fmt;
			flow_entry->cvlan_fmt = entry->bf3.cvlan_fmt;
#if defined(MPPE)
			flow_entry->bridge_nexthop_valid = entry->bf3.next_hop4_valid;
			flow_entry->bridge_nexthop = entry->bf3.next_hop4;
#endif
		}
		flow_entry->deacclr_en = entry->bf.de_acce;
		flow_entry->copy_tocpu_en = entry->bf.copy_to_cpu_en;
		flow_entry->syn_toggle = entry->bf.syn_toggle;
		flow_entry->sevice_code = entry->bf.service_code;
		flow_entry->pmtu_check_l3 = entry->bf.pmtu_check_type;
#if defined(JHPPE)
		flow_entry->pmtu = entry->bf.pmtu;
#else
		flow_entry->pmtu = entry->bf.pmtu_0;
		flow_entry->pmtu |= entry->bf.pmtu_1 << SW_FIELD_OFFSET_IN_WORD(IN_FLOW_3TUPLE_TBL_PMTU_OFFSET);
#endif
	}

	return SW_OK;
}

sw_error_t adpt_ppe_flow_ipv6_3tuple_convert(fal_flow_entry_t *flow_entry,
		union in_flow_ipv6_3tuple_tbl_u *entry, a_bool_t to_hsl)
{
	if (to_hsl) {
		entry->bf.age = flow_entry->age;
		entry->bf.src_l3_if_valid = flow_entry->src_intf_valid;
		entry->bf.src_l3_if = flow_entry->src_intf_index;
#if defined(JHPPE)
		entry->bf.fwd_type_0 = flow_entry->fwd_type;
		entry->bf.fwd_type_1 = flow_entry->fwd_type >> SW_FIELD_OFFSET_IN_WORD(IN_FLOW_IPV6_3TUPLE_TBL_FWD_TYPE_OFFSET);
		entry->bf.pri_profile = flow_entry->pri_profile;
		entry->bf.counter_id_0 = flow_entry->sampling_id;
		entry->bf.counter_id_1 = flow_entry->sampling_id >> SW_FIELD_OFFSET_IN_WORD(IN_FLOW_IPV6_3TUPLE_TBL_COUNTER_ID_OFFSET);
#else
		entry->bf.fwd_type = flow_entry->fwd_type;
		entry->bf.pri_profile_0 = flow_entry->pri_profile;
		entry->bf.pri_profile_1 = flow_entry->pri_profile >> SW_FIELD_OFFSET_IN_WORD(IN_FLOW_IPV6_3TUPLE_TBL_PRI_PROFILE_OFFSET);
#endif
		if (flow_entry->fwd_type == FAL_FLOW_SNAT) {
			entry->bf.next_hop1 = flow_entry->snat_nexthop;
		} else if (flow_entry->fwd_type == FAL_FLOW_DNAT) {
			entry->bf1.next_hop2 = flow_entry->dnat_nexthop;
		} else if (flow_entry->fwd_type == FAL_FLOW_ROUTE) {
			entry->bf2.next_hop3 = flow_entry->route_nexthop;
			entry->bf2.port_vp_valid1= flow_entry->port_valid;
			entry->bf2.port_vp1 = flow_entry->route_port;
#if defined(JHPPE)
			entry->bf2.nat_action1 = flow_entry->nat_action;
#endif
		} else if (flow_entry->fwd_type == FAL_FLOW_BRIDGE) {
			entry->bf3.port_vp2 = flow_entry->bridge_port;
			entry->bf3.vlan_fmt_valid = flow_entry->vlan_fmt_valid;
			entry->bf3.svlan_fmt = flow_entry->svlan_fmt;
			entry->bf3.cvlan_fmt = flow_entry->cvlan_fmt;
#if defined(MPPE)
			entry->bf3.next_hop4_valid = flow_entry->bridge_nexthop_valid;
			entry->bf3.next_hop4 = flow_entry->bridge_nexthop;
#endif
		}
		entry->bf.de_acce = flow_entry->deacclr_en;
		entry->bf.copy_to_cpu_en = flow_entry->copy_tocpu_en;
		entry->bf.syn_toggle = flow_entry->syn_toggle;
		entry->bf.service_code = flow_entry->sevice_code;
		entry->bf.pmtu_0 = flow_entry->pmtu;
		entry->bf.pmtu_1 = flow_entry->pmtu >> SW_FIELD_OFFSET_IN_WORD(IN_FLOW_IPV6_3TUPLE_TBL_PMTU_OFFSET);
		entry->bf.pmtu_check_type = flow_entry->pmtu_check_l3;
	} else {
		flow_entry->age = entry->bf.age;
		flow_entry->src_intf_valid = entry->bf.src_l3_if_valid;
		flow_entry->src_intf_index = entry->bf.src_l3_if;
#if defined(JHPPE)
		flow_entry->fwd_type = entry->bf.fwd_type_0;
		flow_entry->fwd_type |= entry->bf.fwd_type_1 << SW_FIELD_OFFSET_IN_WORD(IN_FLOW_IPV6_3TUPLE_TBL_FWD_TYPE_OFFSET);
		flow_entry->pri_profile = entry->bf.pri_profile;
		flow_entry->sampling_id = entry->bf.counter_id_0;
		flow_entry->sampling_id |= entry->bf.counter_id_1 << SW_FIELD_OFFSET_IN_WORD(IN_FLOW_IPV6_3TUPLE_TBL_COUNTER_ID_OFFSET);
#else
		flow_entry->fwd_type = entry->bf.fwd_type;
		flow_entry->pri_profile = entry->bf.pri_profile_0;
		flow_entry->pri_profile |= entry->bf.pri_profile_1 << SW_FIELD_OFFSET_IN_WORD(IN_FLOW_IPV6_3TUPLE_TBL_PRI_PROFILE_OFFSET);
#endif
		if (flow_entry->fwd_type == FAL_FLOW_SNAT) {
			flow_entry->snat_nexthop = entry->bf.next_hop1;
		} else if (flow_entry->fwd_type == FAL_FLOW_DNAT) {
			flow_entry->dnat_nexthop = entry->bf1.next_hop2;
		} else if (flow_entry->fwd_type == FAL_FLOW_ROUTE) {
			flow_entry->route_nexthop = entry->bf2.next_hop3;
			flow_entry->port_valid = entry->bf2.port_vp_valid1;
			flow_entry->route_port = entry->bf2.port_vp1;
#if defined(JHPPE)
			flow_entry->nat_action = entry->bf2.nat_action1;
#endif
		} else if (flow_entry->fwd_type == FAL_FLOW_BRIDGE) {
			flow_entry->bridge_port = entry->bf3.port_vp2;
			flow_entry->vlan_fmt_valid = entry->bf3.vlan_fmt_valid;
			flow_entry->svlan_fmt = entry->bf3.svlan_fmt;
			flow_entry->cvlan_fmt = entry->bf3.cvlan_fmt;
#if defined(MPPE)
			flow_entry->bridge_nexthop_valid = entry->bf3.next_hop4_valid;
			flow_entry->bridge_nexthop = entry->bf3.next_hop4;
#endif
		}
		flow_entry->deacclr_en = entry->bf.de_acce;
		flow_entry->copy_tocpu_en = entry->bf.copy_to_cpu_en;
		flow_entry->syn_toggle = entry->bf.syn_toggle;
		flow_entry->sevice_code = entry->bf.service_code;
		flow_entry->pmtu = entry->bf.pmtu_0;
		flow_entry->pmtu |= entry->bf.pmtu_1 << SW_FIELD_OFFSET_IN_WORD(IN_FLOW_IPV6_3TUPLE_TBL_PMTU_OFFSET);
		flow_entry->pmtu_check_l3 = entry->bf.pmtu_check_type;
	}

	return SW_OK;
}

#if defined(JHPPE)
sw_error_t adpt_ppe_flow_ip_6tuple_convert(fal_flow_entry_t *flow_entry,
		union in_flow_6tuple_tbl_u *entry, a_bool_t to_hsl)
{
	if (to_hsl) {
		entry->bf.age = flow_entry->age;
		entry->bf.src_l3_if_valid = flow_entry->src_intf_valid;
		entry->bf.src_l3_if = flow_entry->src_intf_index;
		entry->bf.fwd_type_0 = flow_entry->fwd_type;
		entry->bf.fwd_type_1 = flow_entry->fwd_type >> SW_FIELD_OFFSET_IN_WORD(IN_FLOW_6TUPLE_TBL_FWD_TYPE_OFFSET);
		if (flow_entry->fwd_type == FAL_FLOW_SNAT) {
			entry->bf.next_hop1 = flow_entry->snat_nexthop;
			entry->bf.l4_port1 = flow_entry->snat_srcport;
		} else if (flow_entry->fwd_type == FAL_FLOW_DNAT) {
			entry->bf1.next_hop2 = flow_entry->dnat_nexthop;
			entry->bf1.l4_port2 = flow_entry->dnat_dstport;
		} else if (flow_entry->fwd_type == FAL_FLOW_ROUTE) {
			entry->bf2.next_hop3 = flow_entry->route_nexthop;
			entry->bf2.port_vp_valid1= flow_entry->port_valid;
			entry->bf2.port_vp1 = flow_entry->route_port;
			entry->bf2.nat_action1 = flow_entry->nat_action;
		} else if (flow_entry->fwd_type == FAL_FLOW_BRIDGE) {
			entry->bf3.port_vp2 = flow_entry->bridge_port;
			entry->bf3.vlan_fmt_valid = flow_entry->vlan_fmt_valid;
			entry->bf3.svlan_fmt = flow_entry->svlan_fmt;
			entry->bf3.cvlan_fmt = flow_entry->cvlan_fmt;
			entry->bf3.next_hop4_valid = flow_entry->bridge_nexthop_valid;
			entry->bf3.next_hop4 = flow_entry->bridge_nexthop;
		}
		entry->bf.de_acce = flow_entry->deacclr_en;
		entry->bf.copy_to_cpu_en = flow_entry->copy_tocpu_en;
		entry->bf.syn_toggle = flow_entry->syn_toggle;
		entry->bf.pri_profile = flow_entry->pri_profile;
		entry->bf.service_code = flow_entry->sevice_code;

		entry->bf.pmtu_check_type = flow_entry->pmtu_check_l3;
		entry->bf.pmtu = flow_entry->pmtu;
		entry->bf.counter_id_0 = flow_entry->sampling_id;
		entry->bf.counter_id_1 = flow_entry->sampling_id >> SW_FIELD_OFFSET_IN_WORD(IN_FLOW_6TUPLE_TBL_COUNTER_ID_OFFSET);
	} else {
		flow_entry->age = entry->bf.age;
		flow_entry->src_intf_valid = entry->bf.src_l3_if_valid;
		flow_entry->src_intf_index = entry->bf.src_l3_if;
		flow_entry->fwd_type = entry->bf.fwd_type_0;
		flow_entry->fwd_type |= entry->bf.fwd_type_1 << SW_FIELD_OFFSET_IN_WORD(IN_FLOW_6TUPLE_TBL_FWD_TYPE_OFFSET);

		if (flow_entry->fwd_type == FAL_FLOW_SNAT) {
			flow_entry->snat_nexthop = entry->bf.next_hop1;
			flow_entry->snat_srcport = entry->bf.l4_port1;
		} else if (flow_entry->fwd_type == FAL_FLOW_DNAT) {
			flow_entry->dnat_nexthop = entry->bf1.next_hop2;
			flow_entry->dnat_dstport = entry->bf1.l4_port2;
		} else if (flow_entry->fwd_type == FAL_FLOW_ROUTE) {
			flow_entry->route_nexthop = entry->bf2.next_hop3;
			flow_entry->port_valid = entry->bf2.port_vp_valid1;
			flow_entry->route_port = entry->bf2.port_vp1;
			flow_entry->nat_action = entry->bf2.nat_action1;
		} else if (flow_entry->fwd_type == FAL_FLOW_BRIDGE) {
			flow_entry->bridge_port = entry->bf3.port_vp2;
			flow_entry->vlan_fmt_valid = entry->bf3.vlan_fmt_valid;
			flow_entry->svlan_fmt = entry->bf3.svlan_fmt;
			flow_entry->cvlan_fmt = entry->bf3.cvlan_fmt;
			flow_entry->bridge_nexthop_valid = entry->bf3.next_hop4_valid;
			flow_entry->bridge_nexthop = entry->bf3.next_hop4;
		}
		flow_entry->deacclr_en = entry->bf.de_acce;
		flow_entry->copy_tocpu_en = entry->bf.copy_to_cpu_en;
		flow_entry->syn_toggle = entry->bf.syn_toggle;
		flow_entry->pri_profile = entry->bf.pri_profile;
		flow_entry->sevice_code = entry->bf.service_code;

		flow_entry->pmtu_check_l3 = entry->bf.pmtu_check_type;
		flow_entry->pmtu = entry->bf.pmtu;
		flow_entry->sampling_id = entry->bf.counter_id_0;
		flow_entry->sampling_id |= entry->bf.counter_id_1 << SW_FIELD_OFFSET_IN_WORD(IN_FLOW_6TUPLE_TBL_COUNTER_ID_OFFSET);
	}

	return SW_OK;
}
#endif

sw_error_t
adpt_hppe_flow_entry_host_op_add(
		a_uint32_t dev_id,
		a_uint32_t add_mode, /*index or hash*/
		fal_flow_entry_t *flow_entry)
{
	sw_error_t rv = SW_OK;
	a_uint32_t type = 0;
	ADPT_DEV_ID_CHECK(dev_id);
	ADPT_NULL_POINT_CHECK(flow_entry);

	type = flow_entry->entry_type;
	if ((type & FAL_FLOW_IP4_5TUPLE_ADDR) == FAL_FLOW_IP4_5TUPLE_ADDR) {
		union in_flow_tbl_u entry;
		aos_mem_zero(&entry, sizeof(entry));

		adpt_ppe_flow_key_ipv4_5tuple_convert(flow_entry, &entry, A_TRUE);
		adpt_ppe_flow_ipv4_5tuple_convert(flow_entry, &entry, A_TRUE);

		rv = hppe_flow_entry_host_op_ipv4_5tuple_add(dev_id, (a_uint32_t)add_mode, &flow_entry->entry_id, &entry);
	} else if ((type & FAL_FLOW_IP6_5TUPLE_ADDR) == FAL_FLOW_IP6_5TUPLE_ADDR) {
		union in_flow_ipv6_5tuple_tbl_u entry;
		aos_mem_zero(&entry, sizeof(entry));

		adpt_ppe_flow_key_ipv6_5tuple_convert(flow_entry, &entry, A_TRUE);
		adpt_ppe_flow_ipv6_5tuple_convert(flow_entry, &entry, A_TRUE);

		rv = hppe_flow_entry_host_op_ipv6_5tuple_add(dev_id, (a_uint32_t)add_mode, &flow_entry->entry_id, &entry);
	} else if ((type & FAL_FLOW_IP4_3TUPLE_ADDR) == FAL_FLOW_IP4_3TUPLE_ADDR) {
		union in_flow_3tuple_tbl_u entry;
		aos_mem_zero(&entry, sizeof(entry));

		adpt_ppe_flow_key_ipv4_3tuple_convert(flow_entry, &entry, A_TRUE);
		adpt_ppe_flow_ipv4_3tuple_convert(flow_entry, &entry, A_TRUE);

		rv = hppe_flow_entry_host_op_ipv4_3tuple_add(dev_id, (a_uint32_t)add_mode, &flow_entry->entry_id, &entry);
	} else if ((type & FAL_FLOW_IP6_3TUPLE_ADDR) == FAL_FLOW_IP6_3TUPLE_ADDR) {
		union in_flow_ipv6_3tuple_tbl_u entry;
		aos_mem_zero(&entry, sizeof(entry));

		adpt_ppe_flow_key_ipv6_3tuple_convert(flow_entry, &entry, A_TRUE);
		adpt_ppe_flow_ipv6_3tuple_convert(flow_entry, &entry, A_TRUE);

		rv = hppe_flow_entry_host_op_ipv6_3tuple_add(dev_id, (a_uint32_t)add_mode, &flow_entry->entry_id, &entry);
	}
#if defined(JHPPE)
	else if ((type & FAL_FLOW_IP4_6TUPLE_ADDR) || (type & FAL_FLOW_IP6_6TUPLE_ADDR)) {
		/* entry_type is not checked by hardware, which means IPv6 and IPv4 supported. */
		union in_flow_6tuple_tbl_u entry;
		aos_mem_zero(&entry, sizeof(entry));

		adpt_ppe_flow_key_ip_6tuple_convert(flow_entry, &entry, type, A_TRUE);
		adpt_ppe_flow_ip_6tuple_convert(flow_entry, &entry, A_TRUE);

		rv = jhppe_flow_entry_host_op_ip_6tuple_add(dev_id, (a_uint32_t)add_mode, &flow_entry->entry_id, &entry);
	}
#endif
	else
		return SW_FAIL;

	if (rv != SW_OK)
		return rv;

#if defined(MPPE)
	rv = mppe_qos_mapping_tbl_flow_policer_set(dev_id, flow_entry->entry_id,
			flow_entry->policer_valid, flow_entry->policer_index);

	if (rv)
		return rv;
#endif

#if defined(JHPPE)
	rv = adpt_jhppe_flow_gro_en_set(dev_id, flow_entry->entry_id, flow_entry->gro_en);
	if (rv)
		return rv;
#endif

	return adpt_hppe_flow_qos_set(dev_id, flow_entry->entry_id, &(flow_entry->flow_qos));
}

sw_error_t
adpt_hppe_flow_entry_host_op_get(
		a_uint32_t dev_id,
		a_uint32_t get_mode,
		fal_flow_entry_t *flow_entry)
{
	sw_error_t rv = SW_OK;
	a_uint32_t type = 0;
	a_uint32_t entry_id = flow_entry->entry_id;
	union in_flow_cnt_tbl_u cnt;

	ADPT_DEV_ID_CHECK(dev_id);
	ADPT_NULL_POINT_CHECK(flow_entry);

	aos_mem_zero(&cnt, sizeof(cnt));

	type = flow_entry->entry_type;
	if ((type & FAL_FLOW_IP4_5TUPLE_ADDR) == FAL_FLOW_IP4_5TUPLE_ADDR) {
		union in_flow_tbl_u entry;
		aos_mem_zero(&entry, sizeof(entry));

		adpt_ppe_flow_key_ipv4_5tuple_convert(flow_entry, &entry, A_TRUE);
		rv = hppe_flow_entry_host_op_ipv4_5tuple_get(dev_id, get_mode, &entry_id, &entry);

		flow_entry->entry_id = entry_id;
		adpt_ppe_flow_ipv4_5tuple_convert(flow_entry, &entry, A_FALSE);
		adpt_ppe_flow_key_ipv4_5tuple_convert(flow_entry, &entry, A_FALSE);
	} else if ((type & FAL_FLOW_IP6_5TUPLE_ADDR) == FAL_FLOW_IP6_5TUPLE_ADDR) {
		union in_flow_ipv6_5tuple_tbl_u entry;
		aos_mem_zero(&entry, sizeof(entry));

		adpt_ppe_flow_key_ipv6_5tuple_convert(flow_entry, &entry, A_TRUE);
		rv = hppe_flow_entry_host_op_ipv6_5tuple_get(dev_id, get_mode, &entry_id, &entry);

		flow_entry->entry_id = entry_id;
		adpt_ppe_flow_ipv6_5tuple_convert(flow_entry, &entry, A_FALSE);
		adpt_ppe_flow_key_ipv6_5tuple_convert(flow_entry, &entry, A_FALSE);
	} else if ((type & FAL_FLOW_IP4_3TUPLE_ADDR) == FAL_FLOW_IP4_3TUPLE_ADDR) {
		union in_flow_3tuple_tbl_u entry;
		aos_mem_zero(&entry, sizeof(entry));

		adpt_ppe_flow_key_ipv4_3tuple_convert(flow_entry, &entry, A_TRUE);
		rv = hppe_flow_entry_host_op_ipv4_3tuple_get(dev_id, get_mode, &entry_id, &entry);

		flow_entry->entry_id = entry_id;
		adpt_ppe_flow_ipv4_3tuple_convert(flow_entry, &entry, A_FALSE);
		adpt_ppe_flow_key_ipv4_3tuple_convert(flow_entry, &entry, A_FALSE);
	} else if ((type & FAL_FLOW_IP6_3TUPLE_ADDR) == FAL_FLOW_IP6_3TUPLE_ADDR) {
		union in_flow_ipv6_3tuple_tbl_u entry;
		aos_mem_zero(&entry, sizeof(entry));

		adpt_ppe_flow_key_ipv6_3tuple_convert(flow_entry, &entry, A_TRUE);
		rv = hppe_flow_entry_host_op_ipv6_3tuple_get(dev_id, get_mode, &entry_id, &entry);

		flow_entry->entry_id = entry_id;
		adpt_ppe_flow_ipv6_3tuple_convert(flow_entry, &entry, A_FALSE);
		adpt_ppe_flow_key_ipv6_3tuple_convert(flow_entry, &entry, A_FALSE);
	}
#if defined(JHPPE)
	else if ((type & FAL_FLOW_IP4_6TUPLE_ADDR) || (type & FAL_FLOW_IP6_6TUPLE_ADDR)) {
		/* entry_type is not checked by hardware, which means IPv6 and IPv4 supported. */
		union in_flow_6tuple_tbl_u entry;
		aos_mem_zero(&entry, sizeof(entry));

		adpt_ppe_flow_key_ip_6tuple_convert(flow_entry, &entry, type, A_TRUE);
		rv = jhppe_flow_entry_host_op_ip_6tuple_get(dev_id, get_mode, &entry_id, &entry);
		flow_entry->entry_id = entry_id;
		adpt_ppe_flow_ip_6tuple_convert(flow_entry, &entry, A_FALSE);
		adpt_ppe_flow_key_ip_6tuple_convert(flow_entry, &entry, type, A_FALSE);
	}
#endif
	else
		return SW_FAIL;

	if (rv)
		return rv;

	rv = hppe_in_flow_cnt_tbl_get(dev_id, flow_entry->entry_id, &cnt);
	if (rv)
		return rv;

	flow_entry->pkt_counter = cnt.bf.hit_pkt_counter;
	flow_entry->byte_counter = cnt.bf.hit_byte_counter_0 | \
				   ((a_uint64_t)cnt.bf.hit_byte_counter_1 << SW_FIELD_OFFSET_IN_WORD(IN_FLOW_CNT_TBL_HIT_BYTE_COUNTER_OFFSET));

#if defined(MPPE)
	rv = mppe_qos_mapping_tbl_flow_policer_get(dev_id, flow_entry->entry_id,
			&(flow_entry->policer_valid), &(flow_entry->policer_index));
	if (rv)
		return rv;
#endif

#if defined(JHPPE)
	rv = adpt_jhppe_flow_gro_en_get(dev_id, flow_entry->entry_id, &(flow_entry->gro_en));
	if (rv)
		return rv;
#endif

	return adpt_hppe_flow_qos_get(dev_id, flow_entry->entry_id, &(flow_entry->flow_qos));
}

sw_error_t
adpt_hppe_flow_entry_host_op_del(
		a_uint32_t dev_id,
		a_uint32_t del_mode,
		fal_flow_entry_t *flow_entry)
{
	sw_error_t rv = SW_OK;
	a_uint32_t type = 0;
	ADPT_DEV_ID_CHECK(dev_id);
	ADPT_NULL_POINT_CHECK(flow_entry);

	if (del_mode == FAL_FLOW_OP_MODE_FLUSH)
		return hppe_flow_host_flush_common(dev_id);
		//return hppe_flow_flush_common(dev_id);

	type = flow_entry->entry_type;
	if ((type & FAL_FLOW_IP4_5TUPLE_ADDR) == FAL_FLOW_IP4_5TUPLE_ADDR) {
		union in_flow_tbl_u entry;
		aos_mem_zero(&entry, sizeof(entry));

		adpt_ppe_flow_key_ipv4_5tuple_convert(flow_entry, &entry, A_TRUE);

		rv = hppe_flow_entry_host_op_ipv4_5tuple_del(dev_id, del_mode, &flow_entry->entry_id, &entry);
	} else if ((type & FAL_FLOW_IP6_5TUPLE_ADDR) == FAL_FLOW_IP6_5TUPLE_ADDR) {
		union in_flow_ipv6_5tuple_tbl_u entry;
		aos_mem_zero(&entry, sizeof(entry));

		adpt_ppe_flow_key_ipv6_5tuple_convert(flow_entry, &entry, A_TRUE);

		rv = hppe_flow_entry_host_op_ipv6_5tuple_del(dev_id, del_mode, &flow_entry->entry_id, &entry);
	} else if ((type & FAL_FLOW_IP4_3TUPLE_ADDR) == FAL_FLOW_IP4_3TUPLE_ADDR) {
		union in_flow_3tuple_tbl_u entry;
		aos_mem_zero(&entry, sizeof(entry));

		adpt_ppe_flow_key_ipv4_3tuple_convert(flow_entry, &entry, A_TRUE);

		rv = hppe_flow_entry_host_op_ipv4_3tuple_del(dev_id, del_mode, &flow_entry->entry_id, &entry);
	} else if ((type & FAL_FLOW_IP6_3TUPLE_ADDR) == FAL_FLOW_IP6_3TUPLE_ADDR) {
		union in_flow_ipv6_3tuple_tbl_u entry;
		aos_mem_zero(&entry, sizeof(entry));

		adpt_ppe_flow_key_ipv6_3tuple_convert(flow_entry, &entry, A_TRUE);

		rv = hppe_flow_entry_host_op_ipv6_3tuple_del(dev_id, del_mode, &flow_entry->entry_id, &entry);
	}
#if defined(JHPPE)
	else if ((type & FAL_FLOW_IP4_6TUPLE_ADDR) || (type & FAL_FLOW_IP6_6TUPLE_ADDR)) {
		/* entry_type is not checked by hardware, which means IPv6 and IPv4 supported. */
		union in_flow_6tuple_tbl_u entry;
		aos_mem_zero(&entry, sizeof(entry));

		adpt_ppe_flow_key_ip_6tuple_convert(flow_entry, &entry, type, A_TRUE);

		rv = jhppe_flow_entry_host_op_ip_6tuple_del(dev_id, del_mode, &flow_entry->entry_id, &entry);
	}
#endif
	else
		return SW_FAIL;
	return rv;
}

sw_error_t
adpt_hppe_flow_host_add(
		a_uint32_t dev_id,
		a_uint32_t add_mode,
		fal_flow_host_entry_t *flow_host)
{
	sw_error_t rv = SW_OK;
	fal_flow_entry_t *flow_entry = &(flow_host->flow_entry);
	fal_host_entry_t *host_entry = &(flow_host->host_entry);
	ADPT_DEV_ID_CHECK(dev_id);
	ADPT_NULL_POINT_CHECK(flow_host);

	rv = adpt_hppe_ip_flow_host_data_add(dev_id, host_entry);
	SW_RTN_ON_ERROR(rv);

	rv = adpt_hppe_flow_entry_host_op_add(dev_id, add_mode, flow_entry);
	SW_RTN_ON_ERROR(rv);

	rv = hppe_flow_host_tbl_op_rslt_host_entry_index_get(dev_id, &host_entry->entry_id);
	return rv;
}

sw_error_t
adpt_hppe_flow_host_get(
		a_uint32_t dev_id,
		a_uint32_t get_mode,
		fal_flow_host_entry_t *flow_host)
{
	sw_error_t rv = SW_OK;
	fal_flow_entry_t *flow_entry = &(flow_host->flow_entry);
	fal_host_entry_t *host_entry = &(flow_host->host_entry);
	ADPT_DEV_ID_CHECK(dev_id);
	ADPT_NULL_POINT_CHECK(flow_host);

	rv = adpt_hppe_ip_flow_host_data_rd_add(dev_id, host_entry);
	rv = adpt_hppe_flow_entry_host_op_get(dev_id, get_mode, flow_entry);
	if(rv != SW_OK)
		return rv;

	rv = hppe_flow_host_tbl_rd_op_rslt_host_entry_index_get(dev_id, &host_entry->entry_id);
	if(rv != SW_OK)
		return rv;

	rv = adpt_hppe_ip_flow_host_data_get(dev_id, get_mode, host_entry);

	return rv;
}

sw_error_t
adpt_hppe_flow_host_del(
		a_uint32_t dev_id,
		a_uint32_t del_mode,
		fal_flow_host_entry_t *flow_host)
{
	fal_flow_entry_t *flow_entry = &(flow_host->flow_entry);
	fal_host_entry_t *host_entry = &(flow_host->host_entry);
	ADPT_DEV_ID_CHECK(dev_id);
	ADPT_NULL_POINT_CHECK(flow_host);

	adpt_hppe_ip_flow_host_data_del(dev_id, del_mode, host_entry);
	adpt_hppe_flow_entry_host_op_del(dev_id, del_mode, flow_entry);
	return SW_OK;
}


sw_error_t
adpt_hppe_flow_entry_get(
		a_uint32_t dev_id,
		a_uint32_t get_mode,
		fal_flow_entry_t *flow_entry)
{
	sw_error_t rv = SW_OK;
	a_uint32_t type = 0;
	union in_flow_cnt_tbl_u cnt;

	ADPT_DEV_ID_CHECK(dev_id);
	ADPT_NULL_POINT_CHECK(flow_entry);

	type = flow_entry->entry_type;
	if ((type & FAL_FLOW_IP4_5TUPLE_ADDR) == FAL_FLOW_IP4_5TUPLE_ADDR) {
		union in_flow_tbl_u entry;
		aos_mem_zero(&entry, sizeof(entry));

		adpt_ppe_flow_key_ipv4_5tuple_convert(flow_entry, &entry, A_TRUE);
		rv = hppe_flow_ipv4_5tuple_get(dev_id, get_mode, &flow_entry->entry_id, &entry);
		if (entry.bf.entry_type != FLOW_ENTRY_TYPE_IPV4 ||
				entry.bf.protocol_type == FLOW_TUPLE_TYPE_3) {
			return SW_BAD_VALUE;
		}

		adpt_ppe_flow_ipv4_5tuple_convert(flow_entry, &entry, A_FALSE);
		adpt_ppe_flow_key_ipv4_5tuple_convert(flow_entry, &entry, A_FALSE);
	} else if ((type & FAL_FLOW_IP6_5TUPLE_ADDR) == FAL_FLOW_IP6_5TUPLE_ADDR) {
		union in_flow_ipv6_5tuple_tbl_u entry;
		aos_mem_zero(&entry, sizeof(entry));

		adpt_ppe_flow_key_ipv6_5tuple_convert(flow_entry, &entry, A_TRUE);
		rv = hppe_flow_ipv6_5tuple_get(dev_id, get_mode, &flow_entry->entry_id, &entry);
		if (entry.bf.entry_type != FLOW_ENTRY_TYPE_IPV6 ||
				entry.bf.protocol_type == FLOW_TUPLE_TYPE_3) {
			return SW_BAD_VALUE;
		}

		adpt_ppe_flow_ipv6_5tuple_convert(flow_entry, &entry, A_FALSE);
		adpt_ppe_flow_key_ipv6_5tuple_convert(flow_entry, &entry, A_FALSE);
	} else if ((type & FAL_FLOW_IP4_3TUPLE_ADDR) == FAL_FLOW_IP4_3TUPLE_ADDR) {
		union in_flow_3tuple_tbl_u entry;
		aos_mem_zero(&entry, sizeof(entry));

		adpt_ppe_flow_key_ipv4_3tuple_convert(flow_entry, &entry, A_TRUE);
		rv = hppe_flow_ipv4_3tuple_get(dev_id, get_mode, &flow_entry->entry_id, &entry);
		if (entry.bf.entry_type != FLOW_ENTRY_TYPE_IPV4 ||
				entry.bf.protocol_type != FLOW_TUPLE_TYPE_3) {
			return SW_BAD_VALUE;
		}

		adpt_ppe_flow_ipv4_3tuple_convert(flow_entry, &entry, A_FALSE);
		adpt_ppe_flow_key_ipv4_3tuple_convert(flow_entry, &entry, A_FALSE);
	} else if ((type & FAL_FLOW_IP6_3TUPLE_ADDR) == FAL_FLOW_IP6_3TUPLE_ADDR) {
		union in_flow_ipv6_3tuple_tbl_u entry;
		aos_mem_zero(&entry, sizeof(entry));

		adpt_ppe_flow_key_ipv6_3tuple_convert(flow_entry, &entry, A_TRUE);
		rv = hppe_flow_ipv6_3tuple_get(dev_id, get_mode, &flow_entry->entry_id, &entry);
		if (entry.bf.entry_type != FLOW_ENTRY_TYPE_IPV6 ||
				entry.bf.protocol_type != FLOW_TUPLE_TYPE_3) {
			return SW_BAD_VALUE;
		}

		adpt_ppe_flow_ipv6_3tuple_convert(flow_entry, &entry, A_FALSE);
		adpt_ppe_flow_key_ipv6_3tuple_convert(flow_entry, &entry, A_FALSE);

	}
#if defined(JHPPE)
	else if ((type & FAL_FLOW_IP4_6TUPLE_ADDR) || (type & FAL_FLOW_IP6_6TUPLE_ADDR)) {
		/* both IPv6 and IPv4 supported. */
		union in_flow_6tuple_tbl_u entry;
		aos_mem_zero(&entry, sizeof(entry));

		adpt_ppe_flow_key_ip_6tuple_convert(flow_entry, &entry, type, A_TRUE);
		rv = jhppe_flow_ip_6tuple_get(dev_id, get_mode, &flow_entry->entry_id, &entry);
		adpt_ppe_flow_ip_6tuple_convert(flow_entry, &entry, A_FALSE);
		adpt_ppe_flow_key_ip_6tuple_convert(flow_entry, &entry, type, A_FALSE);
	}
#endif
	else
		return SW_FAIL;

	if (rv)
		return rv;

	aos_mem_zero(&cnt, sizeof(cnt));
	rv = hppe_in_flow_cnt_tbl_get(dev_id, flow_entry->entry_id, &cnt);
	if (rv)
		return rv;

	flow_entry->pkt_counter = cnt.bf.hit_pkt_counter;
	flow_entry->byte_counter = cnt.bf.hit_byte_counter_0 | \
				   ((a_uint64_t)cnt.bf.hit_byte_counter_1 << 32);

#if defined(MPPE)
	rv = mppe_qos_mapping_tbl_flow_policer_get(dev_id, flow_entry->entry_id,
			&(flow_entry->policer_valid), &(flow_entry->policer_index));
	if (rv)
		return rv;
#endif

#if defined(JHPPE)
	rv = adpt_jhppe_flow_gro_en_get(dev_id, flow_entry->entry_id, &(flow_entry->gro_en));
	if (rv)
		return rv;
#endif

	return adpt_hppe_flow_qos_get(dev_id, flow_entry->entry_id, &(flow_entry->flow_qos));
}

sw_error_t
adpt_hppe_flow_entry_next(
		a_uint32_t dev_id,
		a_uint32_t next_mode,
		fal_flow_entry_t *flow_entry)
{
	a_uint32_t i = 0, step = 0;
	sw_error_t rv = SW_OK;

	ADPT_DEV_ID_CHECK(dev_id);
	ADPT_NULL_POINT_CHECK(flow_entry);

	if (FAL_NEXT_ENTRY_FIRST_ID == flow_entry->entry_id)
		i = 0;

	if (next_mode == FAL_FLOW_IP4_3TUPLE_ADDR ||
		next_mode == FAL_FLOW_IP4_5TUPLE_ADDR) {
		if (FAL_NEXT_ENTRY_FIRST_ID != flow_entry->entry_id)
			i = flow_entry->entry_id + 1;
		step = 1;
	} else if (next_mode == FAL_FLOW_IP6_5TUPLE_ADDR ||
		 next_mode == FAL_FLOW_IP6_3TUPLE_ADDR) {
		if (FAL_NEXT_ENTRY_FIRST_ID != flow_entry->entry_id)
			i = (flow_entry->entry_id & ~1) + 2;
		step = 2;
	}
	for (; i < IN_FLOW_CNT_TBL_NUM;) {
		flow_entry->entry_type = next_mode;
		flow_entry->entry_id = i;
		rv = adpt_hppe_flow_entry_get(dev_id, 1, flow_entry);
		if (!rv) {
			return rv;
		}
		i += step;
	}

	return SW_FAIL;

}

sw_error_t
adpt_hppe_flow_entry_del(
		a_uint32_t dev_id,
		a_uint32_t del_mode,
		fal_flow_entry_t *flow_entry)
{
	sw_error_t rv = SW_OK;
	a_uint32_t type = 0;
	ADPT_DEV_ID_CHECK(dev_id);
	ADPT_NULL_POINT_CHECK(flow_entry);

	if (del_mode == FAL_FLOW_OP_MODE_FLUSH)
		return hppe_flow_flush_common(dev_id);

	type = flow_entry->entry_type;
	if ((type & FAL_FLOW_IP4_5TUPLE_ADDR) == FAL_FLOW_IP4_5TUPLE_ADDR) {
		union in_flow_tbl_u entry;
		aos_mem_zero(&entry, sizeof(entry));

		adpt_ppe_flow_key_ipv4_5tuple_convert(flow_entry, &entry, A_TRUE);

		rv = hppe_flow_ipv4_5tuple_del(dev_id, del_mode, &flow_entry->entry_id, &entry);
	} else if ((type & FAL_FLOW_IP6_5TUPLE_ADDR) == FAL_FLOW_IP6_5TUPLE_ADDR) {
		union in_flow_ipv6_5tuple_tbl_u entry;
		aos_mem_zero(&entry, sizeof(entry));

		adpt_ppe_flow_key_ipv6_5tuple_convert(flow_entry, &entry, A_TRUE);

		rv = hppe_flow_ipv6_5tuple_del(dev_id, del_mode, &flow_entry->entry_id, &entry);
	} else if ((type & FAL_FLOW_IP4_3TUPLE_ADDR) == FAL_FLOW_IP4_3TUPLE_ADDR) {
		union in_flow_3tuple_tbl_u entry;
		aos_mem_zero(&entry, sizeof(entry));

		adpt_ppe_flow_key_ipv4_3tuple_convert(flow_entry, &entry, A_TRUE);

		rv = hppe_flow_ipv4_3tuple_del(dev_id, del_mode, &flow_entry->entry_id, &entry);
	} else if ((type & FAL_FLOW_IP6_3TUPLE_ADDR) == FAL_FLOW_IP6_3TUPLE_ADDR) {
		union in_flow_ipv6_3tuple_tbl_u entry;
		aos_mem_zero(&entry, sizeof(entry));

		adpt_ppe_flow_key_ipv6_3tuple_convert(flow_entry, &entry, A_TRUE);

		rv = hppe_flow_ipv6_3tuple_del(dev_id, del_mode, &flow_entry->entry_id, &entry);
	}
#if defined(JHPPE)
	else if ((type & FAL_FLOW_IP4_6TUPLE_ADDR) || (type & FAL_FLOW_IP6_6TUPLE_ADDR)) {
		/* entry_type is not checked by hardware, which means IPv6 and IPv4 supported. */
		union in_flow_6tuple_tbl_u entry;
		aos_mem_zero(&entry, sizeof(entry));

		adpt_ppe_flow_key_ip_6tuple_convert(flow_entry, &entry, type, A_TRUE);

		rv = jhppe_flow_ip_6tuple_del(dev_id, del_mode, &flow_entry->entry_id, &entry);
	}
#endif
	else
		return SW_FAIL;
	return rv;
}
sw_error_t
adpt_hppe_flow_status_get(a_uint32_t dev_id, a_bool_t *enable)
{
	sw_error_t rv = SW_OK;
	union flow_ctrl0_u flow_ctrl0;

	ADPT_DEV_ID_CHECK(dev_id);
	ADPT_NULL_POINT_CHECK(enable);

	memset(&flow_ctrl0, 0, sizeof(flow_ctrl0));

	rv = hppe_flow_ctrl0_get(dev_id, &flow_ctrl0);
	if( rv != SW_OK )
		return rv;

	*enable = flow_ctrl0.bf.flow_en;
	return SW_OK;
}

sw_error_t
adpt_hppe_flow_ctrl_set(
		a_uint32_t dev_id,
		fal_flow_pkt_type_t type,
		fal_flow_direction_t dir,
		fal_flow_mgmt_t *ctrl)
{
	sw_error_t rv = SW_OK;
	union flow_ctrl1_u flow_ctrl1;

	ADPT_DEV_ID_CHECK(dev_id);
	ADPT_NULL_POINT_CHECK(ctrl);

	memset(&flow_ctrl1, 0, sizeof(flow_ctrl1));
	rv = hppe_flow_ctrl1_get(dev_id, type, &flow_ctrl1);
	if( rv != SW_OK )
		return rv;

	if (dir == FAL_FLOW_LAN_TO_LAN_DIR) {
		flow_ctrl1.bf.flow_ctl0_miss_action = ctrl->miss_action;
		flow_ctrl1.bf.flow_ctl0_frag_bypass = ctrl->frag_bypass_en;
		flow_ctrl1.bf.flow_ctl0_tcp_special = ctrl->tcp_spec_bypass_en;
		flow_ctrl1.bf.flow_ctl0_bypass = ctrl->all_bypass_en;
		flow_ctrl1.bf.flow_ctl0_key_sel = ctrl->key_sel;
	} else if (dir == FAL_FLOW_LAN_TO_WAN_DIR) {
		flow_ctrl1.bf.flow_ctl1_miss_action = ctrl->miss_action;
		flow_ctrl1.bf.flow_ctl1_frag_bypass = ctrl->frag_bypass_en;
		flow_ctrl1.bf.flow_ctl1_tcp_special = ctrl->tcp_spec_bypass_en;
		flow_ctrl1.bf.flow_ctl1_bypass = ctrl->all_bypass_en;
		flow_ctrl1.bf.flow_ctl1_key_sel = ctrl->key_sel;
	} else if (dir == FAL_FLOW_WAN_TO_LAN_DIR) {
		flow_ctrl1.bf.flow_ctl2_miss_action = ctrl->miss_action;
		flow_ctrl1.bf.flow_ctl2_frag_bypass = ctrl->frag_bypass_en;
		flow_ctrl1.bf.flow_ctl2_tcp_special = ctrl->tcp_spec_bypass_en;
		flow_ctrl1.bf.flow_ctl2_bypass = ctrl->all_bypass_en;
		flow_ctrl1.bf.flow_ctl2_key_sel = ctrl->key_sel;
	} else if (dir == FAL_FLOW_WAN_TO_WAN_DIR) {
		flow_ctrl1.bf.flow_ctl3_miss_action = ctrl->miss_action;
		flow_ctrl1.bf.flow_ctl3_frag_bypass = ctrl->frag_bypass_en;
		flow_ctrl1.bf.flow_ctl3_tcp_special = ctrl->tcp_spec_bypass_en;
		flow_ctrl1.bf.flow_ctl3_bypass = ctrl->all_bypass_en;
		flow_ctrl1.bf.flow_ctl3_key_sel = ctrl->key_sel;
	} else if (dir == FAL_FLOW_UNKOWN_DIR_DIR) {
		flow_ctrl1.bf.flow_ctl4_miss_action = ctrl->miss_action;
		flow_ctrl1.bf.flow_ctl4_frag_bypass = ctrl->frag_bypass_en;
		flow_ctrl1.bf.flow_ctl4_tcp_special = ctrl->tcp_spec_bypass_en;
		flow_ctrl1.bf.flow_ctl4_bypass = ctrl->all_bypass_en;
		flow_ctrl1.bf.flow_ctl4_key_sel = ctrl->key_sel;
	} else
		return SW_FAIL;

	return hppe_flow_ctrl1_set(dev_id, type, &flow_ctrl1);;
}

#if !defined(IN_FLOW_MINI)
sw_error_t
adpt_hppe_flow_age_timer_get(a_uint32_t dev_id, fal_flow_age_timer_t *age_timer)
{
	sw_error_t rv = SW_OK;
	union flow_ctrl0_u flow_ctrl0;

	ADPT_DEV_ID_CHECK(dev_id);
	ADPT_NULL_POINT_CHECK(age_timer);

	memset(&flow_ctrl0, 0, sizeof(flow_ctrl0));

	rv = hppe_flow_ctrl0_get(dev_id, &flow_ctrl0);
	if( rv != SW_OK )
		return rv;

	age_timer->age_time = flow_ctrl0.bf.flow_age_timer;
	age_timer->unit = flow_ctrl0.bf.flow_age_timer_unit;
	return SW_OK;
}
#endif

sw_error_t
adpt_hppe_flow_status_set(a_uint32_t dev_id, a_bool_t enable)
{
	sw_error_t rv = SW_OK;
	union flow_ctrl0_u flow_ctrl0;

	ADPT_DEV_ID_CHECK(dev_id);

	memset(&flow_ctrl0, 0, sizeof(flow_ctrl0));

	rv = hppe_flow_ctrl0_get(dev_id, &flow_ctrl0);
	if( rv != SW_OK )
		return rv;

	flow_ctrl0.bf.flow_en = enable;
	return hppe_flow_ctrl0_set(dev_id, &flow_ctrl0);
}

sw_error_t
adpt_hppe_flow_ctrl_get(
		a_uint32_t dev_id,
		fal_flow_pkt_type_t type,
		fal_flow_direction_t dir,
		fal_flow_mgmt_t *ctrl)
{
	sw_error_t rv = SW_OK;
	union flow_ctrl1_u flow_ctrl1;

	ADPT_DEV_ID_CHECK(dev_id);
	ADPT_NULL_POINT_CHECK(ctrl);

	memset(&flow_ctrl1, 0, sizeof(flow_ctrl1));
	rv = hppe_flow_ctrl1_get(dev_id, type, &flow_ctrl1);
	if( rv != SW_OK )
		return rv;

	if (dir == FAL_FLOW_LAN_TO_LAN_DIR) {
		ctrl->miss_action = flow_ctrl1.bf.flow_ctl0_miss_action;
		ctrl->frag_bypass_en = flow_ctrl1.bf.flow_ctl0_frag_bypass;
		ctrl->tcp_spec_bypass_en = flow_ctrl1.bf.flow_ctl0_tcp_special;
		ctrl->all_bypass_en = flow_ctrl1.bf.flow_ctl0_bypass;
		ctrl->key_sel = flow_ctrl1.bf.flow_ctl0_key_sel;
	} else if (dir == FAL_FLOW_LAN_TO_WAN_DIR) {
		ctrl->miss_action = flow_ctrl1.bf.flow_ctl1_miss_action;
		ctrl->frag_bypass_en = flow_ctrl1.bf.flow_ctl1_frag_bypass;
		ctrl->tcp_spec_bypass_en = flow_ctrl1.bf.flow_ctl1_tcp_special;
		ctrl->all_bypass_en = flow_ctrl1.bf.flow_ctl1_bypass;
		ctrl->key_sel = flow_ctrl1.bf.flow_ctl1_key_sel;
	} else if (dir == FAL_FLOW_WAN_TO_LAN_DIR) {
		ctrl->miss_action = flow_ctrl1.bf.flow_ctl2_miss_action;
		ctrl->frag_bypass_en = flow_ctrl1.bf.flow_ctl2_frag_bypass;
		ctrl->tcp_spec_bypass_en = flow_ctrl1.bf.flow_ctl2_tcp_special;
		ctrl->all_bypass_en = flow_ctrl1.bf.flow_ctl2_bypass;
		ctrl->key_sel = flow_ctrl1.bf.flow_ctl2_key_sel;
	} else if (dir == FAL_FLOW_WAN_TO_WAN_DIR) {
		ctrl->miss_action = flow_ctrl1.bf.flow_ctl3_miss_action;
		ctrl->frag_bypass_en = flow_ctrl1.bf.flow_ctl3_frag_bypass;
		ctrl->tcp_spec_bypass_en = flow_ctrl1.bf.flow_ctl3_tcp_special;
		ctrl->all_bypass_en = flow_ctrl1.bf.flow_ctl3_bypass;
		ctrl->key_sel = flow_ctrl1.bf.flow_ctl3_key_sel;
	} else if (dir == FAL_FLOW_UNKOWN_DIR_DIR) {
		ctrl->miss_action = flow_ctrl1.bf.flow_ctl4_miss_action;
		ctrl->frag_bypass_en = flow_ctrl1.bf.flow_ctl4_frag_bypass;
		ctrl->tcp_spec_bypass_en = flow_ctrl1.bf.flow_ctl4_tcp_special;
		ctrl->all_bypass_en = flow_ctrl1.bf.flow_ctl4_bypass;
		ctrl->key_sel = flow_ctrl1.bf.flow_ctl4_key_sel;
	} else
		return SW_FAIL;

	return SW_OK;
}

#if !defined(IN_FLOW_MINI)
sw_error_t
adpt_hppe_flow_age_timer_set(a_uint32_t dev_id, fal_flow_age_timer_t *age_timer)
{
	sw_error_t rv = SW_OK;
	union flow_ctrl0_u flow_ctrl0;

	ADPT_DEV_ID_CHECK(dev_id);
	ADPT_NULL_POINT_CHECK(age_timer);

	memset(&flow_ctrl0, 0, sizeof(flow_ctrl0));

	rv = hppe_flow_ctrl0_get(dev_id, &flow_ctrl0);
	if( rv != SW_OK )
		return rv;

	flow_ctrl0.bf.flow_age_timer = age_timer->age_time;
	flow_ctrl0.bf.flow_age_timer_unit = age_timer->unit;
	return hppe_flow_ctrl0_set(dev_id, &flow_ctrl0);
}
#endif

sw_error_t
adpt_hppe_flow_entry_add(
		a_uint32_t dev_id,
		a_uint32_t add_mode, /*index or hash*/
		fal_flow_entry_t *flow_entry)
{
	sw_error_t rv = SW_OK;
	a_uint32_t type = 0;
	ADPT_DEV_ID_CHECK(dev_id);
	ADPT_NULL_POINT_CHECK(flow_entry);

	type = flow_entry->entry_type;
	if ((type & FAL_FLOW_IP4_5TUPLE_ADDR) == FAL_FLOW_IP4_5TUPLE_ADDR) {
		union in_flow_tbl_u entry;
		aos_mem_zero(&entry, sizeof(entry));

		adpt_ppe_flow_key_ipv4_5tuple_convert(flow_entry, &entry, A_TRUE);
		adpt_ppe_flow_ipv4_5tuple_convert(flow_entry, &entry, A_TRUE);

		rv = hppe_flow_ipv4_5tuple_add(dev_id, (a_uint32_t)add_mode, &flow_entry->entry_id, &entry);
	} else if ((type & FAL_FLOW_IP6_5TUPLE_ADDR) == FAL_FLOW_IP6_5TUPLE_ADDR) {
		union in_flow_ipv6_5tuple_tbl_u entry;
		aos_mem_zero(&entry, sizeof(entry));

		adpt_ppe_flow_key_ipv6_5tuple_convert(flow_entry, &entry, A_TRUE);
		adpt_ppe_flow_ipv6_5tuple_convert(flow_entry, &entry, A_TRUE);

		rv = hppe_flow_ipv6_5tuple_add(dev_id, (a_uint32_t)add_mode, &flow_entry->entry_id, &entry);
	} else if ((type & FAL_FLOW_IP4_3TUPLE_ADDR) == FAL_FLOW_IP4_3TUPLE_ADDR) {
		union in_flow_3tuple_tbl_u entry;
		aos_mem_zero(&entry, sizeof(entry));

		adpt_ppe_flow_key_ipv4_3tuple_convert(flow_entry, &entry, A_TRUE);
		adpt_ppe_flow_ipv4_3tuple_convert(flow_entry, &entry, A_TRUE);

		rv = hppe_flow_ipv4_3tuple_add(dev_id, (a_uint32_t)add_mode, &flow_entry->entry_id, &entry);
	} else if ((type & FAL_FLOW_IP6_3TUPLE_ADDR) == FAL_FLOW_IP6_3TUPLE_ADDR) {
		union in_flow_ipv6_3tuple_tbl_u entry;
		aos_mem_zero(&entry, sizeof(entry));

		adpt_ppe_flow_key_ipv6_3tuple_convert(flow_entry, &entry, A_TRUE);
		adpt_ppe_flow_ipv6_3tuple_convert(flow_entry, &entry, A_TRUE);

		rv = hppe_flow_ipv6_3tuple_add(dev_id, (a_uint32_t)add_mode, &flow_entry->entry_id, &entry);
	}
#if defined(JHPPE)
	else if ((type & FAL_FLOW_IP4_6TUPLE_ADDR) || (type & FAL_FLOW_IP6_6TUPLE_ADDR)) {
		/* entry_type is not checked by hardware, which means IPv6 and IPv4 supported. */
		union in_flow_6tuple_tbl_u entry;
		aos_mem_zero(&entry, sizeof(entry));

		adpt_ppe_flow_key_ip_6tuple_convert(flow_entry, &entry, type, A_TRUE);
		adpt_ppe_flow_ip_6tuple_convert(flow_entry, &entry, A_TRUE);

		rv = jhppe_flow_ip_6tuple_add(dev_id, (a_uint32_t)add_mode, &flow_entry->entry_id, &entry);
	}
#endif
	else
		return SW_FAIL;

	if (rv != SW_OK)
		return rv;

#if defined(MPPE)
	rv = mppe_qos_mapping_tbl_flow_policer_set(dev_id, flow_entry->entry_id,
			flow_entry->policer_valid, flow_entry->policer_index);

	if (rv)
		return rv;
#endif

#if defined(JHPPE)
	rv = adpt_jhppe_flow_gro_en_set(dev_id, flow_entry->entry_id, flow_entry->gro_en);
	if (rv)
		return rv;
#endif

	return adpt_hppe_flow_qos_set(dev_id, flow_entry->entry_id, &(flow_entry->flow_qos));
}

sw_error_t
adpt_hppe_flow_counter_get(a_uint32_t dev_id,
		a_uint32_t flow_index, fal_entry_counter_t *flow_counter)
{
	sw_error_t rv = SW_OK;
	union in_flow_cnt_tbl_u flow_cnt;

	ADPT_DEV_ID_CHECK(dev_id);
	ADPT_NULL_POINT_CHECK(flow_counter);

	if (flow_index >= IN_FLOW_CNT_TBL_NUM)
		return SW_OUT_OF_RANGE;

	aos_mem_zero(&flow_cnt, sizeof(flow_cnt));

	rv = hppe_in_flow_cnt_tbl_get(dev_id, flow_index, &flow_cnt);
	SW_RTN_ON_ERROR(rv);

	flow_counter->matched_pkts = flow_cnt.bf.hit_pkt_counter;
	flow_counter->matched_bytes = flow_cnt.bf.hit_byte_counter_0 |
		(a_uint64_t)flow_cnt.bf.hit_byte_counter_1 << SW_FIELD_OFFSET_IN_WORD(
				IN_FLOW_CNT_TBL_HIT_BYTE_COUNTER_OFFSET);
	return rv;
}

sw_error_t
adpt_hppe_flow_counter_cleanup(a_uint32_t dev_id, a_uint32_t flow_index)
{
	sw_error_t rv = SW_OK;
	union in_flow_cnt_tbl_u flow_cnt;

	ADPT_DEV_ID_CHECK(dev_id);

	if (flow_index >= IN_FLOW_CNT_TBL_NUM)
		return SW_OUT_OF_RANGE;

	aos_mem_zero(&flow_cnt, sizeof(flow_cnt));

	rv = hppe_in_flow_cnt_tbl_set(dev_id, flow_index, &flow_cnt);

	return rv;
}

sw_error_t
adpt_ppe_flow_fwd_type_set(a_uint32_t dev_id, a_uint32_t flow_index,
			   fal_flow_fwd_type_t fwd_type)
{
	sw_error_t rv = SW_OK;
	union in_flow_tbl_u entry;

	ADPT_DEV_ID_CHECK(dev_id);
	if (flow_index >= IN_FLOW_CNT_TBL_NUM)
		return SW_OUT_OF_RANGE;

	aos_mem_zero(&entry, sizeof(entry));

	rv = hppe_in_flow_tbl_get(dev_id, flow_index, &entry);
	SW_RTN_ON_ERROR(rv);

#if defined(JHPPE)
	entry.bf.fwd_type_0 = fwd_type;
	entry.bf.fwd_type_1 = fwd_type >> SW_FIELD_OFFSET_IN_WORD(IN_FLOW_TBL_FWD_TYPE_OFFSET);
#else
	entry.bf.fwd_type = fwd_type;
#endif

	return hppe_in_flow_tbl_set(dev_id, flow_index, &entry);
}

sw_error_t
adpt_ppe_flow_fwd_type_get(a_uint32_t dev_id, a_uint32_t flow_index,
			   fal_flow_fwd_type_t *fwd_type)
{
	sw_error_t rv = SW_OK;
	union in_flow_tbl_u entry;

	ADPT_DEV_ID_CHECK(dev_id);
	if (flow_index >= IN_FLOW_CNT_TBL_NUM)
		return SW_OUT_OF_RANGE;

	aos_mem_zero(&entry, sizeof(entry));

	rv = hppe_in_flow_tbl_get(dev_id, flow_index, &entry);
	SW_RTN_ON_ERROR(rv);

#if defined(JHPPE)
	*fwd_type = entry.bf.fwd_type_0;
	*fwd_type |= entry.bf.fwd_type_1 << SW_FIELD_OFFSET_IN_WORD(IN_FLOW_TBL_FWD_TYPE_OFFSET);
#else
	*fwd_type = entry.bf.fwd_type;
#endif

	return SW_OK;
}

sw_error_t
adpt_hppe_flow_entry_en_set(a_uint32_t dev_id, a_uint32_t flow_index, a_bool_t enable)
{
	sw_error_t rv = SW_OK;
	union in_flow_tbl_u entry;

	ADPT_DEV_ID_CHECK(dev_id);

	if (flow_index >= IN_FLOW_CNT_TBL_NUM)
		return SW_OUT_OF_RANGE;

	aos_mem_zero(&entry, sizeof(entry));

	rv = hppe_in_flow_tbl_get(dev_id, flow_index, &entry);
	SW_RTN_ON_ERROR(rv);

	entry.bf.valid = enable;

	rv = hppe_in_flow_tbl_set(dev_id, flow_index, &entry);
	return rv;
}

sw_error_t
adpt_hppe_flow_entry_en_get(a_uint32_t dev_id, a_uint32_t flow_index, a_bool_t *enable)
{
	sw_error_t rv = SW_OK;
	union in_flow_tbl_u entry;

	ADPT_DEV_ID_CHECK(dev_id);
	ADPT_NULL_POINT_CHECK(enable);

	if (flow_index >= IN_FLOW_CNT_TBL_NUM)
		return SW_OUT_OF_RANGE;

	aos_mem_zero(&entry, sizeof(entry));

	rv = hppe_in_flow_tbl_get(dev_id, flow_index, &entry);
	SW_RTN_ON_ERROR(rv);

	*enable = entry.bf.valid;
	return rv;
}

sw_error_t
adpt_hppe_flow_global_cfg_get(
		a_uint32_t dev_id,
		fal_flow_global_cfg_t *cfg)
{
	sw_error_t rv = SW_OK;
	union flow_ctrl0_u flow_ctrl0;
	union l3_route_ctrl_u route_ctrl;
	union l3_route_ctrl_ext_u route_ctrl_ext;
	adpt_ppe_type_t ppe_type = MAX_PPE_TYPE;

	ADPT_DEV_ID_CHECK(dev_id);
	ADPT_NULL_POINT_CHECK(cfg);
	
	memset(&flow_ctrl0, 0, sizeof(flow_ctrl0));
	memset(&route_ctrl, 0, sizeof(route_ctrl));
	memset(&route_ctrl_ext, 0, sizeof(route_ctrl_ext));

	ppe_type = adpt_ppe_type_get(dev_id);

#if defined(MPPE)
	if (ppe_type == MPPE_TYPE) {
		union eg_gen_ctrl_u eg_ctrl;

		memset(&eg_ctrl, 0, sizeof(eg_ctrl));
		rv = mppe_eg_gen_ctrl_get(dev_id, &eg_ctrl);
		SW_RTN_ON_ERROR(rv);

		cfg->flow_cookie_pri = eg_ctrl.bf.flow_cookie_pri;
	}
#endif

	rv = hppe_flow_ctrl0_get(dev_id, &flow_ctrl0);
	SW_RTN_ON_ERROR(rv);

	rv = hppe_l3_route_ctrl_get(dev_id, &route_ctrl);
	SW_RTN_ON_ERROR(rv);

	rv = hppe_l3_route_ctrl_ext_get(dev_id, &route_ctrl_ext);
	SW_RTN_ON_ERROR(rv);

	if (ppe_type != MAX_PPE_TYPE && ppe_type != HPPE_TYPE) {
		a_bool_t flow_cpy_escape = A_FALSE;

		rv = adpt_appe_flow_copy_escape_get(dev_id, &flow_cpy_escape);
		SW_RTN_ON_ERROR(rv);

		cfg->flow_mismatch_copy_escape_en = flow_cpy_escape;
	}

	cfg->src_if_check_action= route_ctrl.bf.flow_src_if_check_cmd;
	cfg->src_if_check_deacclr_en= route_ctrl.bf.flow_src_if_check_de_acce;
	cfg->service_loop_en = route_ctrl_ext.bf.flow_service_code_loop_en;
	cfg->ptmu_fail_action = route_ctrl_ext.bf.flow_pmtu_fail;
	cfg->ptmu_fail_deacclr_en = route_ctrl_ext.bf.flow_pmtu_fail_de_acce;
	cfg->ptmu_fail_df_action = route_ctrl_ext.bf.flow_pmtu_df_fail;
	cfg->ptmu_fail_df_deacclr_en = route_ctrl_ext.bf.flow_pmtu_df_fail_de_acce;
	cfg->l2_vpn_en = route_ctrl_ext.bf.l2_vpn_en;
	cfg->l3_vpn_en = route_ctrl_ext.bf.l3_vpn_en;

#if defined(JHPPE)
	cfg->flow_key_en_bitmap = 0;
	if (route_ctrl_ext.bf.routing_as_flow_key)
		cfg->flow_key_en_bitmap |= FAL_FLOW_KEY_ROUTING;
	if (route_ctrl_ext.bf.ipsec_ah_en)
		cfg->flow_key_en_bitmap |= FAL_FLOW_KEY_IPSEC_AH;
	if (route_ctrl_ext.bf.ipsec_esp_en)
		cfg->flow_key_en_bitmap |= FAL_FLOW_KEY_IPSEC_ESP;
	if (route_ctrl_ext.bf.ipsec_natt_en)
		cfg->flow_key_en_bitmap |= FAL_FLOW_KEY_IPSEC_NATT;
#endif
	cfg->service_loop_action = route_ctrl.bf.flow_service_code_loop;
	cfg->service_loop_deacclr_en = route_ctrl.bf.flow_service_code_loop_de_acce;
	cfg->flow_deacclr_action = route_ctrl.bf.flow_de_acce_cmd;
	cfg->sync_mismatch_action = route_ctrl.bf.flow_sync_mismatch_cmd;
	cfg->sync_mismatch_deacclr_en = route_ctrl.bf.flow_sync_mismatch_de_acce;
	cfg->hash_mode_0 = flow_ctrl0.bf.flow_hash_mode_0;
	cfg->hash_mode_1 = flow_ctrl0.bf.flow_hash_mode_1;

	return SW_OK;
}

sw_error_t
adpt_hppe_flow_global_cfg_set(
		a_uint32_t dev_id,
		fal_flow_global_cfg_t *cfg)
{
	sw_error_t rv = SW_OK;
	union flow_ctrl0_u flow_ctrl0;
	union l3_route_ctrl_u route_ctrl;
	union l3_route_ctrl_ext_u route_ctrl_ext;
	adpt_ppe_type_t ppe_type = MAX_PPE_TYPE;

	ADPT_DEV_ID_CHECK(dev_id);
	ADPT_NULL_POINT_CHECK(cfg);
	
	memset(&flow_ctrl0, 0, sizeof(flow_ctrl0));
	memset(&route_ctrl, 0, sizeof(route_ctrl));
	memset(&route_ctrl_ext, 0, sizeof(route_ctrl_ext));

	ppe_type = adpt_ppe_type_get(dev_id);

	rv = hppe_flow_ctrl0_get(dev_id, &flow_ctrl0);
	SW_RTN_ON_ERROR(rv);

	rv = hppe_l3_route_ctrl_get(dev_id, &route_ctrl);
	SW_RTN_ON_ERROR(rv);

	rv = hppe_l3_route_ctrl_ext_get(dev_id, &route_ctrl_ext);
	SW_RTN_ON_ERROR(rv);

	route_ctrl.bf.flow_src_if_check_cmd = cfg->src_if_check_action;
	route_ctrl.bf.flow_src_if_check_de_acce = cfg->src_if_check_deacclr_en;
	route_ctrl_ext.bf.flow_service_code_loop_en = cfg->service_loop_en;
	route_ctrl_ext.bf.flow_pmtu_fail = cfg->ptmu_fail_action;
	route_ctrl_ext.bf.flow_pmtu_fail_de_acce = cfg->ptmu_fail_deacclr_en;
	route_ctrl_ext.bf.flow_pmtu_df_fail = cfg->ptmu_fail_df_action;
	route_ctrl_ext.bf.flow_pmtu_df_fail_de_acce = cfg->ptmu_fail_df_deacclr_en;
	route_ctrl_ext.bf.l2_vpn_en = cfg->l2_vpn_en;
	route_ctrl_ext.bf.l3_vpn_en = cfg->l3_vpn_en;

#if defined(JHPPE)
	if (cfg->flow_key_en_bitmap & FAL_FLOW_KEY_ROUTING)
		route_ctrl_ext.bf.routing_as_flow_key = 1;
	else
		route_ctrl_ext.bf.routing_as_flow_key = 0;

	if (cfg->flow_key_en_bitmap & FAL_FLOW_KEY_IPSEC_AH)
		route_ctrl_ext.bf.ipsec_ah_en = 1;
	else
		route_ctrl_ext.bf.ipsec_ah_en = 0;

	if (cfg->flow_key_en_bitmap & FAL_FLOW_KEY_IPSEC_ESP)
		route_ctrl_ext.bf.ipsec_esp_en = 1;
	else
		route_ctrl_ext.bf.ipsec_esp_en = 0;

	if (cfg->flow_key_en_bitmap & FAL_FLOW_KEY_IPSEC_NATT)
		route_ctrl_ext.bf.ipsec_natt_en = 1;
	else
		route_ctrl_ext.bf.ipsec_natt_en = 0;
#endif
	route_ctrl.bf.flow_service_code_loop = cfg->service_loop_action;
	route_ctrl.bf.flow_service_code_loop_de_acce = cfg->service_loop_deacclr_en;
	route_ctrl.bf.flow_de_acce_cmd = cfg->flow_deacclr_action;
	route_ctrl.bf.flow_sync_mismatch_cmd = cfg->sync_mismatch_action;
	route_ctrl.bf.flow_sync_mismatch_de_acce = cfg->sync_mismatch_deacclr_en;
	flow_ctrl0.bf.flow_hash_mode_0 = cfg->hash_mode_0;
	flow_ctrl0.bf.flow_hash_mode_1 = cfg->hash_mode_1;

	rv = hppe_flow_ctrl0_set(dev_id, &flow_ctrl0);
	SW_RTN_ON_ERROR(rv);

	rv = hppe_l3_route_ctrl_set(dev_id, &route_ctrl);
	SW_RTN_ON_ERROR(rv);

	rv = hppe_l3_route_ctrl_ext_set(dev_id, &route_ctrl_ext);
	SW_RTN_ON_ERROR(rv);

	if (ppe_type != MAX_PPE_TYPE && ppe_type != HPPE_TYPE) {
		rv = adpt_appe_flow_copy_escape_set(dev_id,
				cfg->flow_mismatch_copy_escape_en);
		SW_RTN_ON_ERROR(rv);
	}

#if defined(MPPE)
	if (ppe_type == MPPE_TYPE) {
		union eg_gen_ctrl_u eg_ctrl;
		memset(&eg_ctrl, 0, sizeof(eg_ctrl));

		rv = mppe_eg_gen_ctrl_get(dev_id, &eg_ctrl);
		SW_RTN_ON_ERROR(rv);

		eg_ctrl.bf.flow_cookie_pri = cfg->flow_cookie_pri;

		rv = mppe_eg_gen_ctrl_set(dev_id, &eg_ctrl);
		SW_RTN_ON_ERROR(rv);
	}
#endif

	return SW_OK;
}

/* clear ip6 address low bits */
fal_ip6_addr_t
ip6_clear_low_bits(fal_ip6_addr_t ip6, a_uint32_t len) 
{
	a_uint8_t i = 0;
	a_uint8_t ints_to_clear = len / 32;
	a_uint8_t bits_to_clear = len % 32;
	a_uint32_t *addr = ip6.ul;

	for (i = 0; i < ints_to_clear && i <= 3; i++) {
		addr[3 - i] = 0;
	}

	if (bits_to_clear > 0 && ints_to_clear <= 3) {
	    addr[3 - ints_to_clear] &= ~((1U << bits_to_clear) - 1);
	}

	return ip6;
}

/* one's complement checksum of ip6 array*/
a_uint16_t
ip6_csum(fal_ip6_addr_t ip6)
{
	a_uint32_t *addr = ip6.ul;
	a_uint32_t sum = 0;
	a_uint8_t i;

	for (i = 0; i < 4; i++) {
		sum += (addr[i] >> 16) & 0xFFFF;
		sum += addr[i] & 0xFFFF;
	}

	while (0xFFFF < sum) {
		sum += 1 - 0x10000;
	}

	return  (a_uint16_t)(~sum);
}

/* One's complement sum */
a_uint16_t
ip6_csum_add(a_uint16_t       csum1, a_uint16_t csum2)
{
	a_uint32_t result = csum1 + csum2;
	
	while (0xFFFF < result) {
		result += 1 - 0x10000;
	}

	 return (a_uint16_t)result;
}

/* One's complement difference */
a_uint16_t
ip6_csum_sub(a_uint16_t       csum1, a_uint16_t csum2)
{
	return ip6_csum_add(csum1, ~csum2);
}

/*search first non-0xffff from ip6_prefix_len*/
a_int32_t
ip6_offset_find(fal_ip6_addr_t ip6, a_uint32_t prefix_len)
{
	a_uint32_t *addr = ip6.ul, val;
	a_int32_t start;

	if(prefix_len <= 48)
		return 48;

	/*When prefix_len is in the range (49~64), start from 64*/
	start = ((prefix_len / 16) * 16) + ((prefix_len % 16)? 16:0);

	while(start < 128) {
		val = addr[start/32] >> (16 - (start%32));
		if ((val & 0xFFFF) != 0xFFFF)
			return start;
		start += 16;
	}

	return -1;
}

static int napt66_convert(fal_ip6_addr_t *ip6, union eg_ipv6_prefix_tbl_u *entry, a_bool_t to_hsl)
{
	if (to_hsl) {
		entry->bf.prefix_0 = ip6->ul[3];
		entry->bf.prefix_1 = ip6->ul[2];
		entry->bf.prefix_2 = ip6->ul[1];
		entry->bf.prefix_3 = ip6->ul[0];
	} else {
		ip6->ul[0] = entry->bf.prefix_3;
		ip6->ul[1] = entry->bf.prefix_2;
		ip6->ul[2] = entry->bf.prefix_1;
		ip6->ul[3] = entry->bf.prefix_0;
	}

	return 0;
}

sw_error_t
adpt_hppe_flow_npt66_prefix_add(a_uint32_t dev_id, a_uint32_t l3_if_index, fal_ip6_addr_t *ip6, a_uint32_t prefix_len)
{
	union eg_ipv6_prefix_tbl_u entry = { 0 };

	napt66_convert(ip6, &entry, A_TRUE);
	entry.bf.length = prefix_len;

	hppe_eg_ipv6_prefix_tbl_set(dev_id, l3_if_index, &entry);

	SSDK_DEBUG("**********%s:%d*****[%d]*%x:%x:%x:%x %d*******\n", __func__, __LINE__, l3_if_index, 
		entry.bf.prefix_0, entry.bf.prefix_1, entry.bf.prefix_2, entry.bf.prefix_3, entry.bf.length);

	return SW_OK;
	/*
		EG_IPV6_PREFIX_TBL[255:0]
			PREFIX	ipv6_addr		128
			LENGTH					7
	*/
}

sw_error_t
adpt_hppe_flow_npt66_prefix_get(a_uint32_t dev_id, a_uint32_t l3_if_index, fal_ip6_addr_t *ip6, a_uint32_t *prefix_len)
{
	union eg_ipv6_prefix_tbl_u entry = { 0 };

	hppe_eg_ipv6_prefix_tbl_get(dev_id, l3_if_index, &entry);

	napt66_convert(ip6, &entry, A_FALSE);
	*prefix_len = entry.bf.length;

	SSDK_DEBUG("**********%s:%d*****[%d]*%x:%x:%x:%x %d*******\n", __func__, __LINE__, l3_if_index, 
		entry.bf.prefix_0, entry.bf.prefix_1, entry.bf.prefix_2, entry.bf.prefix_3, entry.bf.length);

	return SW_OK;
}

sw_error_t
adpt_hppe_flow_npt66_prefix_del(a_uint32_t dev_id, a_uint32_t l3_if_index)
{
	union eg_ipv6_prefix_tbl_u entry = { 0 };

	hppe_eg_ipv6_prefix_tbl_set(dev_id, l3_if_index, &entry);

	SSDK_DEBUG("**********%s:%d*****[%d]*%x:%x:%x:%x %d*******\n", __func__, __LINE__, l3_if_index, 
		entry.bf.prefix_0, entry.bf.prefix_1, entry.bf.prefix_2, entry.bf.prefix_3, entry.bf.length);

	return SW_OK;
}

#define IP6_MAX_LEN 128
#define EG_FLOW_IPV6_IID_OFFSET_TO_FILED_VAL(offset) ((offset)/16)
sw_error_t
adpt_hppe_flow_npt66_iid_cal(a_uint32_t dev_id,               fal_flow_npt66_iid_calc_t *iid_cal, fal_flow_npt66_iid_t *iid_result)
{
	a_uint32_t prefix_len = iid_cal->prefix_len;
	a_uint32_t tip_prefix_len = iid_cal->tip_prefix_len;
	a_uint32_t clear_len = IP6_MAX_LEN-prefix_len;
	a_uint32_t tip_clear_len = IP6_MAX_LEN-tip_prefix_len;
	fal_ip6_addr_t org_ip = (iid_cal->is_dnat)?iid_cal->dip:iid_cal->sip;
	fal_ip6_addr_t tip = iid_cal->tip;
	a_uint16_t tip_csum, org_csum, iid_org, iid_val, csum_adj;
	a_uint32_t iid_offset = ip6_offset_find(org_ip, max(prefix_len, tip_prefix_len));
	
	if(iid_offset == -1)
		return SW_BAD_PARAM;
	
	tip_csum = ip6_csum(ip6_clear_low_bits(tip,tip_clear_len));
	org_csum = ip6_csum(ip6_clear_low_bits(org_ip,clear_len));
	iid_org = (org_ip.ul[iid_offset/32]>>(16-(iid_offset % 32)))&0xFFFF;
	csum_adj = ip6_csum_sub(tip_csum, org_csum);
	iid_val = ip6_csum_add(csum_adj, iid_org);

	iid_result->iid = (a_uint32_t)((iid_val==0xFFFF)? 0 : iid_val);
	iid_result->iid_offset = EG_FLOW_IPV6_IID_OFFSET_TO_FILED_VAL(iid_offset);
	iid_result->is_dnat = iid_cal->is_dnat;

	SSDK_DEBUG("iid calc is_dnat:%d org_ip:%08x:%08x:%08x:%08x tip:%08x:%08x:%08x:%08x prefix_len:%d tip_prefix_len:%d\n"
			"==>tip_csum:%04x - org_csum:%04x + iid_org:%04x = iid:%04x (offset:%d)\n",
			iid_result->is_dnat,org_ip.ul[0], org_ip.ul[1], org_ip.ul[2], org_ip.ul[3],
			tip.ul[0], tip.ul[1], tip.ul[2], tip.ul[3],	prefix_len, tip_prefix_len,
			tip_csum, org_csum, iid_org, iid_val, iid_offset);

	return SW_OK;
}


sw_error_t
adpt_hppe_flow_npt66_iid_add(a_uint32_t dev_id,   a_uint32_t flow_index, fal_flow_npt66_iid_t *iid)
{
	union eg_flow_ipv6_iid_tbl_u entry = { 0 };

	entry.bf.iid = iid->iid;
	entry.bf.offset =  iid->iid_offset;
	entry.bf.iid_update = 1;
	entry.bf.prefix_update = 1;
	entry.bf.src_dst = iid->is_dnat;
	hppe_eg_flow_ipv6_iid_tbl_set(dev_id, flow_index, &entry);

	SSDK_DEBUG("******%s:%d*****[%d]*%x:%x:%x:%x %d*******\n", __func__, __LINE__, flow_index, 
		entry.bf.iid, entry.bf.offset, entry.bf.iid_update, entry.bf.prefix_update, entry.bf.src_dst);

	return SW_OK;

	/*
		EG_FLOW_IPV6_IID_TBL[2047:0]
			IID 	16	0	Outbound: IID = External_csum(from table) - Internal_csm(from packet) + 16Bit_IID (src address)
						 	 Inbound: IID = Internal_csm(from table) - External_csum(from packet) + 16Bit_IID (dst address)
			OFFSET	3	0  "0: IPv6_addr[0..15]
							1: IPv6_addr[16..31]
							2: IPv6_addr[32..47]
							3: IPv6_addr[48..63]
							4: IPv6_addr[64..79]
							5: IPv6_addr[80..95]
							6: IPv6_addr[96..111]
							7: IPv6_addr[112..127]"
			IID_UPDATE		1	0	1: enable update
			PREFIX_UPDATE	1	0	1: enable update
			SRC_DST 		1	0	0: source address; 1: destination address
	*/
}
sw_error_t
adpt_hppe_flow_npt66_iid_get(a_uint32_t dev_id,   a_uint32_t flow_index, fal_flow_npt66_iid_t *iid)
{
	union eg_flow_ipv6_iid_tbl_u entry = { 0 };

	hppe_eg_flow_ipv6_iid_tbl_get(dev_id, flow_index, &entry);

	memset(iid, 0, sizeof(fal_flow_npt66_iid_t));

	if(	entry.bf.iid_update&&entry.bf.prefix_update) {
		iid->iid = entry.bf.iid;
		iid->iid_offset= entry.bf.offset;
		iid->is_dnat = entry.bf.src_dst;
	} else {
		return SW_NOT_FOUND;
	}

	SSDK_DEBUG("*******%s:%d*****[%d]*%x:%x:%x:%x %d*******\n", __func__, __LINE__, flow_index, 
		entry.bf.iid, entry.bf.offset, entry.bf.iid_update, entry.bf.prefix_update, entry.bf.src_dst);

	return SW_OK;
}

sw_error_t
adpt_hppe_flow_npt66_iid_del(a_uint32_t dev_id,   a_uint32_t flow_index)
{
	union eg_flow_ipv6_iid_tbl_u entry = { 0 };
	
	hppe_eg_flow_ipv6_iid_tbl_set(dev_id, flow_index, &entry);
	SSDK_DEBUG("******%s:%d*****[%d]*%x:%x:%x:%x %d*******\n", __func__, __LINE__, flow_index, 
		entry.bf.iid, entry.bf.offset, entry.bf.iid_update, entry.bf.prefix_update, entry.bf.src_dst);

	return SW_OK;
}

sw_error_t
adpt_hppe_flow_npt66_status_set(a_uint32_t dev_id, a_bool_t enable)
{
	union eg_global_ctrl_u eg_global_ctrl = {0};

	ADPT_DEV_ID_CHECK(dev_id);
	eg_global_ctrl.bf.prefix_xlt_en = enable;

	return hppe_eg_global_ctrl_set(dev_id, &eg_global_ctrl);
}

sw_error_t
adpt_hppe_flow_npt66_status_get(a_uint32_t dev_id, a_bool_t *enable)
{
	sw_error_t rv = SW_OK;
	union eg_global_ctrl_u eg_global_ctrl = {0};

	ADPT_DEV_ID_CHECK(dev_id);
	ADPT_NULL_POINT_CHECK(enable);

	rv = hppe_eg_global_ctrl_get(dev_id, &eg_global_ctrl);
	if( rv != SW_OK )
		return rv;

	*enable = eg_global_ctrl.bf.prefix_xlt_en;

	return SW_OK;
}

#if defined(JHPPE)
sw_error_t
adpt_ppe_flow_eip_lookup_mode_set(a_uint32_t dev_id, fal_flow_eip_lookup_mode_t mode)
{
	sw_error_t rv;
	union eg_bridge_config_u bridge_config;
	int eip_mode = 0;

	ADPT_DEV_ID_CHECK(dev_id);
	aos_mem_zero(&bridge_config, sizeof(union eg_bridge_config_u));

	rv = hppe_eg_bridge_config_get(dev_id, &bridge_config);
	SW_RTN_ON_ERROR(rv);

	switch (mode) {
	case FAL_FLOW_EIP_LOOKUP_MODE_FLOW:
		eip_mode = BIT(1);
		break;
	case FAL_FLOW_EIP_LOOKUP_MODE_TRANSFORM:
	default:
		eip_mode = 0;
		break;
	}

	bridge_config.bf.ppe_eip_rsv_w4_3130 = eip_mode;

	return hppe_eg_bridge_config_set(dev_id, &bridge_config);
}

sw_error_t
adpt_ppe_flow_eip_lookup_mode_get(a_uint32_t dev_id, fal_flow_eip_lookup_mode_t *mode)
{
	sw_error_t rv;
	union eg_bridge_config_u bridge_config;

	ADPT_DEV_ID_CHECK(dev_id);
	aos_mem_zero(&bridge_config, sizeof(union eg_bridge_config_u));

	rv = hppe_eg_bridge_config_get(dev_id, &bridge_config);
	SW_RTN_ON_ERROR(rv);

	if (bridge_config.bf.ppe_eip_rsv_w4_3130 & BIT(1))
		*mode = FAL_FLOW_EIP_LOOKUP_MODE_FLOW;
	else
		*mode = FAL_FLOW_EIP_LOOKUP_MODE_TRANSFORM;

	return SW_OK;
}
#endif

sw_error_t adpt_hppe_flow_init(a_uint32_t dev_id)
{
	adpt_api_t *p_adpt_api = NULL;

	p_adpt_api = adpt_api_ptr_get(dev_id);

	if(p_adpt_api == NULL)
		return SW_FAIL;

	p_adpt_api->adpt_flow_host_add = adpt_hppe_flow_host_add;
	p_adpt_api->adpt_flow_entry_get = adpt_hppe_flow_entry_get;
	p_adpt_api->adpt_flow_entry_del = adpt_hppe_flow_entry_del;
	p_adpt_api->adpt_flow_status_get = adpt_hppe_flow_status_get;
#if !defined(IN_FLOW_MINI)
	p_adpt_api->adpt_flow_age_timer_get = adpt_hppe_flow_age_timer_get;
	p_adpt_api->adpt_flow_age_timer_set = adpt_hppe_flow_age_timer_set;
#endif
	p_adpt_api->adpt_flow_status_set = adpt_hppe_flow_status_set;
	p_adpt_api->adpt_flow_host_get = adpt_hppe_flow_host_get;
	p_adpt_api->adpt_flow_host_del = adpt_hppe_flow_host_del;
	p_adpt_api->adpt_flow_entry_add = adpt_hppe_flow_entry_add;
	p_adpt_api->adpt_flow_global_cfg_get = adpt_hppe_flow_global_cfg_get;
	p_adpt_api->adpt_flow_global_cfg_set = adpt_hppe_flow_global_cfg_set;
	p_adpt_api->adpt_flow_entry_next = adpt_hppe_flow_entry_next;
	p_adpt_api->adpt_flow_counter_get = adpt_hppe_flow_counter_get;
	p_adpt_api->adpt_flow_counter_cleanup = adpt_hppe_flow_counter_cleanup;
	p_adpt_api->adpt_flow_entry_en_set = adpt_hppe_flow_entry_en_set;
	p_adpt_api->adpt_flow_entry_en_get = adpt_hppe_flow_entry_en_get;
	p_adpt_api->adpt_flow_qos_set = adpt_hppe_flow_qos_set;
	p_adpt_api->adpt_flow_qos_get = adpt_hppe_flow_qos_get;
	p_adpt_api->adpt_flow_ctrl_get = adpt_hppe_flow_ctrl_get;
	p_adpt_api->adpt_flow_ctrl_set = adpt_hppe_flow_ctrl_set;
	p_adpt_api->adpt_flow_npt66_prefix_add = adpt_hppe_flow_npt66_prefix_add;
	p_adpt_api->adpt_flow_npt66_prefix_get = adpt_hppe_flow_npt66_prefix_get;
	p_adpt_api->adpt_flow_npt66_prefix_del = adpt_hppe_flow_npt66_prefix_del;
	p_adpt_api->adpt_flow_npt66_iid_cal = adpt_hppe_flow_npt66_iid_cal;
	p_adpt_api->adpt_flow_npt66_iid_add = adpt_hppe_flow_npt66_iid_add;
	p_adpt_api->adpt_flow_npt66_iid_get = adpt_hppe_flow_npt66_iid_get;
	p_adpt_api->adpt_flow_npt66_iid_del = adpt_hppe_flow_npt66_iid_del;
	p_adpt_api->adpt_flow_npt66_status_set = adpt_hppe_flow_npt66_status_set;
	p_adpt_api->adpt_flow_npt66_status_get = adpt_hppe_flow_npt66_status_get;
#if defined(JHPPE)
	p_adpt_api->adpt_flow_eip_lookup_mode_set = adpt_ppe_flow_eip_lookup_mode_set;
	p_adpt_api->adpt_flow_eip_lookup_mode_get = adpt_ppe_flow_eip_lookup_mode_get;
#endif
	p_adpt_api->adpt_flow_fwd_type_set = adpt_ppe_flow_fwd_type_set;
	p_adpt_api->adpt_flow_fwd_type_get = adpt_ppe_flow_fwd_type_get;
#if defined(JHPPE)
	p_adpt_api->adpt_flow_key_get = adpt_jhppe_flow_key_get;
	p_adpt_api->adpt_flow_key_set = adpt_jhppe_flow_key_set;
	p_adpt_api->adpt_flow_sampling_id_get = adpt_jhppe_flow_sampling_id_get;
	p_adpt_api->adpt_flow_sampling_id_set = adpt_jhppe_flow_sampling_id_set;
	p_adpt_api->adpt_flow_gro_en_get = adpt_jhppe_flow_gro_en_get;
	p_adpt_api->adpt_flow_gro_en_set = adpt_jhppe_flow_gro_en_set;
	p_adpt_api->adpt_flow_app_entry_add = adpt_jhppe_flow_app_entry_add;
	p_adpt_api->adpt_flow_app_entry_get = adpt_jhppe_flow_app_entry_get;
	p_adpt_api->adpt_flow_app_entry_del = adpt_jhppe_flow_app_entry_del;
#endif

#if defined(MRPPE)
	bitmap_zero(flow_cookie_48bit, EG_FLOW_TREE_MAP_TBL_NUM);
#endif

	return SW_OK;
}

/**
 * @}
 */
