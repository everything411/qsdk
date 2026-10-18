/*
 * Copyright (c) 2016-2017, The Linux Foundation. All rights reserved.
* Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
* SPDX-License-Identifier: ISC
*/

#include "ref_vsi.h"
#include "ssdk_init.h"
#include "ssdk_plat.h"
#include "adpt.h"
#include "ssdk_dts.h"

#define PPE_VSI_MAX FAL_VSI_MAX

static ref_vsi_t ref_vsi_mapping[SW_MAX_NR_DEV][PPE_VSI_MAX+1] ={{{0, 0, {0},NULL},
								{0, 0, {0}, NULL},
								{0, 0, {0}, NULL}}};
union {
	aos_lock_t spin_lock;
	aos_mutex_lock_t mutex_lock;
} ppe_vlan_vsi_lock[SW_MAX_NR_DEV];

#define REF_VLAN_VSI_LOCK(lock, access_mode) \
do { \
	if (access_mode == HSL_REG_MDIO) \
		aos_mutex_lock(&lock.mutex_lock); \
	else \
		aos_lock_bh(&lock.spin_lock); \
} while(0)

#define REF_VLAN_VSI_UNLOCK(lock, access_mode) \
do { \
	if (access_mode == HSL_REG_MDIO) \
		aos_mutex_unlock(&lock.mutex_lock); \
	else \
		aos_unlock_bh(&lock.spin_lock); \
} while(0)

#define REF_VLAN_VSI_LOCK_INIT(lock, access_mode) \
do { \
	if (access_mode == HSL_REG_MDIO) \
		aos_mutex_lock_init(&lock.mutex_lock); \
	else \
		aos_lock_init(&lock.spin_lock); \
} while(0)

static sw_error_t
_ppe_vsi_member_init(a_uint32_t dev_id, a_uint32_t vsi_id)
{
	fal_vsi_member_t vsi_member;

	aos_mem_zero(&vsi_member, sizeof(vsi_member));

	vsi_member.member_ports = 0x1;
	vsi_member.umc_ports    = 0x1;
	vsi_member.uuc_ports    = 0x1;
	vsi_member.bc_ports     = 0x1;

	fal_vsi_member_set(dev_id, vsi_id, &vsi_member);

	return SW_OK;
}

static sw_error_t
_ppe_vsi_member_update(a_uint32_t dev_id, a_uint32_t vsi_id,
				fal_port_t port_id, a_uint32_t op)
{
	sw_error_t rv;
	fal_vsi_member_t vsi_member;
	a_uint32_t port_value = 0, vport_value = 0;

	port_value = FAL_PORT_ID_VALUE (port_id);
	vport_value = port_value - SSDK_MIN_VIRTUAL_PORT_ID;
	SSDK_DEBUG("port_id:0x%x, port_value:%d\n", port_id, port_value);
	rv = fal_vsi_member_get( dev_id, vsi_id, &vsi_member);
	if( rv != SW_OK )
		return rv;
	if( PPE_VSI_DEL == op )
	{
		if(FAL_IS_VPORT(port_id))
		{
			vsi_member.member_vports[vport_value/32]
				&= (~(1<<(vport_value%32)));
			SSDK_DEBUG("op:%d, vsi_member.member_vports[%d]: 0x%x\n",
				op, vport_value/32, vsi_member.member_vports[vport_value/32]);
		}
		else
		{
			vsi_member.member_ports &= (~(1<<port_value));
			/*vsi_member.bc_ports &= (~(1<<port_id));
			vsi_member.umc_ports &= (~(1<<port_id));
			vsi_member.uuc_ports &= (~(1<<port_id));*/
			SSDK_DEBUG("op:%d, vsi_member.member_ports :0x%x\n",
				op,vsi_member.member_ports);
		}
	}
	else if( PPE_VSI_ADD == op )
	{
		if(FAL_IS_VPORT(port_id))
		{
			vsi_member.member_vports[vport_value/32]
				|= (1<<vport_value%32);
			SSDK_DEBUG("op:%d, vsi_member.member_vports[%d]: 0x%x\n",
				op, vport_value/32, vsi_member.member_vports[vport_value/32]);
		}
		else
		{
			vsi_member.member_ports |= (1<<port_value);
			/*vsi_member.bc_ports |= (1<<port_id);
			vsi_member.umc_ports |= (1<<port_id);
			vsi_member.uuc_ports |= (1<<port_id);*/
			SSDK_DEBUG("op:%d, vsi_member.member_ports :0x%x\n",
				op,vsi_member.member_ports);
		}
	}
	rv = fal_vsi_member_set(dev_id, vsi_id, &vsi_member);
	if( rv != SW_OK )
		return rv;

	return SW_OK;
}

