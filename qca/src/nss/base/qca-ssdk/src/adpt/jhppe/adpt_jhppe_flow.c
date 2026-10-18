/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: ISC
 */

/**
 * @defgroup
 * @{
 */
#include "sw.h"
#include "hsl_reg.h"
#include "fal_flow.h"
#include "adpt.h"

#define FLOW_ENTRY_TYPE_IPV4 0
#define FLOW_ENTRY_TYPE_IPV6 1
#define FLOW_TUPLE_TYPE_3    0

sw_error_t adpt_jhppe_flow_key_get(a_uint32_t dev_id,
				   fal_flow_protocol_type_t key_type,
				   fal_flow_key_t *flow_key)
{
	union l3_flow_key_gen_u key_gen;
	union l3_flow_key_sel_u key_sel;
	sw_error_t rv = SW_OK;

	ADPT_DEV_ID_CHECK(dev_id);
	ADPT_NULL_POINT_CHECK(flow_key);

	if (FAL_FLOW_KEY_GEN0 != key_type && FAL_FLOW_KEY_GEN1 != key_type)
		return SW_OUT_OF_RANGE;

	key_type -= FAL_FLOW_KEY_GEN0;

	aos_mem_zero(&key_gen, sizeof(union l3_flow_key_gen_u));
	aos_mem_zero(&key_sel, sizeof(union l3_flow_key_sel_u));

	rv = jhppe_l3_flow_key_gen_get(dev_id, key_type, &key_gen);
	SW_RTN_ON_ERROR(rv);

	rv = jhppe_l3_flow_key_sel_get(dev_id, key_type, &key_sel);
	SW_RTN_ON_ERROR(rv);

	flow_key->valid = key_gen.bf.valid;
	flow_key->key_bmp = 0;

	if (key_gen.bf.l3_type_vld)
		flow_key->key_bmp |= BIT(FAL_FLOW_KEY_L3_TYPE);

	if (key_gen.bf.l4_type_vld)
		flow_key->key_bmp |= BIT(FAL_FLOW_KEY_L4_TYPE);

	if (key_gen.bf.app_type_vld)
		flow_key->key_bmp |= BIT(FAL_FLOW_KEY_APP_TYPE);

	if (key_sel.bf.sip_inc)
		flow_key->key_bmp |= BIT(FAL_FLOW_KEY_SIP);

	if (key_sel.bf.dip_inc)
		flow_key->key_bmp |= BIT(FAL_FLOW_KEY_DIP);

	if (key_sel.bf.ip_prot_inc)
		flow_key->key_bmp |= BIT(FAL_FLOW_KEY_IP_PROTOCOL);

	if (key_sel.bf.sport_inc)
		flow_key->key_bmp |= BIT(FAL_FLOW_KEY_SPORT);

	if (key_sel.bf.dport_inc)
		flow_key->key_bmp |= BIT(FAL_FLOW_KEY_DPORT);

	if (key_sel.bf.udf0_inc)
		flow_key->key_bmp |= BIT(FAL_FLOW_KEY_UDF0);

	if (key_sel.bf.udf1_inc)
		flow_key->key_bmp |= BIT(FAL_FLOW_KEY_UDF1);

	flow_key->l3_type = key_gen.bf.l3_type;
	flow_key->l4_type = key_gen.bf.l4_type;
	flow_key->app_type = key_gen.bf.app_type;
	flow_key->udf0_idx = key_sel.bf.udf0_id;
	flow_key->udf0_mask = key_sel.bf.udf0_mask;
	flow_key->udf1_idx = key_sel.bf.udf1_id;
	flow_key->udf1_mask = key_sel.bf.udf1_mask;

	return SW_OK;
}

