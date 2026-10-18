/*
* Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
* SPDX-License-Identifier: ISC
*/

#include "sw_ioctl.h"
#include "fal_pon.h"
#include "fal_uk_if.h"

sw_error_t fal_pon_gemport_global_set(a_uint32_t dev_id, fal_gemport_global_cfg_t *cfg)
{
	sw_error_t rv;

	rv = sw_uk_exec(SW_API_PON_GEMPORT_GLB_SET, dev_id, cfg);
	return rv;
}

sw_error_t fal_pon_gemport_global_get(a_uint32_t dev_id, fal_gemport_global_cfg_t *cfg)
{
    sw_error_t rv;

    rv = sw_uk_exec(SW_API_PON_GEMPORT_GLB_GET, dev_id, cfg);
    return rv;
}

sw_error_t
fal_pon_gemport_gen_default_set(a_uint32_t dev_id, fal_gemport_gen_default_t *def_property)
{
    sw_error_t rv;

    rv = sw_uk_exec(SW_API_PON_GEMPORT_GEN_DFT_SET, dev_id, def_property);
    return rv;
}

sw_error_t
fal_pon_gemport_gen_default_get(a_uint32_t dev_id, fal_gemport_gen_default_t *def_property)
{
    sw_error_t rv;

    rv = sw_uk_exec(SW_API_PON_GEMPORT_GEN_DFT_GET, dev_id, def_property);
    return rv;
}

sw_error_t
fal_pon_gemport_gen_entry_set(a_uint32_t dev_id, a_uint32_t index, fal_gemport_gen_t *gen_entry)
{
    sw_error_t rv;

    rv = sw_uk_exec(SW_API_PON_GEMPORT_GEN_ENTRY_SET, dev_id, index, gen_entry);
    return rv;
}

sw_error_t
fal_pon_gemport_gen_entry_get(a_uint32_t dev_id, a_uint32_t index, fal_gemport_gen_t *gen_entry)
{
    sw_error_t rv;

    rv = sw_uk_exec(SW_API_PON_GEMPORT_GEN_ENTRY_GET, dev_id, index, gen_entry);
    return rv;
}

sw_error_t
fal_pon_gemport_gen_en_set(a_uint32_t dev_id, a_uint32_t gemport, a_bool_t enable)
{
	sw_error_t rv;

	rv = sw_uk_exec(SW_API_PON_GEMPORT_GEN_EN_SET, dev_id, gemport, enable);
	return rv;
}

sw_error_t
fal_pon_gemport_gen_en_get(a_uint32_t dev_id, a_uint32_t gemport, a_bool_t *enable)
{
	sw_error_t rv;

	rv = sw_uk_exec(SW_API_PON_GEMPORT_GEN_EN_GET, dev_id, gemport, enable);
	return rv;
}

sw_error_t
fal_pon_gemport_map_en_set(a_uint32_t dev_id, fal_port_t port_id, a_bool_t mapping_en)
{
	sw_error_t rv;

	rv = sw_uk_exec(SW_API_PON_GEMPORT_MAP_EN_SET, dev_id, port_id, mapping_en);
	return rv;
}

sw_error_t
fal_pon_gemport_map_en_get(a_uint32_t dev_id, fal_port_t port_id, a_bool_t *mapping_en)
{
	sw_error_t rv;

	rv = sw_uk_exec(SW_API_PON_GEMPORT_MAP_EN_GET, dev_id, port_id, mapping_en);
	return rv;
}

sw_error_t
fal_pon_gemport_map_set(a_uint32_t dev_id, a_uint32_t gemport, fal_gemport_map_t *map_entry)
{
	sw_error_t rv;

	rv = sw_uk_exec(SW_API_PON_GEMPORT_MAP_SET, dev_id, gemport, map_entry);
	return rv;
}

sw_error_t
fal_pon_gemport_map_get(a_uint32_t dev_id, a_uint32_t gemport, fal_gemport_map_t *map_entry)
{
	sw_error_t rv;

	rv = sw_uk_exec(SW_API_PON_GEMPORT_MAP_GET, dev_id, gemport, map_entry);
	return rv;
}

sw_error_t
fal_pon_gemport_cfg_set(a_uint32_t dev_id, a_uint32_t gemport, fal_gemport_cfg_t *cfg)
{
	sw_error_t rv;

	rv = sw_uk_exec(SW_API_PON_GEMPORT_CFG_SET, dev_id, gemport, cfg);
	return rv;
}

sw_error_t
fal_pon_gemport_cfg_get(a_uint32_t dev_id, a_uint32_t gemport, fal_gemport_cfg_t *cfg)
{
	sw_error_t rv;

	rv = sw_uk_exec(SW_API_PON_GEMPORT_CFG_GET, dev_id, gemport, cfg);
	return rv;
}

sw_error_t
fal_pon_gemport_policer_set(a_uint32_t dev_id, a_uint32_t gemport, fal_gemport_policer_t *policer_cfg)
{
	sw_error_t rv;

	rv = sw_uk_exec(SW_API_PON_GEMPORT_PLC_SET, dev_id, gemport, policer_cfg);
	return rv;
}

sw_error_t
fal_pon_gemport_policer_get(a_uint32_t dev_id, a_uint32_t gemport, fal_gemport_policer_t *policer_cfg)
{
	sw_error_t rv;

	rv = sw_uk_exec(SW_API_PON_GEMPORT_PLC_GET, dev_id, gemport, policer_cfg);
	return rv;
}
