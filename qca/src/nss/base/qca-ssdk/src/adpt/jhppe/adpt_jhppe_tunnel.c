/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: ISC
 */

/**
 * @defgroup
 * @{
 */

#include "hsl_reg.h"
#include "fal_tunnel.h"
#include "adpt.h"

/* tuple addr entry type */
enum {
	ADPT_TUPLE_SIP_ENTRY,
	ADPT_TUPLE_DIP_ENTRY,
};

/* tuple port entry type */
enum {
	ADPT_TUPLE_SPORT_ENTRY,
	ADPT_TUPLE_DPORT_ENTRY,
	ADPT_TUPLE_PROTO_ENTRY,
};

#ifndef IN_TUNNEL_MINI
static sw_error_t
_adpt_jhppe_tunnel_tuple_addr_entry_get(a_uint32_t dev_id, a_uint32_t index,
		fal_tunnel_tuple_entry_t *entry)
{
	union tpr_tuple_ip_addr_ctrl_u ip_addr_ctrl = {0};
	a_uint32_t addr[4];
	sw_error_t rv;

	rv = jhppe_tpr_tuple_ip_addr_ctrl_get(dev_id, index, &ip_addr_ctrl);
	SW_RTN_ON_ERROR(rv);

	entry->ip_ver = ip_addr_ctrl.bf.ver;
	if (entry->ip_ver == FAL_TUNNEL_IP_VER_4) {
		rv = jhppe_tpr_tuple_ip_addr_0_value_get(dev_id, index, &addr[0]);
		SW_RTN_ON_ERROR(rv);

		switch (ip_addr_ctrl.bf.type) {
		case ADPT_TUPLE_SIP_ENTRY:
			/* sip */
			entry->key_bmp |= BIT(FAL_TUNNEL_KEY_SIP_EN);
			entry->sip.ip4_addr = addr[0];
			break;
		case ADPT_TUPLE_DIP_ENTRY:
			/* dip */
			entry->key_bmp |= BIT(FAL_TUNNEL_KEY_DIP_EN);
			entry->dip.ip4_addr = addr[0];
			break;
		default:
			return SW_BAD_PARAM;
		}
	} else {
		rv = jhppe_tpr_tuple_ip_addr_0_value_get(dev_id, index, &addr[0]);
		SW_RTN_ON_ERROR(rv);
		rv = jhppe_tpr_tuple_ip_addr_1_value_get(dev_id, index, &addr[1]);
		SW_RTN_ON_ERROR(rv);
		rv = jhppe_tpr_tuple_ip_addr_2_value_get(dev_id, index, &addr[2]);
		SW_RTN_ON_ERROR(rv);
		rv = jhppe_tpr_tuple_ip_addr_3_value_get(dev_id, index, &addr[3]);
		SW_RTN_ON_ERROR(rv);

		switch (ip_addr_ctrl.bf.type) {
		case ADPT_TUPLE_SIP_ENTRY:
			/* sip */
			entry->key_bmp |= BIT(FAL_TUNNEL_KEY_SIP_EN);
			entry->sip.ip6_addr.ul[0] = addr[3];
			entry->sip.ip6_addr.ul[1] = addr[2];
			entry->sip.ip6_addr.ul[2] = addr[1];
			entry->sip.ip6_addr.ul[3] = addr[0];
			break;
		case ADPT_TUPLE_DIP_ENTRY:
			/* dip */
			entry->key_bmp |= BIT(FAL_TUNNEL_KEY_DIP_EN);
			entry->dip.ip6_addr.ul[0] = addr[3];
			entry->dip.ip6_addr.ul[1] = addr[2];
			entry->dip.ip6_addr.ul[2] = addr[1];
			entry->dip.ip6_addr.ul[3] = addr[0];
			break;
		default:
			return SW_BAD_PARAM;
		}
	}

	return SW_OK;
}

static sw_error_t
_adpt_jhppe_tunnel_tuple_port_entry_get(a_uint32_t dev_id, a_uint32_t index,
		fal_tunnel_tuple_entry_t *entry)
{
	union tpr_tuple_l4_port_u l4_port = {0};
	sw_error_t rv;

	rv = jhppe_tpr_tuple_l4_port_get(dev_id, index, &l4_port);
	SW_RTN_ON_ERROR(rv);

	switch (l4_port.bf.port_type) {
	case ADPT_TUPLE_SPORT_ENTRY:
		entry->key_bmp |= BIT(FAL_TUNNEL_KEY_SPORT_EN);
		entry->sport = l4_port.bf.port_value;
		break;
	case ADPT_TUPLE_DPORT_ENTRY:
		entry->key_bmp |= BIT(FAL_TUNNEL_KEY_DPORT_EN);
		entry->dport = l4_port.bf.port_value;
		break;
	case ADPT_TUPLE_PROTO_ENTRY:
		entry->key_bmp |= BIT(FAL_TUNNEL_KEY_L4PROTO_EN);
		entry->l4_proto = l4_port.bf.port_value;
		break;
	default:
		return SW_BAD_PARAM;
	}

	return SW_OK;
}