sw_error_t adpt_jhppe_flow_key_set(a_uint32_t dev_id,
				   fal_flow_protocol_type_t key_type,
				   fal_flow_key_t *flow_key)
{
	union l3_flow_key_gen_u key_gen;
	union l3_flow_key_sel_u key_sel;
	sw_error_t rv = SW_OK;

	ADPT_DEV_ID_CHECK(dev_id);
	ADPT_NULL_POINT_CHECK(flow_key);

	if (FAL_FLOW_KEY_GEN0 != key_type && FAL_FLOW_KEY_GEN1 != key_type)
		return SW_OUT_OF_RANGE;

	key_type -= FAL_FLOW_KEY_GEN0;

	aos_mem_zero(&key_gen, sizeof(union l3_flow_key_gen_u));
	aos_mem_zero(&key_sel, sizeof(union l3_flow_key_sel_u));

	rv = jhppe_l3_flow_key_gen_get(dev_id, key_type, &key_gen);
	SW_RTN_ON_ERROR(rv);

	rv = jhppe_l3_flow_key_sel_get(dev_id, key_type, &key_sel);
	SW_RTN_ON_ERROR(rv);

	if (flow_key->key_bmp & BIT(FAL_FLOW_KEY_L3_TYPE))
		key_gen.bf.l3_type_vld = 1;
	else
		key_gen.bf.l3_type_vld = 0;

	if (flow_key->key_bmp & BIT(FAL_FLOW_KEY_L4_TYPE))
		key_gen.bf.l4_type_vld = 1;
	else
		key_gen.bf.l4_type_vld = 0;

	if (flow_key->key_bmp & BIT(FAL_FLOW_KEY_APP_TYPE))
		key_gen.bf.app_type_vld = 1;
	else
		key_gen.bf.app_type_vld = 0;

	if (flow_key->key_bmp & BIT(FAL_FLOW_KEY_SIP))
		key_sel.bf.sip_inc = 1;
	else
		key_sel.bf.sip_inc = 0;

	if (flow_key->key_bmp & BIT(FAL_FLOW_KEY_DIP))
		key_sel.bf.dip_inc = 1;
	else
		key_sel.bf.dip_inc = 0;

	if (flow_key->key_bmp & BIT(FAL_FLOW_KEY_IP_PROTOCOL))
		key_sel.bf.ip_prot_inc = 1;
	else
		key_sel.bf.ip_prot_inc = 0;

	if (flow_key->key_bmp & BIT(FAL_FLOW_KEY_SPORT))
		key_sel.bf.sport_inc = 1;
	else
		key_sel.bf.sport_inc = 0;

	if (flow_key->key_bmp & BIT(FAL_FLOW_KEY_DPORT))
		key_sel.bf.dport_inc = 1;
	else
		key_sel.bf.dport_inc = 0;

	if (flow_key->key_bmp & BIT(FAL_FLOW_KEY_UDF0))
		key_sel.bf.udf0_inc = 1;
	else
		key_sel.bf.udf0_inc = 0;

	if (flow_key->key_bmp & BIT(FAL_FLOW_KEY_UDF1))
		key_sel.bf.udf1_inc = 1;
	else
		key_sel.bf.udf1_inc = 0;

	key_gen.bf.l3_type = flow_key->l3_type;
	key_gen.bf.l4_type = flow_key->l4_type;
	key_gen.bf.app_type = flow_key->app_type;
	key_sel.bf.udf0_id = flow_key->udf0_idx;
	key_sel.bf.udf0_mask = flow_key->udf0_mask;
	key_sel.bf.udf1_id = flow_key->udf1_idx;
	key_sel.bf.udf1_mask = flow_key->udf1_mask;

	rv = jhppe_l3_flow_key_gen_set(dev_id, key_type, &key_gen);
	SW_RTN_ON_ERROR(rv);

	return jhppe_l3_flow_key_sel_set(dev_id, key_type, &key_sel);
}

