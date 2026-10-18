/*
* Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
* SPDX-License-Identifier: ISC
*/

/**
 * @defgroup fal_pon FAL_PON
 * @{
 */
#include "sw.h"
#include "fal_pon.h"
#include "adpt.h"

sw_error_t fal_pon_gemport_global_set(a_uint32_t dev_id, fal_gemport_global_cfg_t *cfg)
    DEFINE_FAL_FUNC_ADPT_EXPORT(pon_gemport_global_set, dev_id, cfg)

sw_error_t fal_pon_gemport_global_get(a_uint32_t dev_id, fal_gemport_global_cfg_t *cfg)
    DEFINE_FAL_FUNC_ADPT_EXPORT(pon_gemport_global_get, dev_id, cfg)

sw_error_t
fal_pon_gemport_gen_default_set(a_uint32_t dev_id, fal_gemport_gen_default_t *def_property)
	DEFINE_FAL_FUNC_ADPT_EXPORT(pon_gemport_gen_default_set, dev_id, def_property)

sw_error_t
fal_pon_gemport_gen_default_get(a_uint32_t dev_id, fal_gemport_gen_default_t *def_property)
	DEFINE_FAL_FUNC_ADPT_EXPORT(pon_gemport_gen_default_get, dev_id, def_property)

sw_error_t
fal_pon_gemport_gen_entry_set(a_uint32_t dev_id, a_uint32_t index, fal_gemport_gen_t *gen_entry)
	DEFINE_FAL_FUNC_ADPT_EXPORT(pon_gemport_gen_entry_set, dev_id, index, gen_entry)

sw_error_t
fal_pon_gemport_gen_entry_get(a_uint32_t dev_id, a_uint32_t index, fal_gemport_gen_t *gen_entry)
	DEFINE_FAL_FUNC_ADPT_EXPORT(pon_gemport_gen_entry_get, dev_id, index, gen_entry)

sw_error_t
fal_pon_gemport_gen_en_set(a_uint32_t dev_id, a_uint32_t gemport, a_bool_t enable)
	DEFINE_FAL_FUNC_ADPT_EXPORT(pon_gemport_gen_en_set, dev_id, gemport, enable)

sw_error_t
fal_pon_gemport_gen_en_get(a_uint32_t dev_id, a_uint32_t gemport, a_bool_t *enable)
	DEFINE_FAL_FUNC_ADPT_EXPORT(pon_gemport_gen_en_get, dev_id, gemport, enable)

sw_error_t
fal_pon_gemport_map_en_set(a_uint32_t dev_id, fal_port_t port_id, a_bool_t mapping_en)
	DEFINE_FAL_FUNC_ADPT_EXPORT(pon_gemport_map_en_set, dev_id, port_id, mapping_en)

sw_error_t
fal_pon_gemport_map_en_get(a_uint32_t dev_id, fal_port_t port_id, a_bool_t *mapping_en)
	DEFINE_FAL_FUNC_ADPT_EXPORT(pon_gemport_map_en_get, dev_id, port_id, mapping_en)

sw_error_t
fal_pon_gemport_map_set(a_uint32_t dev_id, a_uint32_t gemport, fal_gemport_map_t *map_entry)
	DEFINE_FAL_FUNC_ADPT_EXPORT(pon_gemport_map_set, dev_id, gemport, map_entry)

sw_error_t
fal_pon_gemport_map_get(a_uint32_t dev_id, a_uint32_t gemport, fal_gemport_map_t *map_entry)
	DEFINE_FAL_FUNC_ADPT_EXPORT(pon_gemport_map_get, dev_id, gemport, map_entry)

sw_error_t
fal_pon_gemport_cfg_set(a_uint32_t dev_id, a_uint32_t gemport, fal_gemport_cfg_t *cfg)
	DEFINE_FAL_FUNC_ADPT_EXPORT(pon_gemport_cfg_set, dev_id, gemport, cfg)

sw_error_t
fal_pon_gemport_cfg_get(a_uint32_t dev_id, a_uint32_t gemport, fal_gemport_cfg_t *cfg)
	DEFINE_FAL_FUNC_ADPT_EXPORT(pon_gemport_cfg_get, dev_id, gemport, cfg)

sw_error_t
fal_pon_gemport_policer_set(a_uint32_t dev_id, a_uint32_t gemport, fal_gemport_policer_t *policer_cfg)
	DEFINE_FAL_FUNC_ADPT_EXPORT(pon_gemport_policer_set, dev_id, gemport, policer_cfg)

sw_error_t
fal_pon_gemport_policer_get(a_uint32_t dev_id, a_uint32_t gemport, fal_gemport_policer_t *policer_cfg)
	DEFINE_FAL_FUNC_ADPT_EXPORT(pon_gemport_policer_get, dev_id, gemport, policer_cfg)

/**
 * @}
 */