static sw_error_t _ppe_vlan_vsi_mapping_add(a_uint32_t dev_id, fal_port_t port_id,
					a_uint32_t stag_vid, a_uint32_t ctag_vid, a_uint32_t vsi_id)
{
	ref_vlan_info_t *p_vsi_info = NULL;
	sw_error_t rv;

	rv = fal_port_vlan_vsi_set(dev_id, port_id, stag_vid, ctag_vid, vsi_id);
	if( rv != SW_OK )
	{
		SSDK_ERROR("port 0x%x svlan %d cvlan %d vsi %d set fail, rv = %d\n",
				port_id, stag_vid, ctag_vid, vsi_id, rv);
		return rv;
	}
	rv = _ppe_vsi_member_update(dev_id, vsi_id, port_id, PPE_VSI_ADD);
	if( rv != SW_OK )
	{
		return rv;
	}

	/*vlan based vsi update*/
	p_vsi_info = ref_vsi_mapping[dev_id][vsi_id].pHead;
	while(p_vsi_info != NULL)
	{
		if((stag_vid == p_vsi_info->stag_vid) &&
			(ctag_vid == p_vsi_info->ctag_vid))
		{
			if(FAL_IS_PPORT(port_id) && FAL_IS_PPORT(p_vsi_info->vlan_port_bitmap))
			{
				p_vsi_info->vlan_port_bitmap |= 1 << port_id;
				break;
			}
		}
		p_vsi_info = p_vsi_info->pNext;
	}

	if(p_vsi_info == NULL)/*create a new entry if no match*/
	{
		SSDK_DEBUG("port 0x%x svlan %d cvlan %d vsi %d create new entry\n",
				port_id, stag_vid, ctag_vid, vsi_id);
		p_vsi_info = kzalloc(sizeof(ref_vlan_info_t), GFP_ATOMIC);
		if(p_vsi_info == NULL)
		{
			SSDK_ERROR("port 0x%x svlan %d cvlan %d vsi %d aos_mem_alloc fail\n",
					port_id, stag_vid, ctag_vid, vsi_id);
			return SW_NO_RESOURCE;
		}
		if(FAL_IS_VPORT(port_id))
		{
			p_vsi_info->vlan_port_bitmap = port_id;
		}
		else
		{
			p_vsi_info->vlan_port_bitmap = 1 << port_id;
		}
		p_vsi_info->stag_vid = stag_vid;
		p_vsi_info->ctag_vid = ctag_vid;
		p_vsi_info->pNext = (ref_vsi_mapping[dev_id][vsi_id].pHead);
		ref_vsi_mapping[dev_id][vsi_id].pHead = p_vsi_info;
	}

	return rv;
}