static sw_error_t
_adpt_jhppe_tunnel_tuple_addr_port_entry_get(a_uint32_t dev_id,
	a_uint32_t *addr_map, a_uint32_t *port_map, fal_tunnel_tuple_entry_t *entry)

{
	sw_error_t rv;
	int i;

	for_each_set_bit(i, (const unsigned long *)addr_map, TPR_TUPLE_IP_ADDR_CTRL_NUM) {
		rv = _adpt_jhppe_tunnel_tuple_addr_entry_get(dev_id, i, entry);
		SW_RTN_ON_ERROR(rv);
	}

	for_each_set_bit(i, (const unsigned long *)port_map, TPR_TUPLE_L4_PORT_NUM) {
		rv = _adpt_jhppe_tunnel_tuple_port_entry_get(dev_id, i, entry);
		SW_RTN_ON_ERROR(rv);
	}

	return SW_OK;
}
#endif

static sw_error_t
_adpt_jhppe_tunnel_tuple_new_addr_entry_set(a_uint32_t dev_id, fal_tunnel_tuple_entry_t *entry,
		a_uint32_t type, a_uint32_t *free_addr_map, a_uint32_t *addr_map)
{
	union tpr_tuple_ip_addr_ctrl_u ip_addr_ctrl = {0};
	a_uint32_t addr[4];
	sw_error_t rv;
	int i;

	i = find_first_bit((const unsigned long *)free_addr_map, TPR_TUPLE_IP_ADDR_CTRL_NUM);
	if (i == TPR_TUPLE_IP_ADDR_CTRL_NUM) {
		return SW_NO_RESOURCE;
	}

	ip_addr_ctrl.bf.type = type;
	ip_addr_ctrl.bf.ver = entry->ip_ver;
	rv = jhppe_tpr_tuple_ip_addr_ctrl_set(dev_id, i, &ip_addr_ctrl);
	SW_RTN_ON_ERROR(rv);

	if (entry->ip_ver == FAL_TUNNEL_IP_VER_4) {
		/* ipv4 */
		switch (type) {
		case ADPT_TUPLE_SIP_ENTRY:
			/* sip */
			addr[0] = entry->sip.ip4_addr;
			break;
		case ADPT_TUPLE_DIP_ENTRY:
			/* dip */
			addr[0] = entry->dip.ip4_addr;
			break;
		default:
			return SW_BAD_PARAM;
		}

		rv = jhppe_tpr_tuple_ip_addr_0_value_set(dev_id, i, addr[0]);
		SW_RTN_ON_ERROR(rv);
	} else {
		/* ipv6 */
		switch (type) {
		case ADPT_TUPLE_SIP_ENTRY:
			/* sip */
			addr[0] = entry->sip.ip6_addr.ul[3];
			addr[1] = entry->sip.ip6_addr.ul[2];
			addr[2] = entry->sip.ip6_addr.ul[1];
			addr[3] = entry->sip.ip6_addr.ul[0];
			break;
		case ADPT_TUPLE_DIP_ENTRY:
			/* dip */
			addr[0] = entry->dip.ip6_addr.ul[3];
			addr[1] = entry->dip.ip6_addr.ul[2];
			addr[2] = entry->dip.ip6_addr.ul[1];
			addr[3] = entry->dip.ip6_addr.ul[0];
			break;
		default:
			return SW_BAD_PARAM;
		}

		rv = jhppe_tpr_tuple_ip_addr_0_value_set(dev_id, i, addr[0]);
		SW_RTN_ON_ERROR(rv);
		rv = jhppe_tpr_tuple_ip_addr_1_value_set(dev_id, i, addr[1]);
		SW_RTN_ON_ERROR(rv);
		rv = jhppe_tpr_tuple_ip_addr_2_value_set(dev_id, i, addr[2]);
		SW_RTN_ON_ERROR(rv);
		rv = jhppe_tpr_tuple_ip_addr_3_value_set(dev_id, i, addr[3]);
		SW_RTN_ON_ERROR(rv);
	}

	*addr_map = BIT(i);
	*free_addr_map = *free_addr_map & (*free_addr_map - 1);

	return SW_OK;
}

static sw_error_t
_adpt_jhppe_tunnel_tuple_new_port_entry_set(a_uint32_t dev_id, fal_tunnel_tuple_entry_t *entry,
		a_uint32_t type, a_uint32_t *free_port_map, a_uint32_t *port_map)
{
	union tpr_tuple_l4_port_u l4_port = {0};
	a_uint32_t port_value;
	sw_error_t rv;
	int i = 0;

	i = find_first_bit((const unsigned long *)free_port_map, TPR_TUPLE_L4_PORT_NUM);
	if (i == TPR_TUPLE_L4_PORT_NUM)
		return SW_NO_RESOURCE;

	l4_port.bf.port_type = type;

	switch (type) {
	case ADPT_TUPLE_SPORT_ENTRY:
		port_value = entry->sport;
		break;
	case ADPT_TUPLE_DPORT_ENTRY:
		port_value = entry->dport;
		break;
	case ADPT_TUPLE_PROTO_ENTRY:
		port_value = entry->l4_proto;
		break;
	default:
		return SW_BAD_PARAM;
	}

	l4_port.bf.port_value = port_value;

	rv = jhppe_tpr_tuple_l4_port_set(dev_id, i, &l4_port);
	SW_RTN_ON_ERROR(rv);

	*port_map = BIT(i);
	*free_port_map = *free_port_map & (*free_port_map - 1);

	return SW_OK;
}