sw_error_t adpt_jhppe_flow_sampling_id_get(a_uint32_t dev_id,
					   a_uint32_t flow_index,
					   a_uint32_t *sampling_id)
{
	sw_error_t rv = SW_OK;
	union in_flow_tbl_u entry;
	a_uint32_t op_index;
	a_bool_t is_ipv6, is_3tuple, is_6tuple;

	ADPT_DEV_ID_CHECK(dev_id);
	ADPT_NULL_POINT_CHECK(sampling_id);

	if (flow_index >= IN_FLOW_TBL_NUM)
		return SW_OUT_OF_RANGE;

	aos_mem_zero(&entry, sizeof(entry));

	op_index = flow_index;
	rv = hppe_flow_ipv4_5tuple_get(dev_id, INDEX_MODE,
					&op_index, &entry);
	SW_RTN_ON_ERROR(rv);

	is_ipv6 = (entry.bf.entry_type == FLOW_ENTRY_TYPE_IPV6);
	is_3tuple = (entry.bf.protocol_type == FLOW_TUPLE_TYPE_3);
	is_6tuple = (entry.bf.protocol_type == FAL_FLOW_NATT ||
		     entry.bf.protocol_type == FAL_FLOW_KEY_GEN0 ||
		     entry.bf.protocol_type == FAL_FLOW_KEY_GEN1);

	if ((is_ipv6 || is_6tuple) && (flow_index & 0x1))
		return SW_BAD_PARAM;

	if (is_6tuple) {
		union in_flow_6tuple_tbl_u wide_entry;

		aos_mem_zero(&wide_entry, sizeof(wide_entry));

		op_index = flow_index;
		rv = jhppe_flow_ip_6tuple_get(dev_id, INDEX_MODE,
					      &op_index, &wide_entry);
		SW_RTN_ON_ERROR(rv);

		*sampling_id = wide_entry.bf.counter_id_0;
		*sampling_id |= wide_entry.bf.counter_id_1 <<
			SW_FIELD_OFFSET_IN_WORD(IN_FLOW_6TUPLE_TBL_COUNTER_ID_OFFSET);
	} else if (is_ipv6 && is_3tuple) {
		union in_flow_ipv6_3tuple_tbl_u wide_entry;

		aos_mem_zero(&wide_entry, sizeof(wide_entry));

		op_index = flow_index;
		rv = hppe_flow_ipv6_3tuple_get(dev_id, INDEX_MODE,
						&op_index, &wide_entry);
		SW_RTN_ON_ERROR(rv);

		*sampling_id = wide_entry.bf.counter_id_0;
		*sampling_id |= wide_entry.bf.counter_id_1 <<
			SW_FIELD_OFFSET_IN_WORD(IN_FLOW_IPV6_3TUPLE_TBL_COUNTER_ID_OFFSET);
	} else if (is_ipv6) {
		union in_flow_ipv6_5tuple_tbl_u wide_entry;

		aos_mem_zero(&wide_entry, sizeof(wide_entry));

		op_index = flow_index;
		rv = hppe_flow_ipv6_5tuple_get(dev_id, INDEX_MODE,
						&op_index, &wide_entry);
		SW_RTN_ON_ERROR(rv);

		*sampling_id = wide_entry.bf.counter_id_0;
		*sampling_id |= wide_entry.bf.counter_id_1 <<
			SW_FIELD_OFFSET_IN_WORD(IN_FLOW_IPV6_5TUPLE_TBL_COUNTER_ID_OFFSET);
	} else if (is_3tuple) {
		union in_flow_3tuple_tbl_u narrow_entry;

		aos_mem_zero(&narrow_entry, sizeof(narrow_entry));

		op_index = flow_index;
		rv = hppe_flow_ipv4_3tuple_get(dev_id, INDEX_MODE,
						&op_index, &narrow_entry);
		SW_RTN_ON_ERROR(rv);

		*sampling_id = narrow_entry.bf.counter_id;
	} else {
		*sampling_id = entry.bf.counter_id;
	}

	return SW_OK;
}