static sw_error_t _ppe_vlan_vsi_mapping_del(a_uint32_t dev_id, fal_port_t port_id,
					a_uint32_t stag_vid, a_uint32_t ctag_vid, a_uint32_t vsi_id)
{
	ref_vlan_info_t *p_vsi_info = NULL;
	ref_vlan_info_t *p_prev = NULL;
	sw_error_t rv;
	a_bool_t in_vsi = 0;
	a_uint32_t vport_value = 0;

	rv = fal_port_vlan_vsi_set(dev_id, port_id, stag_vid, ctag_vid, PPE_VSI_INVALID);
	if( rv != SW_OK )
	{
		SSDK_ERROR("port %d svlan %d cvlan %d vsi %d set fail, rv = %d\n",
				port_id, stag_vid, ctag_vid, vsi_id, rv);
		return rv;
	}

	/*vlan based vsi update*/
	p_vsi_info = ref_vsi_mapping[dev_id][vsi_id].pHead;
	p_prev = p_vsi_info;
	while(p_vsi_info != NULL)
	{
		if((ctag_vid == p_vsi_info->ctag_vid) &&
			(stag_vid == p_vsi_info->stag_vid))
		{
			/*update software data*/
			if(FAL_IS_VPORT(port_id))
			{
				if(FAL_IS_VPORT(p_vsi_info->vlan_port_bitmap) &&
					p_vsi_info->vlan_port_bitmap == port_id)
				{
					p_vsi_info->vlan_port_bitmap = 0;
				}
			}
			else
			{
				if(FAL_IS_PPORT(p_vsi_info->vlan_port_bitmap) &&
					p_vsi_info->vlan_port_bitmap & (1 << port_id))
				{
					p_vsi_info->vlan_port_bitmap &= (~(1 << port_id));
				}
			}
			if(p_vsi_info->vlan_port_bitmap == 0)/*free node if bitmap is 0*/
			{
				if(p_vsi_info == ref_vsi_mapping[dev_id][vsi_id].pHead)
				{
					ref_vsi_mapping[dev_id][vsi_id].pHead =
							p_vsi_info->pNext;
				}
				else
				{
					p_prev->pNext = p_vsi_info->pNext;
				}
				aos_mem_free(p_vsi_info);
				p_vsi_info = NULL;
				break;
			}
		}
		else
		{
			in_vsi = 1;/*port + another vlan --> vsi*/
		}
		p_prev = p_vsi_info;
		p_vsi_info = p_vsi_info->pNext;
	}

	vport_value = FAL_PORT_ID_VALUE(port_id) - SSDK_MIN_VIRTUAL_PORT_ID;
	if(in_vsi == 0 &&
		((FAL_IS_PPORT(port_id) && (!(1 << port_id &
		ref_vsi_mapping[dev_id][vsi_id].pport_bitmap))) ||
		(FAL_IS_VPORT(port_id) && (!((1 << (vport_value%32) &
		ref_vsi_mapping[dev_id][vsi_id].vport_bitmap[vport_value/32]))))))
	{
		rv = _ppe_vsi_member_update(dev_id, vsi_id, port_id, PPE_VSI_DEL);
		if( rv != SW_OK )
		{
			SSDK_ERROR("port 0x%x svlan %d cvlan %d vsi %d fail, rv = %d\n",
					port_id, stag_vid, ctag_vid, vsi_id, rv);
			return rv;
		}
	}

	return SW_OK;
}

static sw_error_t _ppe_port_vsi_mapping_update(a_uint32_t dev_id,
					fal_port_t port_id, a_uint32_t vsi_id)
{
	ref_vsi_t *p_vsi = NULL;
	sw_error_t rv;
	a_uint32_t i = 0;
	a_uint32_t port_value = 0, vport_value = 0;

	port_value = FAL_PORT_ID_VALUE(port_id);
	vport_value = port_value - SSDK_MIN_VIRTUAL_PORT_ID;

	if(vsi_id != FAL_VSI_INVALID)
	{
		if(vsi_id > PPE_VSI_MAX)
		{
			SSDK_ERROR("VSI %d is not supported\n", vsi_id);
			return SW_OUT_OF_RANGE;
		}
		if(ref_vsi_mapping[dev_id][vsi_id].valid == 0)
		{
			SSDK_ERROR("port 0x%x vsi %d entry not found\n", port_id, vsi_id);
			return SW_NOT_FOUND;
		}
	}

	/*check port previous vsi*/
	for( i = 0; i <= PPE_VSI_MAX; i++ )
	{
		p_vsi = &(ref_vsi_mapping[dev_id][i]);
		if(p_vsi->valid != 0)
		{
			if(FAL_IS_VPORT(port_id))
			{
				if(p_vsi->vport_bitmap[vport_value/32] & (1 << (vport_value%32)))
				{
					/*remmove from preious vsi*/
					rv = _ppe_vsi_member_update(dev_id, i, port_id,
						PPE_VSI_DEL);
					if( rv != SW_OK )
						return rv;
					p_vsi->vport_bitmap[vport_value/32] &=
						(~(1<<(vport_value%32)));
				}
			}
			else
			{
				if(p_vsi->pport_bitmap & (1 << port_value))
				{
					/*remmove from preious vsi*/
					rv = _ppe_vsi_member_update(dev_id, i, port_id,
						PPE_VSI_DEL);
					if( rv != SW_OK )
						return rv;
					p_vsi->pport_bitmap &= (~(1<<port_value));
				}
			}
		}
	}
	SSDK_DEBUG("port 0x%x, vsi %d set\n", port_id, vsi_id);
	/*port based vsi update*/
	if(vsi_id <= FAL_VSI_MAX)
	{
		rv = _ppe_vsi_member_update(dev_id, vsi_id, port_id, PPE_VSI_ADD);
		if( rv != SW_OK )
			return rv;
		if(FAL_IS_VPORT(port_id))
		{
			ref_vsi_mapping[dev_id][vsi_id].vport_bitmap[vport_value/32] |=
				1 << (vport_value%32);
		}
		else
		{
			ref_vsi_mapping[dev_id][vsi_id].pport_bitmap |= 1 << port_value;
		}
	}

	return SW_OK;
}