static sw_error_t
_adpt_jhppe_tunnel_tuple_addr_entry_find(a_uint32_t dev_id, fal_tunnel_tuple_entry_t *entry,
	a_uint32_t *saddr_map, a_uint32_t *daddr_map, a_uint32_t *free_addr_map)
{
	union tpr_tuple_ip_addr_ctrl_u ip_addr_ctrl = {0};
	sw_error_t rv;
	a_uint32_t addr[4];
	int i;
	struct qca_phy_priv *priv = ssdk_phy_priv_data_get(dev_id);
	SW_RTN_ON_NULL(priv);

	for (i = 0; i < TPR_TUPLE_IP_ADDR_CTRL_NUM; i++) {
		if (priv->tnl_priv.tpr_tuple_ip_addr_entry_used_map[i] == 0) {
			*free_addr_map |= BIT(i);
			continue;
		}

		rv = jhppe_tpr_tuple_ip_addr_ctrl_get(dev_id, i, &ip_addr_ctrl);
		SW_RTN_ON_ERROR(rv);
		rv = jhppe_tpr_tuple_ip_addr_0_value_get(dev_id, i, &addr[0]);
		SW_RTN_ON_ERROR(rv);
		rv = jhppe_tpr_tuple_ip_addr_1_value_get(dev_id, i, &addr[1]);
		SW_RTN_ON_ERROR(rv);
		rv = jhppe_tpr_tuple_ip_addr_2_value_get(dev_id, i, &addr[2]);
		SW_RTN_ON_ERROR(rv);
		rv = jhppe_tpr_tuple_ip_addr_3_value_get(dev_id, i, &addr[3]);
		SW_RTN_ON_ERROR(rv);

		if ((entry->key_bmp & BIT(FAL_TUNNEL_KEY_SIP_EN)) &&
				(ip_addr_ctrl.bf.type == ADPT_TUPLE_SIP_ENTRY)) {
			if (entry->ip_ver == FAL_TUNNEL_IP_VER_4 &&
					ip_addr_ctrl.bf.ver == FAL_TUNNEL_IP_VER_4) {
				if (entry->sip.ip4_addr == addr[0]) {
					/* find sipv4 entry */
					*saddr_map = BIT(i);
				}
			} else if (entry->ip_ver == FAL_TUNNEL_IP_VER_6 &&
					ip_addr_ctrl.bf.ver == FAL_TUNNEL_IP_VER_6) {
				if (entry->sip.ip6_addr.ul[0] == addr[3] &&
					entry->sip.ip6_addr.ul[1] == addr[2] &&
					entry->sip.ip6_addr.ul[2] == addr[1] &&
					entry->sip.ip6_addr.ul[3] == addr[0]) {
					/* find sipv6 entry*/
					*saddr_map = BIT(i);
				}
			}
		}

		if ((entry->key_bmp & BIT(FAL_TUNNEL_KEY_DIP_EN)) &&
				(ip_addr_ctrl.bf.type == ADPT_TUPLE_DIP_ENTRY)) {
			if (entry->ip_ver == FAL_TUNNEL_IP_VER_4 &&
					ip_addr_ctrl.bf.ver == FAL_TUNNEL_IP_VER_4) {
				if (entry->dip.ip4_addr == addr[0]) {
					/* find dipv4 entry */
					*daddr_map = BIT(i);
				}
			} else if (entry->ip_ver == FAL_TUNNEL_IP_VER_6 &&
					ip_addr_ctrl.bf.ver == FAL_TUNNEL_IP_VER_6) {
				if (entry->dip.ip6_addr.ul[0] == addr[3] &&
					entry->dip.ip6_addr.ul[1] == addr[2] &&
					entry->dip.ip6_addr.ul[2] == addr[1] &&
					entry->dip.ip6_addr.ul[3] == addr[0]) {
					/* find dipv6 entry*/
					*daddr_map = BIT(i);
				}
			}
		}
	}

	return SW_OK;
}

