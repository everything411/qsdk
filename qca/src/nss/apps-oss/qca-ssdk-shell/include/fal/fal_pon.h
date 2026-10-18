/*
* Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
* SPDX-License-Identifier: ISC
*/

/**
 * @defgroup fal_gpon _FAL_PON_H_
 * @{
 */
#ifndef _FAL_PON_H_
#define _FAL_PON_H_

#ifdef __cplusplus
extern "C" {
#endif                          /* __cplusplus */

#include "sw.h"
#include "fal_type.h"

#define PON_PORT_ID 6

/* US mapping gemport global config */
typedef struct {
	/* when gemport_gen rule mismatch,
	 * but dest match this port then will fellow gen_miss_cmd */
	a_uint8_t gen_miss_pon_port;
	/* when gemport_gen rule mismatch,
	 * but dest match genmschk_pon_port or bypasbit DOT1P_DST_LKUP_BYPASS set,
	 * applied flowarding actions. */
	fal_fwd_cmd_t gen_miss_cmd;

	fal_vlan_match_mode_t vlan_mode; /* 0: use VLAN, 1: use VSI, to match gemport_gen vlan field */
	fal_pcp_match_mode_t pcp_mode; /*  0: use pcp and dei, 1: use int_pri, to match gemport_gen pcp and dei field */
} fal_gemport_global_cfg_t;

/* US mapping gemport default properties */
typedef struct {
	a_uint32_t vlan_id; /* For untagged packet assigned VID */
	a_uint8_t pcp; /* For untagged packet assigned PCP */
	a_uint8_t dei; /* For untagged packet assigned DEI */
	a_uint8_t dscp; /* For NON-IP packet assigned DSCP */
	a_uint8_t dscp_mask; /* For IP packet Lookup DSCP mask value */
} fal_gemport_gen_default_t;

/* US mapping gemport */
typedef struct {
	a_bool_t src_info_valid; /* check if rule include src info */
	fal_port_t src_info; /* src info, port bitmap(FAL_PORT_TYPE_PPORT) or vp(FAL_PORT_TYPE_VPORT) */
	a_bool_t dest_info_valid; /* check if rule include dest port id */
	fal_dest_info_t dest_info; /* dest portid, physical or virtual portid */
	a_bool_t vlan_id_valid; /* check if rule include vid */
	a_uint32_t vlan_id; /* vlan id */
	a_uint8_t pri_type; /* priority type, 0: use VLAN PCP+DEI, 1: use DSCP */
	a_bool_t pcp_valid; /* check if rule include pcp, pri_type = 0 */
	a_uint8_t pcp; /* pcp value */
	a_bool_t dei_valid; /* check if rule include vid, pri_type = 0 */
	a_uint8_t dei; /* dei value */
	a_bool_t dscp_valid; /* check if rule include dscp, pri_type = 1 */
	a_uint8_t dscp; /* dscp value */

	a_uint8_t gemport; /* gemport id, 0 - 127 */
} fal_gemport_gen_t;

/* DS gemport mapping */
typedef struct {
	a_bool_t src_en; /* check if src info would be mapped */
	a_uint8_t src_port; /* map to src port, pp or vp */
	a_bool_t int_pri_dp_en; /* check if internal priority,drop precedence would be mapped */
	a_uint8_t int_pri; /* internal priority 0-15 */
	a_uint8_t int_dp; /* Internal drop precedence */
	a_bool_t dest_en; /* check if dest info would be generated */
	fal_dest_info_t dest_info; /* dest info, pp, vp, port bitmap */
	a_uint8_t service_code; /* service code */
} fal_gemport_map_t;

/* gemport config */
typedef struct {
	a_bool_t service_code_en; /* check if action will enable service code update */
	a_uint8_t service_code; /* service code */
	a_bool_t int_pri_en; /* check if action will change internal priority */
	a_uint8_t int_pri; /* internal priority 0-15 */
	a_bool_t int_dp_en; /* check if action will change internal drop precedence */
	a_uint8_t int_dp; /* internal drop precedence */
	a_bool_t enq_vp_en; /* check if action will change enqueue vp */
	a_uint8_t enq_vp; /* enqueue vp */
	a_bool_t dest_en; /* check if action will change dest port */
	/* dest portid, used for update the vp(vip) from first round of ppe hgu or
	 * vp from host, could be physical or virtual portid, mostly the PON_PORT_ID */
	a_uint8_t dest_pp;
	a_uint8_t dest_vp;
	fal_fwd_cmd_t fwd_cmd; /* action command */
	a_uint8_t bypass_bitmap; /* qm,sawf bypassbitmap, refer POST_BYPASS_BITMAP of IN_L2_SERVICE_TBL */
} fal_gemport_cfg_t;

/* gemport policer config */
typedef struct {
	a_bool_t us_policer_en; /* check if action will generate policer idx for US */
	a_bool_t ds_policer_en; /* check if action will generate policer idx for DS, 
							 * DS policer id value would be us_policer_idx + 1 */

	a_uint32_t us_policer_idx; /* US policer idx, range from 0 to 126 */
} fal_gemport_policer_t;

sw_error_t
fal_pon_gemport_global_set(a_uint32_t dev_id, fal_gemport_global_cfg_t *cfg);
sw_error_t
fal_pon_gemport_global_get(a_uint32_t dev_id, fal_gemport_global_cfg_t *cfg);

sw_error_t
fal_pon_gemport_gen_default_set(a_uint32_t dev_id,
				fal_gemport_gen_default_t *def_property);
sw_error_t
fal_pon_gemport_gen_default_get(a_uint32_t dev_id,
				fal_gemport_gen_default_t *def_property);

sw_error_t
fal_pon_gemport_gen_entry_set(a_uint32_t dev_id, a_uint32_t index, fal_gemport_gen_t *gen_entry);
sw_error_t
fal_pon_gemport_gen_entry_get(a_uint32_t dev_id, a_uint32_t index, fal_gemport_gen_t *gen_entry);

sw_error_t
fal_pon_gemport_gen_en_set(a_uint32_t dev_id, a_uint32_t gemport, a_bool_t enable);
sw_error_t
fal_pon_gemport_gen_en_get(a_uint32_t dev_id, a_uint32_t gemport, a_bool_t *enable);

sw_error_t
fal_pon_gemport_map_en_set(a_uint32_t dev_id, fal_port_t port_id, a_bool_t mapping_en);
sw_error_t
fal_pon_gemport_map_en_get(a_uint32_t dev_id, fal_port_t port_id, a_bool_t *mapping_en);

sw_error_t
fal_pon_gemport_map_set(a_uint32_t dev_id, a_uint32_t gemport,
				fal_gemport_map_t *map_entry);
sw_error_t
fal_pon_gemport_map_get(a_uint32_t dev_id, a_uint32_t gemport,
				fal_gemport_map_t *map_entry);

sw_error_t
fal_pon_gemport_cfg_set(a_uint32_t dev_id, a_uint32_t gemport,
				fal_gemport_cfg_t *cfg);
sw_error_t
fal_pon_gemport_cfg_get(a_uint32_t dev_id, a_uint32_t gemport,
				fal_gemport_cfg_t *cfg);

sw_error_t
fal_pon_gemport_policer_set(a_uint32_t dev_id, a_uint32_t gemport,
			fal_gemport_policer_t *policer_cfg);
sw_error_t
fal_pon_gemport_policer_get(a_uint32_t dev_id, a_uint32_t gemport,
			fal_gemport_policer_t *policer_cfg);

#ifdef __cplusplus
}
#endif                          /* __cplusplus */
#endif                          /* _FAL_PON_H_ */
/**
 * @}
 */