sw_error_t
ppe_port_vlan_vsi_set(a_uint32_t dev_id, fal_port_t port_id,
		a_uint32_t stag_vid, a_uint32_t ctag_vid, a_uint32_t vsi_id)
{
	sw_error_t rv;
	a_uint32_t cur_vsi = PPE_VSI_INVALID;
	hsl_reg_mode access_mode = ssdk_switch_reg_access_mode_get(dev_id);

	REF_DEV_ID_CHECK(dev_id);

	SSDK_DEBUG("port 0x%x svlan %d cvlan %d vsi %d \n",
			port_id, stag_vid, ctag_vid, vsi_id);

	if((vsi_id != PPE_VSI_INVALID) &&
		(ref_vsi_mapping[dev_id][vsi_id].valid == 0))
	{
		SSDK_ERROR("port %d svlan %d cvlan %d vsi %d entry not found\n",
				port_id, stag_vid, ctag_vid, vsi_id);
		return SW_NOT_FOUND;
	}

	ppe_port_vlan_vsi_get(dev_id, port_id, stag_vid, ctag_vid, &cur_vsi);
	if(cur_vsi == vsi_id)
		return SW_OK;
	REF_VLAN_VSI_LOCK(ppe_vlan_vsi_lock[dev_id], access_mode);

	if(PPE_VSI_INVALID == vsi_id || cur_vsi != PPE_VSI_INVALID)
	{
		SSDK_DEBUG("Deleting port 0x%x svlan %d cvlan %d vsi %d\n",
				port_id, stag_vid, ctag_vid, cur_vsi);
		rv = _ppe_vlan_vsi_mapping_del(dev_id, port_id, stag_vid, ctag_vid, cur_vsi);
		if( rv != SW_OK )
		{
			REF_VLAN_VSI_UNLOCK(ppe_vlan_vsi_lock[dev_id], access_mode);
			return rv;
		}
	}

	if(PPE_VSI_INVALID != vsi_id)
	{
		SSDK_DEBUG("Adding port 0x%x svlan %d cvlan %d vsi %d\n",
				port_id, stag_vid, ctag_vid, vsi_id);

		rv = _ppe_vlan_vsi_mapping_add(dev_id, port_id, stag_vid, ctag_vid, vsi_id);
	}
	REF_VLAN_VSI_UNLOCK(ppe_vlan_vsi_lock[dev_id], access_mode);
	return rv;
}

sw_error_t ppe_port_vlan_vsi_get(a_uint32_t dev_id, fal_port_t port_id,
		a_uint32_t stag_vid, a_uint32_t ctag_vid, a_uint32_t *vsi_id)
{
	ref_vlan_info_t *p_vsi_info = NULL;
	a_uint32_t i = 0;
	hsl_reg_mode access_mode = ssdk_switch_reg_access_mode_get(dev_id);

	SSDK_DEBUG("Getting port 0x%x svlan %d cvlan %d\n", port_id, stag_vid, ctag_vid);

	REF_DEV_ID_CHECK(dev_id);
	REF_NULL_POINT_CHECK(vsi_id);

	REF_VLAN_VSI_LOCK(ppe_vlan_vsi_lock[dev_id], access_mode);
	for( i = 0; i <= PPE_VSI_MAX; i++ )
	{
		p_vsi_info = ref_vsi_mapping[dev_id][i].pHead;
		while(p_vsi_info != NULL)
		{
			if((ctag_vid== p_vsi_info->ctag_vid) && (stag_vid== p_vsi_info->stag_vid))
			{
				if((FAL_IS_PPORT(port_id) && FAL_IS_PPORT(p_vsi_info->vlan_port_bitmap) &&
					(p_vsi_info->vlan_port_bitmap & (1 << port_id))) || 
					(FAL_IS_VPORT(port_id) && FAL_IS_VPORT(p_vsi_info->vlan_port_bitmap) &&
					(p_vsi_info->vlan_port_bitmap == port_id)))
				{
					*vsi_id = i;
					SSDK_DEBUG("Returned port 0x%x svlan %d cvlan %d vsi %d\n",
							port_id, stag_vid, ctag_vid, *vsi_id);
					REF_VLAN_VSI_UNLOCK(ppe_vlan_vsi_lock[dev_id], access_mode);

					return SW_OK;
				}
			}
			p_vsi_info = p_vsi_info->pNext;
		}
	}
	REF_VLAN_VSI_UNLOCK(ppe_vlan_vsi_lock[dev_id], access_mode);
	return SW_NOT_FOUND;
}