static sw_error_t
_adpt_jhppe_tunnel_tuple_port_entry_find(a_uint32_t dev_id, fal_tunnel_tuple_entry_t *entry,
	a_uint32_t *sport_map, a_uint32_t *dport_map,
	a_uint32_t *proto_map, a_uint32_t *free_port_map)
{
	union tpr_tuple_l4_port_u l4_port = {0};
	sw_error_t rv;
	int i;
	struct qca_phy_priv *priv = ssdk_phy_priv_data_get(dev_id);
	SW_RTN_ON_NULL(priv);

	for (i = 0; i < TPR_TUPLE_L4_PORT_NUM; i++) {
		if (priv->tnl_priv.tpr_tuple_l4_port_entry_used_map[i] == 0) {
			*free_port_map |= BIT(i);
			continue;
		}

		rv = jhppe_tpr_tuple_l4_port_get(dev_id, i, &l4_port);
		SW_RTN_ON_ERROR(rv);

		if ((entry->key_bmp & BIT(FAL_TUNNEL_KEY_SPORT_EN)) &&
				(l4_port.bf.port_type == ADPT_TUPLE_SPORT_ENTRY)) {
			if (entry->sport == l4_port.bf.port_value) {
				/* find sport entry */
				*sport_map = BIT(i);
			}
		}

		if ((entry->key_bmp & BIT(FAL_TUNNEL_KEY_DPORT_EN)) &&
				(l4_port.bf.port_type == ADPT_TUPLE_DPORT_ENTRY)) {
			if (entry->dport == l4_port.bf.port_value) {
				/* find dport entry */
				*dport_map = BIT(i);
			}
		}

		if ((entry->key_bmp & BIT(FAL_TUNNEL_KEY_L4PROTO_EN)) &&
				(l4_port.bf.port_type == ADPT_TUPLE_PROTO_ENTRY)) {
			if (entry->l4_proto == l4_port.bf.port_value) {
				/* find proto entry */
				*proto_map = BIT(i);
			}
		}
	}

	return SW_OK;
}

static sw_error_t
_adpt_jhppe_tunnel_tuple_addr_port_entry_allocate(a_uint32_t dev_id, fal_tunnel_op_type_t op_type,
		fal_tunnel_tuple_entry_t *entry, a_uint32_t *addr_map, a_uint32_t *port_map)
{
	a_uint32_t saddr_map = 0, daddr_map = 0, sport_map = 0, dport_map = 0, proto_map = 0;
	a_uint32_t free_addr_map = 0, free_port_map = 0;
	sw_error_t rv;

	/* find tuple addr entry */
	rv = _adpt_jhppe_tunnel_tuple_addr_entry_find(dev_id, entry,
			&saddr_map, &daddr_map, &free_addr_map);
	SW_RTN_ON_ERROR(rv);

	/* find tuple port entry */
	rv = _adpt_jhppe_tunnel_tuple_port_entry_find(dev_id, entry,
			&sport_map, &dport_map, &proto_map, &free_port_map);
	SW_RTN_ON_ERROR(rv);

	if ((entry->key_bmp & BIT(FAL_TUNNEL_KEY_SIP_EN)) && (saddr_map == 0)) {
		if (op_type == FAL_TUNNEL_OP_TYPE_ADD) {
			/* sip entry not find, set a new entry from the free addr map */
			rv = _adpt_jhppe_tunnel_tuple_new_addr_entry_set(dev_id, entry,
					ADPT_TUPLE_SIP_ENTRY, &free_addr_map, &saddr_map);
			SW_RTN_ON_ERROR(rv);
		} else if (op_type == FAL_TUNNEL_OP_TYPE_DEL) {
			return SW_NOT_FOUND;
		}
	}

	if ((entry->key_bmp & BIT(FAL_TUNNEL_KEY_DIP_EN)) && (daddr_map == 0)) {
		if (op_type == FAL_TUNNEL_OP_TYPE_ADD) {
			/* dip entry not find, set a new entry from the free addr map */
			rv = _adpt_jhppe_tunnel_tuple_new_addr_entry_set(dev_id, entry,
					ADPT_TUPLE_DIP_ENTRY, &free_addr_map, &daddr_map);
			SW_RTN_ON_ERROR(rv);
		} else if (op_type == FAL_TUNNEL_OP_TYPE_DEL) {
			return SW_NOT_FOUND;
		}
	}

	if ((entry->key_bmp & BIT(FAL_TUNNEL_KEY_SPORT_EN)) && (sport_map == 0)) {
		if (op_type == FAL_TUNNEL_OP_TYPE_ADD) {
			/* sport entry not find, set a new entry from the free port map */
			rv = _adpt_jhppe_tunnel_tuple_new_port_entry_set(dev_id, entry,
					ADPT_TUPLE_SPORT_ENTRY, &free_port_map, &sport_map);
			SW_RTN_ON_ERROR(rv);
		} else if (op_type == FAL_TUNNEL_OP_TYPE_DEL) {
			return SW_NOT_FOUND;
		}
	}

	if ((entry->key_bmp & BIT(FAL_TUNNEL_KEY_DPORT_EN)) && (dport_map == 0)) {
		if (op_type == FAL_TUNNEL_OP_TYPE_ADD) {
			/* dport entry not find, set a new entry from the free port map */
			rv = _adpt_jhppe_tunnel_tuple_new_port_entry_set(dev_id, entry,
					ADPT_TUPLE_DPORT_ENTRY, &free_port_map, &dport_map);
			SW_RTN_ON_ERROR(rv);
		} else if (op_type == FAL_TUNNEL_OP_TYPE_DEL) {
			return SW_NOT_FOUND;
		}
	}

	if ((entry->key_bmp & BIT(FAL_TUNNEL_KEY_L4PROTO_EN)) && (proto_map == 0)) {
		if (op_type == FAL_TUNNEL_OP_TYPE_ADD) {
			/* l4 proto entry not find, set a new entry from the free port map */
			rv = _adpt_jhppe_tunnel_tuple_new_port_entry_set(dev_id, entry,
					ADPT_TUPLE_PROTO_ENTRY, &free_port_map, &proto_map);
			SW_RTN_ON_ERROR(rv);
		} else if (op_type == FAL_TUNNEL_OP_TYPE_DEL) {
			return SW_NOT_FOUND;
		}
	}

	*addr_map = saddr_map + daddr_map;
	*port_map = sport_map + dport_map + proto_map;
	return SW_OK;
}

