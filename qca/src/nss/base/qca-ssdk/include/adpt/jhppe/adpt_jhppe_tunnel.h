/*
* Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
* SPDX-License-Identifier: ISC
*/

#ifndef _ADPT_JHPPE_TUNNEL_
#define _ADPT_JHPPE_TUNNEL_

#ifdef __cplusplus
extern "C" {
#endif                          /* __cplusplus */

sw_error_t
adpt_jhppe_tunnel_tuple_entry_add(a_uint32_t dev_id, fal_tunnel_tuple_entry_t *entry);

sw_error_t
adpt_jhppe_tunnel_tuple_entry_del(a_uint32_t dev_id, fal_tunnel_tuple_entry_t *entry);

#ifndef IN_TUNNEL_MINI
sw_error_t
adpt_jhppe_tunnel_tuple_entry_getfirst(a_uint32_t dev_id, fal_tunnel_tuple_entry_t *entry);

sw_error_t
adpt_jhppe_tunnel_tuple_entry_getnext(a_uint32_t dev_id, fal_tunnel_tuple_entry_t *entry);
#endif

sw_error_t
adpt_jhppe_tunnel_decap_miss_action_set(a_uint32_t dev_id,
		fal_tunnel_type_t tunnel_type, fal_tunnel_decap_miss_action_t *miss_action);

#ifndef IN_TUNNEL_MINI
sw_error_t
adpt_jhppe_tunnel_decap_miss_action_get(a_uint32_t dev_id,
		fal_tunnel_type_t tunnel_type, fal_tunnel_decap_miss_action_t *miss_action);
#endif
#ifdef __cplusplus
}
#endif                          /* __cplusplus */
#endif