/*called when
	1. switchdev create physical interface for port
	2. add physical interface to a bridge*/
sw_error_t
ppe_port_vsi_set(a_uint32_t dev_id, fal_port_t port_id, a_uint32_t vsi_id)
{
	sw_error_t rv = SW_OK;

	REF_DEV_ID_CHECK(dev_id);
	SSDK_DEBUG("port 0x%x, vsi %d\n", port_id, vsi_id);
	if(!(FAL_IS_PPORT(port_id)) && !(FAL_IS_VPORT(port_id)))
		return SW_BAD_VALUE;

	if(vsi_id > PPE_VSI_MAX && vsi_id != FAL_VSI_INVALID){
		SSDK_ERROR("invalid VSI port %d, vsi %d\n", port_id, vsi_id);
		return SW_BAD_VALUE;
	}
	rv = fal_port_vsi_set(dev_id, port_id, vsi_id);
	SW_RTN_ON_ERROR (rv);
	rv = _ppe_port_vsi_mapping_update(dev_id, port_id, vsi_id);

	return rv;
}

sw_error_t
ppe_port_vsi_get(a_uint32_t dev_id, fal_port_t port_id, a_uint32_t *vsi_id)
{
	a_uint32_t i = 0;

	for( i = 0; i <= PPE_VSI_MAX; i++ )
	{
		if(FAL_IS_PPORT(port_id))
		{
			if((ref_vsi_mapping[dev_id][i].valid != 0)&&
				(ref_vsi_mapping[dev_id][i].pport_bitmap & (1 << port_id)))
			{
				*vsi_id = i;
				SSDK_DEBUG("returned port 0x%x, vsi %d\n", port_id, *vsi_id);
				return SW_OK;
			}
		}
		if(FAL_IS_VPORT(port_id))
		{
			a_uint32_t vport_value = 0;
			vport_value = FAL_PORT_ID_VALUE(port_id) - SSDK_MIN_VIRTUAL_PORT_ID;
			if((ref_vsi_mapping[dev_id][i].valid != 0)&&
				(ref_vsi_mapping[dev_id][i].vport_bitmap[vport_value/32] &
				(1 << (vport_value%32))))
			{
				*vsi_id = i;
				SSDK_DEBUG("returned port 0x%x, vsi %d\n", port_id, *vsi_id);
				return SW_OK;
			}
		}
	}
	SSDK_DEBUG("VSI is not configured on port 0x%x\n", port_id);
	return SW_NOT_FOUND;
}

sw_error_t ppe_vsi_alloc(a_uint32_t dev_id, a_uint32_t *vsi)
{
	a_uint32_t vsi_id;

	REF_DEV_ID_CHECK(dev_id);
	REF_NULL_POINT_CHECK(vsi);
	SSDK_DEBUG("requesting vsi\n");

	for( vsi_id = 0; vsi_id <= PPE_VSI_MAX; vsi_id++ )
	{
		if(ref_vsi_mapping[dev_id][vsi_id].valid == 0)
		{
			fal_vsi_newaddr_lrn_t newaddr_lrn;
			fal_vsi_stamove_t stamove;
			ref_vsi_mapping[dev_id][vsi_id].valid = 1;
  			ref_vsi_mapping[dev_id][vsi_id].pport_bitmap = 0;
			aos_mem_zero(ref_vsi_mapping[dev_id][vsi_id].vport_bitmap,
				sizeof(ref_vsi_mapping[dev_id][vsi_id].vport_bitmap));
  			ref_vsi_mapping[dev_id][vsi_id].pHead = NULL;
			*vsi = vsi_id;
			_ppe_vsi_member_init(dev_id, vsi_id);
			newaddr_lrn.lrn_en = 1;
			newaddr_lrn.action = 0;
			fal_vsi_newaddr_lrn_set(dev_id, vsi_id, &newaddr_lrn);
			stamove.stamove_en = 1;
			stamove.action = 0;
			fal_vsi_stamove_set(dev_id, vsi_id, &stamove);
			SSDK_DEBUG("vsi %d allocated\n", *vsi);
			return SW_OK;
		}
	}

	return SW_NO_RESOURCE;
}