static a_bool_t
_adpt_jhppe_tuple_match_ctrl_get_by_index(a_uint32_t dev_id, a_uint32_t index,
		fal_tunnel_tuple_entry_t *entry, a_uint32_t *addr_map, a_uint32_t *port_map)
{
	union tpr_tuple_match_ctrl_u match_ctrl = {0};
	a_bool_t valid = A_FALSE;
	sw_error_t rv;

	rv = jhppe_tpr_tuple_match_ctrl_valid_get(dev_id, index, &valid);
	if (rv != SW_OK || valid == A_FALSE)
		return A_FALSE;

	rv = jhppe_tpr_tuple_addr_match_addr_map_get(dev_id, index, addr_map);
	if (rv != SW_OK)
		return A_FALSE;

	rv = jhppe_tpr_tuple_port_match_port_map_get(dev_id, index, port_map);
	if (rv != SW_OK)
		return A_FALSE;

	rv = jhppe_tpr_tuple_match_ctrl_get(dev_id, index, &match_ctrl);
	if (rv != SW_OK)
		return A_FALSE;

	if (match_ctrl.bf.tunnel_type_en) {
		entry->context_type = FAL_TUNNEL_TUPLE_CONTEXT_TUNNEL_TYPE;
		entry->context.tunnel_type = match_ctrl.bf.tunnel_type;
	} else {
		entry->context_type = FAL_TUNNEL_TUPLE_CONTEXT_TUPLE_ID;
		entry->context.tuple_id = match_ctrl.bf.tunnel_type;
	}

	return valid;
}

static sw_error_t
_adpt_jhppe_tuple_match_ctrl_set_by_index(a_uint32_t dev_id, a_uint32_t index, a_bool_t valid,
		fal_tunnel_tuple_entry_t *entry, a_uint32_t addr_map, a_uint32_t port_map)
{
	union tpr_tuple_match_ctrl_u match_ctrl = {0};
	sw_error_t rv;

	rv = jhppe_tpr_tuple_addr_match_addr_map_set(dev_id, index, addr_map);
	SW_RTN_ON_ERROR(rv);

	rv = jhppe_tpr_tuple_port_match_port_map_set(dev_id, index, port_map);
	SW_RTN_ON_ERROR(rv);

	if (valid) {
		match_ctrl.bf.valid = true;
		if (entry->context_type == FAL_TUNNEL_TUPLE_CONTEXT_TUNNEL_TYPE) {
			match_ctrl.bf.tunnel_type_en = true;
			match_ctrl.bf.tuple_id_en = false;
			match_ctrl.bf.tunnel_type = entry->context.tunnel_type;
		} else {
			match_ctrl.bf.tunnel_type_en = false;
			match_ctrl.bf.tuple_id_en = true;
			match_ctrl.bf.tunnel_type = entry->context.tuple_id;
		}
	}

	return jhppe_tpr_tuple_match_ctrl_set(dev_id, index, &match_ctrl);
}

static a_bool_t _adpt_jhppe_tunnel_tuple_match_ctrl_equal(a_uint32_t dev_id,
	fal_tunnel_tuple_entry_t *entry1, fal_tunnel_tuple_entry_t *entry2)
{
	if (entry1->context_type != entry2->context_type)
		return A_FALSE;

	if (((entry1->context_type == FAL_TUNNEL_TUPLE_CONTEXT_TUNNEL_TYPE) &&
		(entry1->context.tunnel_type != entry2->context.tunnel_type)) ||
		((entry1->context_type == FAL_TUNNEL_TUPLE_CONTEXT_TUPLE_ID) &&
		(entry1->context.tuple_id != entry2->context.tuple_id)))
		return A_FALSE;

	return A_TRUE;
}

