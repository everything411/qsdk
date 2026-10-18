/*
 * Copyright (c) 2016-2018, The Linux Foundation. All rights reserved.
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: ISC
 */

/**
 * @defgroup fal_pppoe FAL_PPPOE
 * @{
 */
#ifndef _FAL_PPPOE_H_
#define _FAL_PPPOE_H_

#ifdef __cplusplus
extern "C" {
#endif                          /* __cplusplus */

#include "sw.h"
#include "fal/fal_type.h"
#include "fal_pppoe_legacy.h"

typedef struct
{
	a_uint32_t entry_id; /* the entry id saved to pppoe table */
	a_uint32_t session_id; /* pppoe session id */
	a_bool_t   multi_session; /* matched multicast session */
	a_bool_t   uni_session; /* matched unicast session */
	a_uint32_t vrf_id; /* vrf id, HPPE not supported */
	fal_pbmp_t port_bitmap; /* matched ports */
	a_uint32_t l3_if_index; /* if matched, this l3 interface index will remove pppoe header */
	a_bool_t   l3_if_valid; /* this l3 interface index valid or not */
	a_bool_t   smac_valid; /* if matched source mac address */
	fal_mac_addr_t smac_addr; /* matched source mac address */
	a_uint32_t tl_l3_if_index; /* tunnel pppoe l3_if check, added for ipq95xx */
	a_bool_t   tl_l3_if_valid; /* tunnel l3 interface index valid or not, added for ipq95xx */
} fal_pppoe_session_t;

typedef struct {
	fal_fwd_cmd_t pppoe_multicast_cmd; /* pppoe multicast action with IP routing enable */
	a_bool_t pppoe_multicast_deacclr_en; /*0 for disable and 1 for enable*/
} fal_pppoe_global_cfg_t;

sw_error_t
fal_pppoe_l3_intf_set(a_uint32_t dev_id, a_uint32_t pppoe_index,
		fal_intf_type_t l3_type, fal_intf_id_t *pppoe_intf);

sw_error_t
fal_pppoe_l3_intf_get(a_uint32_t dev_id, a_uint32_t pppoe_index,
		fal_intf_type_t l3_type, fal_intf_id_t *pppoe_intf);

sw_error_t
fal_pppoe_global_ctrl_set(a_uint32_t dev_id, fal_pppoe_global_cfg_t *cfg);

sw_error_t
fal_pppoe_global_ctrl_get(a_uint32_t dev_id, fal_pppoe_global_cfg_t *cfg);

sw_error_t
fal_pppoe_session_table_add(a_uint32_t dev_id, fal_pppoe_session_t * session_tbl);

sw_error_t
fal_pppoe_session_table_del(a_uint32_t dev_id, fal_pppoe_session_t * session_tbl);

sw_error_t
fal_pppoe_session_table_get(a_uint32_t dev_id, fal_pppoe_session_t * session_tbl);

sw_error_t
fal_pppoe_l3intf_enable(a_uint32_t dev_id, a_uint32_t l3_if, a_uint32_t enable);

sw_error_t
fal_pppoe_l3intf_status_get(a_uint32_t dev_id, a_uint32_t l3_if, a_uint32_t *enable);


#ifdef __cplusplus
}
#endif                          /* __cplusplus */
#endif                          /* _FAL_PPPOE_H_ */
/**
 * @}
 */