sw_error_t adpt_jhppe_flow_sampling_id_set(a_uint32_t dev_id,
					   a_uint32_t flow_index,
					   a_uint32_t sampling_id)
{
	sw_error_t rv = SW_OK;
	union in_flow_tbl_u entry;
	a_uint32_t op_index;
	a_bool_t is_ipv6, is_3tuple, is_6tuple;

	ADPT_DEV_ID_CHECK(dev_id);

	if (flow_index >= IN_FLOW_TBL_NUM)
		return SW_OUT_OF_RANGE;

	aos_mem_zero(&entry, sizeof(entry));

	op_index = flow_index;
	rv = hppe_flow_ipv4_5tuple_get(dev_id, INDEX_MODE,
					&op_index, &entry);
	SW_RTN_ON_ERROR(rv);

	is_ipv6 = (entry.bf.entry_type == FLOW_ENTRY_TYPE_IPV6);
	is_3tuple = (entry.bf.protocol_type == FLOW_TUPLE_TYPE_3);
	is_6tuple = (entry.bf.protocol_type == FAL_FLOW_NATT ||
		     entry.bf.protocol_type == FAL_FLOW_KEY_GEN0 ||
		     entry.bf.protocol_type == FAL_FLOW_KEY_GEN1);

	if ((is_ipv6 || is_6tuple) && (flow_index & 0x1))
		return SW_BAD_PARAM;

	if (is_6tuple) {
		union in_flow_6tuple_tbl_u wide_entry;

		aos_mem_zero(&wide_entry, sizeof(wide_entry));

		op_index = flow_index;
		rv = jhppe_flow_ip_6tuple_get(dev_id, INDEX_MODE,
					      &op_index, &wide_entry);
		SW_RTN_ON_ERROR(rv);

		wide_entry.bf.counter_id_0 = sampling_id;
		wide_entry.bf.counter_id_1 = sampling_id >>
			SW_FIELD_OFFSET_IN_WORD(IN_FLOW_6TUPLE_TBL_COUNTER_ID_OFFSET);

		op_index = flow_index;
		rv = jhppe_flow_ip_6tuple_add(dev_id, HASH_MODE,
					      &op_index, &wide_entry);
	} else if (is_ipv6 && is_3tuple) {
		union in_flow_ipv6_3tuple_tbl_u wide_entry;

		aos_mem_zero(&wide_entry, sizeof(wide_entry));

		op_index = flow_index;
		rv = hppe_flow_ipv6_3tuple_get(dev_id, INDEX_MODE,
						&op_index, &wide_entry);
		SW_RTN_ON_ERROR(rv);

		wide_entry.bf.counter_id_0 = sampling_id;
		wide_entry.bf.counter_id_1 = sampling_id >>
			SW_FIELD_OFFSET_IN_WORD(IN_FLOW_IPV6_3TUPLE_TBL_COUNTER_ID_OFFSET);

		op_index = flow_index;
		rv = hppe_flow_ipv6_3tuple_add(dev_id, HASH_MODE,
						&op_index, &wide_entry);
	} else if (is_ipv6) {
		union in_flow_ipv6_5tuple_tbl_u wide_entry;

		aos_mem_zero(&wide_entry, sizeof(wide_entry));

		op_index = flow_index;
		rv = hppe_flow_ipv6_5tuple_get(dev_id, INDEX_MODE,
						&op_index, &wide_entry);
		SW_RTN_ON_ERROR(rv);

		wide_entry.bf.counter_id_0 = sampling_id;
		wide_entry.bf.counter_id_1 = sampling_id >>
			SW_FIELD_OFFSET_IN_WORD(IN_FLOW_IPV6_5TUPLE_TBL_COUNTER_ID_OFFSET);

		op_index = flow_index;
		rv = hppe_flow_ipv6_5tuple_add(dev_id, HASH_MODE,
						&op_index, &wide_entry);
	} else if (is_3tuple) {
		union in_flow_3tuple_tbl_u narrow_entry;

		aos_mem_zero(&narrow_entry, sizeof(narrow_entry));

		op_index = flow_index;
		rv = hppe_flow_ipv4_3tuple_get(dev_id, INDEX_MODE,
						&op_index, &narrow_entry);
		SW_RTN_ON_ERROR(rv);

		narrow_entry.bf.counter_id = sampling_id;

		op_index = flow_index;
		rv = hppe_flow_ipv4_3tuple_add(dev_id, HASH_MODE,
						&op_index, &narrow_entry);
	} else {
		entry.bf.counter_id = sampling_id;

		op_index = flow_index;
		rv = hppe_flow_ipv4_5tuple_add(dev_id, HASH_MODE,
						&op_index, &entry);
	}

	return rv;
}

sw_error_t adpt_jhppe_flow_gro_en_get(a_uint32_t dev_id,
				      a_uint32_t flow_index,
				      a_bool_t *enable)
{
	sw_error_t rv = SW_OK;
	union eg_flow_tree_map_tbl_u entry;

	ADPT_DEV_ID_CHECK(dev_id);
	ADPT_NULL_POINT_CHECK(enable);

	if (flow_index >= EG_FLOW_TREE_MAP_TBL_NUM)
		return SW_OUT_OF_RANGE;

	aos_mem_zero(&entry, sizeof(entry));

	rv = hppe_eg_flow_tree_map_tbl_get(dev_id, flow_index, &entry);
	SW_RTN_ON_ERROR(rv);

	*enable = entry.bf.gro;

	return rv;
}