#ifndef IN_TUNNEL_MINI
static a_bool_t _adpt_jhppe_tunnel_tuple_addr_port_equal(a_uint32_t dev_id,
	fal_tunnel_tuple_entry_t *entry1, fal_tunnel_tuple_entry_t *entry2)
{
	if ((entry1->ip_ver != entry2->ip_ver) || (entry1->key_bmp != entry2->key_bmp))
		return A_FALSE;

	if (entry1->key_bmp & BIT(FAL_TUNNEL_KEY_SIP_EN)) {
		if (((entry1->ip_ver == FAL_TUNNEL_IP_VER_4) &&
			(entry1->sip.ip4_addr != entry2->sip.ip4_addr)) ||
			((entry1->ip_ver == FAL_TUNNEL_IP_VER_6) &&
			 aos_mem_cmp(&entry1->sip.ip6_addr, &entry2->sip.ip6_addr,
				 sizeof(entry1->sip.ip6_addr))))
			return A_FALSE;
	}

	if (entry1->key_bmp & BIT(FAL_TUNNEL_KEY_DIP_EN)) {
		if (((entry1->ip_ver == FAL_TUNNEL_IP_VER_4) &&
			(entry1->dip.ip4_addr != entry2->dip.ip4_addr)) ||
			((entry1->ip_ver == FAL_TUNNEL_IP_VER_6) &&
			 aos_mem_cmp(&entry1->dip.ip6_addr, &entry2->dip.ip6_addr,
				 sizeof(entry1->sip.ip6_addr))))
			return A_FALSE;
	}

	if (((entry1->key_bmp & BIT(FAL_TUNNEL_KEY_SPORT_EN)) &&
	    (entry1->sport != entry2->sport)) ||
		((entry1->key_bmp & BIT(FAL_TUNNEL_KEY_DPORT_EN)) &&
		 (entry1->dport != entry2->dport)) ||
		((entry1->key_bmp & BIT(FAL_TUNNEL_KEY_L4PROTO_EN)) &&
		 (entry1->l4_proto != entry2->l4_proto)))
		return A_FALSE;

	return A_TRUE;
}
#endif

static sw_error_t
_adpt_jhppe_tunnel_tuple_match_ctrl_index_get(a_uint32_t dev_id, fal_tunnel_op_type_t op_type,
	fal_tunnel_tuple_entry_t *entry, a_uint32_t *index,
	a_uint32_t addr_map, a_uint32_t port_map)
{
	a_uint32_t temp_addr_map, temp_port_map;
	fal_tunnel_tuple_entry_t temp_entry = {0};
	int i;

	for (i = TPR_TUPLE_MATCH_CTRL_NUM - 1; i >= 0; i--) {
		aos_mem_zero(&temp_entry, sizeof (fal_tunnel_tuple_entry_t));
		temp_addr_map = 0;
		temp_port_map = 0;
		if (_adpt_jhppe_tuple_match_ctrl_get_by_index(dev_id, i,
					&temp_entry, &temp_addr_map, &temp_port_map)) {
			if (temp_addr_map == addr_map && temp_port_map == port_map) {
				if (op_type == FAL_TUNNEL_OP_TYPE_ADD) {
					if (_adpt_jhppe_tunnel_tuple_match_ctrl_equal(dev_id,
								entry, &temp_entry)) {
						return SW_ALREADY_EXIST;
					} else {
						*index = i;
						return SW_OK;
					}
				} else if (op_type == FAL_TUNNEL_OP_TYPE_DEL) {
					if (_adpt_jhppe_tunnel_tuple_match_ctrl_equal(dev_id,
								entry, &temp_entry)) {
						/* found */
						*index = i;
						return SW_OK;
					}
				} else {
					return SW_BAD_PARAM;
				}
			};
		} else {
			break;
		}
	}

	if (op_type == FAL_TUNNEL_OP_TYPE_ADD) {
		if (i < 0) {
			return SW_NO_RESOURCE;
		} else {
			*index = i;
			return SW_OK;
		}
	} else if (op_type == FAL_TUNNEL_OP_TYPE_DEL) {
		return SW_NOT_FOUND;
	} else {
		return SW_BAD_PARAM;
	}
}

static sw_error_t
_adpt_jhppe_insert_tuple_match_ctrl_by_sort(a_uint32_t dev_id, a_uint32_t index,
		fal_tunnel_tuple_entry_t *entry, a_uint32_t *addr_map, a_uint32_t *port_map)
{
	a_uint32_t weight, temp_weight, temp_addr_map, temp_port_map;
	fal_tunnel_tuple_entry_t temp_entry;
	sw_error_t rv;
	int i, j;
	struct qca_phy_priv *priv = ssdk_phy_priv_data_get(dev_id);
	SW_RTN_ON_NULL(priv);

	weight = bitmap_weight((const unsigned long *)addr_map, TPR_TUPLE_IP_ADDR_CTRL_NUM);
	weight += bitmap_weight((const unsigned long *)port_map, TPR_TUPLE_L4_PORT_NUM);

	for (i = index; i < TPR_TUPLE_MATCH_CTRL_NUM - 1; i++) {
		aos_mem_zero(&temp_entry, sizeof (fal_tunnel_tuple_entry_t));
		temp_addr_map = 0;
		temp_port_map = 0;
		if (!_adpt_jhppe_tuple_match_ctrl_get_by_index(dev_id, i + 1,
					&temp_entry, &temp_addr_map, &temp_port_map))
			return SW_FAIL;

		temp_weight = bitmap_weight((const unsigned long *)&temp_addr_map,
				TPR_TUPLE_IP_ADDR_CTRL_NUM);
		temp_weight += bitmap_weight((const unsigned long *)&temp_port_map,
				TPR_TUPLE_L4_PORT_NUM);

		if (weight < temp_weight) {
			rv = _adpt_jhppe_tuple_match_ctrl_set_by_index(dev_id, i, A_TRUE,
						&temp_entry, temp_addr_map, temp_port_map);
			SW_RTN_ON_ERROR(rv);

			for_each_set_bit(j, (const unsigned long *)&temp_addr_map,
					TPR_TUPLE_IP_ADDR_CTRL_NUM) {
				priv->tnl_priv.tpr_tuple_ip_addr_entry_used_map[j] &= ~BIT(i + 1);
				priv->tnl_priv.tpr_tuple_ip_addr_entry_used_map[j] |= BIT(i);
			}

			for_each_set_bit(j, (const unsigned long *)&temp_port_map,
					TPR_TUPLE_L4_PORT_NUM) {
				priv->tnl_priv.tpr_tuple_l4_port_entry_used_map[j] &= ~BIT(i + 1);
				priv->tnl_priv.tpr_tuple_l4_port_entry_used_map[j] |= BIT(i);
			}
		} else {
			break;
		}
	}

	rv = _adpt_jhppe_tuple_match_ctrl_set_by_index(dev_id, i, A_TRUE,
			entry, *addr_map, *port_map);
	SW_RTN_ON_ERROR(rv);

	for_each_set_bit(j, (const unsigned long *)addr_map, TPR_TUPLE_IP_ADDR_CTRL_NUM)
		priv->tnl_priv.tpr_tuple_ip_addr_entry_used_map[j] |= BIT(i);

	for_each_set_bit(j, (const unsigned long *)port_map, TPR_TUPLE_L4_PORT_NUM)
		priv->tnl_priv.tpr_tuple_l4_port_entry_used_map[j] |= BIT(i);

	return SW_OK;
}