sw_error_t ppe_vsi_free(a_uint32_t dev_id, a_uint32_t vsi_id)
{
	ref_vlan_info_t *p_vsi_info = NULL;
	REF_DEV_ID_CHECK(dev_id);

	if(vsi_id > PPE_VSI_MAX)
		return SW_OUT_OF_RANGE;

	p_vsi_info = ref_vsi_mapping[dev_id][vsi_id].pHead;
	while(p_vsi_info != NULL)
	{
		ref_vlan_info_t *ptemp = p_vsi_info;
		p_vsi_info = p_vsi_info->pNext;
		SSDK_DEBUG("port 0x%x svlan %d cvlan %d, vsi %d free vsi info\n",
				ptemp->vlan_port_bitmap, ptemp->stag_vid, ptemp->ctag_vid, vsi_id);
		aos_mem_free(ptemp);
	}
	ref_vsi_mapping[dev_id][vsi_id].valid = 0;
	ref_vsi_mapping[dev_id][vsi_id].pHead = NULL;
	ref_vsi_mapping[dev_id][vsi_id].pport_bitmap = 0;
	aos_mem_zero(ref_vsi_mapping[dev_id][vsi_id].vport_bitmap,
		sizeof(ref_vsi_mapping[dev_id][vsi_id].vport_bitmap));

	SSDK_DEBUG("vsi %d released\n", vsi_id);

	return SW_OK;
}

sw_error_t ppe_vsi_init(a_uint32_t dev_id)
{
	hsl_reg_mode access_mode = ssdk_switch_reg_access_mode_get(dev_id);

	/*ppe_port_vlan_vsi_set/get need to use ppe_vlan_vsi_lock*/
	REF_VLAN_VSI_LOCK_INIT(ppe_vlan_vsi_lock[dev_id], access_mode);

	return SW_OK;
}

sw_error_t ppe_vsi_tbl_dump(a_uint32_t dev_id, a_uint32_t vsi_id, ref_vsi_info_t *vsi_info)
{
	ref_vlan_info_t *p_vsi_info = NULL;
	a_uint32_t i = 0;

	REF_DEV_ID_CHECK(dev_id);
	if(ref_vsi_mapping[dev_id][vsi_id].valid == 0)
		return SW_OK;
	vsi_info->vsi_id = vsi_id;
	vsi_info->valid =
		ref_vsi_mapping[dev_id][vsi_id].valid;
	vsi_info->port_info.pport_bitmap =
		ref_vsi_mapping[dev_id][vsi_id].pport_bitmap;
	p_vsi_info = ref_vsi_mapping[dev_id][vsi_id].pHead;
	for(i = 0; i < SSDK_MAX_VIRTUAL_PORT_NUM/32; i++)
	{
		vsi_info->port_info.vport_bitmap[i] =
			ref_vsi_mapping[dev_id][vsi_id].vport_bitmap[i];
	}
	i = 0;
	while(p_vsi_info != NULL && i < VSI_VLAN_XLT_RULE_NUM)
	{
		vsi_info->vlan_info[i].ctag_vid = p_vsi_info->ctag_vid;
		vsi_info->vlan_info[i].stag_vid = p_vsi_info->stag_vid;
		vsi_info->vlan_info[i].vlan_port_bitmap = p_vsi_info->vlan_port_bitmap;
		p_vsi_info = p_vsi_info->pNext;
		i++;
	}

	return SW_OK;
}

EXPORT_SYMBOL(ppe_port_vlan_vsi_set);
EXPORT_SYMBOL(ppe_port_vlan_vsi_get);
EXPORT_SYMBOL(ppe_port_vsi_set);
EXPORT_SYMBOL(ppe_port_vsi_get);
EXPORT_SYMBOL(ppe_vsi_alloc);
EXPORT_SYMBOL(ppe_vsi_free);