sw_error_t adpt_jhppe_flow_gro_en_set(a_uint32_t dev_id,
				      a_uint32_t flow_index,
				      a_bool_t enable)
{
	sw_error_t rv = SW_OK;
	union eg_flow_tree_map_tbl_u entry;

	ADPT_DEV_ID_CHECK(dev_id);

	if (flow_index >= EG_FLOW_TREE_MAP_TBL_NUM)
		return SW_OUT_OF_RANGE;

	aos_mem_zero(&entry, sizeof(entry));

	rv = hppe_eg_flow_tree_map_tbl_get(dev_id, flow_index, &entry);
	SW_RTN_ON_ERROR(rv);

	entry.bf.gro = enable;

	return hppe_eg_flow_tree_map_tbl_set(dev_id, flow_index, &entry);
}

sw_error_t adpt_jhppe_flow_app_entry_add(a_uint32_t dev_id,
					 fal_flow_app_entry_t *entry)
{
	union app_udp_port_cfg_u udp_config;
	union nat_t_cfg_u natt_cfg;
	sw_error_t rv = SW_OK;

	ADPT_DEV_ID_CHECK(dev_id);
	ADPT_NULL_POINT_CHECK(entry);

	aos_mem_zero(&udp_config, sizeof(udp_config));
	aos_mem_zero(&natt_cfg, sizeof(natt_cfg));

	rv = jhppe_app_udp_port_cfg_get(dev_id, &udp_config);
	SW_RTN_ON_ERROR(rv);

	rv = jhppe_nat_t_cfg_get(dev_id, &natt_cfg);
	SW_RTN_ON_ERROR(rv);

	udp_config.bf.ip_ver = entry->ip_ver;
	udp_config.bf.udp_type = entry->udp_type;
	udp_config.bf.port_type = entry->l4_port_type;
	udp_config.bf.port_value = entry->l4_port;

	natt_cfg.bf.app_udp_port_map = 1;

	rv = jhppe_app_udp_port_cfg_set(dev_id, &udp_config);
	SW_RTN_ON_ERROR(rv);

	return jhppe_nat_t_cfg_set(dev_id, &natt_cfg);
}

sw_error_t adpt_jhppe_flow_app_entry_get(a_uint32_t dev_id,
					 fal_flow_app_entry_t *entry)
{
	union app_udp_port_cfg_u udp_config;
	union nat_t_cfg_u natt_cfg;
	sw_error_t rv = SW_OK;

	ADPT_DEV_ID_CHECK(dev_id);
	ADPT_NULL_POINT_CHECK(entry);

	aos_mem_zero(&udp_config, sizeof(udp_config));
	aos_mem_zero(&natt_cfg, sizeof(natt_cfg));

	rv = jhppe_app_udp_port_cfg_get(dev_id, &udp_config);
	SW_RTN_ON_ERROR(rv);

	rv = jhppe_nat_t_cfg_get(dev_id, &natt_cfg);
	SW_RTN_ON_ERROR(rv);

	if (natt_cfg.bf.app_udp_port_map == 1) {
		entry->ip_ver = udp_config.bf.ip_ver;
		entry->udp_type = udp_config.bf.udp_type;
		entry->l4_port_type = udp_config.bf.port_type;
		entry->l4_port = udp_config.bf.port_value;
	}

	return SW_OK;
}

sw_error_t adpt_jhppe_flow_app_entry_del(a_uint32_t dev_id,
					 fal_flow_app_entry_t *entry)
{
	union app_udp_port_cfg_u udp_config;
	union nat_t_cfg_u natt_cfg;
	sw_error_t rv = SW_OK;

	ADPT_DEV_ID_CHECK(dev_id);
	ADPT_NULL_POINT_CHECK(entry);

	aos_mem_zero(&udp_config, sizeof(udp_config));
	aos_mem_zero(&natt_cfg, sizeof(natt_cfg));

	rv = jhppe_nat_t_cfg_get(dev_id, &natt_cfg);
	SW_RTN_ON_ERROR(rv);

	natt_cfg.bf.app_udp_port_map = 0;

	rv = jhppe_app_udp_port_cfg_set(dev_id, &udp_config);
	SW_RTN_ON_ERROR(rv);

	return jhppe_nat_t_cfg_set(dev_id, &natt_cfg);
}
/**
 * @}
 */