static sw_error_t
_adpt_jhppe_delete_tuple_match_ctrl_by_sort(a_uint32_t dev_id, a_uint32_t index,
		fal_tunnel_tuple_entry_t *entry, a_uint32_t *addr_map, a_uint32_t *port_map)
{
	fal_tunnel_tuple_entry_t temp_entry, zero_entry = {0};
	a_uint32_t temp_addr_map, temp_port_map;
	sw_error_t rv;
	int i, j;
	struct qca_phy_priv *priv = ssdk_phy_priv_data_get(dev_id);
	SW_RTN_ON_NULL(priv);

	for_each_set_bit(j, (const unsigned long *)addr_map, TPR_TUPLE_IP_ADDR_CTRL_NUM)
		priv->tnl_priv.tpr_tuple_ip_addr_entry_used_map[j] &= ~BIT(index);

	for_each_set_bit(j, (const unsigned long *)port_map, TPR_TUPLE_L4_PORT_NUM)
		priv->tnl_priv.tpr_tuple_l4_port_entry_used_map[j] &= ~BIT(index);

	for (i = index; i > 0; i --) {
		aos_mem_zero(&temp_entry, sizeof (fal_tunnel_tuple_entry_t));
		temp_addr_map = 0;
		temp_port_map = 0;
		if (_adpt_jhppe_tuple_match_ctrl_get_by_index(dev_id, i - 1,
					&temp_entry, &temp_addr_map, &temp_port_map)) {
			rv = _adpt_jhppe_tuple_match_ctrl_set_by_index(dev_id, i, A_TRUE,
					&temp_entry, temp_addr_map, temp_port_map);
			SW_RTN_ON_ERROR(rv);

			for_each_set_bit(j, (const unsigned long *)&temp_addr_map,
					TPR_TUPLE_IP_ADDR_CTRL_NUM) {
				priv->tnl_priv.tpr_tuple_ip_addr_entry_used_map[j] &= ~BIT(i - 1);
				priv->tnl_priv.tpr_tuple_ip_addr_entry_used_map[j] |= BIT(i);
			}

			for_each_set_bit(j, (const unsigned long *)&temp_port_map,
					TPR_TUPLE_L4_PORT_NUM) {
				priv->tnl_priv.tpr_tuple_l4_port_entry_used_map[j] &= ~BIT(i - 1);
				priv->tnl_priv.tpr_tuple_l4_port_entry_used_map[j] |= BIT(i);
			}
		} else {
			break;
		}
	}

	/* clear entry */
	rv =  _adpt_jhppe_tuple_match_ctrl_set_by_index(dev_id, i, A_FALSE, &zero_entry, 0, 0);
	SW_RTN_ON_ERROR(rv);

	return SW_OK;
}

static sw_error_t
_adpt_jhppe_tunnel_tuple_entry_operation(a_uint32_t dev_id, fal_tunnel_op_type_t op_type,
		fal_tunnel_tuple_entry_t *entry)
{
	a_uint32_t addr_map = 0, port_map = 0, index = 0;
	sw_error_t rv;

	rv = _adpt_jhppe_tunnel_tuple_addr_port_entry_allocate(dev_id, op_type,
			entry, &addr_map, &port_map);
	SW_RTN_ON_ERROR(rv);

	rv = _adpt_jhppe_tunnel_tuple_match_ctrl_index_get(dev_id, op_type,
			entry, &index, addr_map, port_map);
	SW_RTN_ON_ERROR(rv);

	switch(op_type) {
	case FAL_TUNNEL_OP_TYPE_ADD:
		rv = _adpt_jhppe_insert_tuple_match_ctrl_by_sort(dev_id, index,
				entry, &addr_map, &port_map);
		break;
	case FAL_TUNNEL_OP_TYPE_DEL:
		rv = _adpt_jhppe_delete_tuple_match_ctrl_by_sort(dev_id, index,
				entry, &addr_map, &port_map);
		break;
	default:
		rv = SW_NOT_SUPPORTED;
	}

	return rv;
}

#ifndef IN_TUNNEL_MINI
sw_error_t
_adpt_jhppe_tunnel_tuple_entry_getnext(a_uint32_t dev_id,
		fal_tunnel_tuple_entry_t *entry, a_bool_t sign_tag)
{
	a_uint32_t addr_map, port_map;
	fal_tunnel_tuple_entry_t tmp_entry = {0};
	sw_error_t rv;
	int i;

	for (i = 0; i < TPR_TUPLE_MATCH_CTRL_NUM; i++) {
		aos_mem_zero(&tmp_entry, sizeof (fal_tunnel_tuple_entry_t));
		addr_map = 0;
		port_map = 0;
		if (_adpt_jhppe_tuple_match_ctrl_get_by_index(dev_id, i,
					&tmp_entry, &addr_map, &port_map)) {
			rv = _adpt_jhppe_tunnel_tuple_addr_port_entry_get(dev_id,
					&addr_map, &port_map, &tmp_entry);
			SW_RTN_ON_ERROR(rv);

			if (A_TRUE == sign_tag) {
				aos_mem_copy(entry, &tmp_entry, sizeof(fal_tunnel_tuple_entry_t));
				/* record hw entry index, addr and port map info */
				entry->index = i;
				entry->addr_map = addr_map;
				entry->port_map = port_map;
				return SW_OK;
			}

			if (_adpt_jhppe_tunnel_tuple_match_ctrl_equal(dev_id, entry, &tmp_entry) &&
				_adpt_jhppe_tunnel_tuple_addr_port_equal(dev_id, entry, &tmp_entry))
				sign_tag = A_TRUE;
		}
	}

	return SW_NO_MORE;
}
#endif

sw_error_t
adpt_jhppe_tunnel_tuple_entry_add(a_uint32_t dev_id, fal_tunnel_tuple_entry_t *entry)
{
	return _adpt_jhppe_tunnel_tuple_entry_operation(dev_id, FAL_TUNNEL_OP_TYPE_ADD, entry);
}

sw_error_t
adpt_jhppe_tunnel_tuple_entry_del(a_uint32_t dev_id, fal_tunnel_tuple_entry_t *entry)
{
	return _adpt_jhppe_tunnel_tuple_entry_operation(dev_id, FAL_TUNNEL_OP_TYPE_DEL, entry);
}

#ifndef IN_TUNNEL_MINI
sw_error_t
adpt_jhppe_tunnel_tuple_entry_getfirst(a_uint32_t dev_id, fal_tunnel_tuple_entry_t *entry)
{
	return _adpt_jhppe_tunnel_tuple_entry_getnext(dev_id, entry, A_TRUE);
}

sw_error_t
adpt_jhppe_tunnel_tuple_entry_getnext(a_uint32_t dev_id, fal_tunnel_tuple_entry_t *entry)
{
	return _adpt_jhppe_tunnel_tuple_entry_getnext(dev_id, entry, A_FALSE);
}
#endif

sw_error_t
adpt_jhppe_tunnel_decap_miss_action_set(a_uint32_t dev_id,
		fal_tunnel_type_t tunnel_type, fal_tunnel_decap_miss_action_t *miss_action)
{
	union tl_key_gen_u key_gen;

	ADPT_DEV_ID_CHECK(dev_id);
	ADPT_NULL_POINT_CHECK(miss_action);

	if (tunnel_type >= FAL_TUNNEL_TYPE_INVALID_TUNNEL)
		return SW_BAD_PARAM;

	SW_RTN_ON_ERROR(appe_tl_key_gen_get(dev_id, tunnel_type, &key_gen));

	key_gen.bf.decap_en = miss_action->decap_en;
	key_gen.bf.service_code_en = miss_action->service_code_en;
	key_gen.bf.service_code = miss_action->service_code;

	return appe_tl_key_gen_set(dev_id, tunnel_type, &key_gen);
}

#ifndef IN_TUNNEL_MINI
sw_error_t
adpt_jhppe_tunnel_decap_miss_action_get(a_uint32_t dev_id,
		fal_tunnel_type_t tunnel_type, fal_tunnel_decap_miss_action_t *miss_action)
{
	union tl_key_gen_u key_gen;

	ADPT_DEV_ID_CHECK(dev_id);
	ADPT_NULL_POINT_CHECK(miss_action);

	if (tunnel_type >= FAL_TUNNEL_TYPE_INVALID_TUNNEL)
		return SW_BAD_PARAM;

	SW_RTN_ON_ERROR(appe_tl_key_gen_get(dev_id, tunnel_type, &key_gen));

	miss_action->decap_en = key_gen.bf.decap_en;
	miss_action->service_code_en = key_gen.bf.service_code_en;
	miss_action->service_code = key_gen.bf.service_code;

	return SW_OK;
}
#endif
