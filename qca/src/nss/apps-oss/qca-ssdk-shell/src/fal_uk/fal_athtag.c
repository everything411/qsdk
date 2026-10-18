/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: ISC
 */

#include "sw_ioctl.h"
#include "fal_athtag.h"
#include "fal_uk_if.h"

sw_error_t
fal_athtag_pri_mapping_set(a_uint32_t dev_id,
		fal_direction_t direction, fal_athtag_pri_mapping_t *pri_mapping)
{
    sw_error_t rv;

    rv = sw_uk_exec(SW_API_ATHTAG_PRI_MAPPING_SET, dev_id, direction, pri_mapping);
    return rv;
}

sw_error_t
fal_athtag_pri_mapping_get(a_uint32_t dev_id,
		fal_direction_t direction, fal_athtag_pri_mapping_t *pri_mapping)
{
    sw_error_t rv;

    rv = sw_uk_exec(SW_API_ATHTAG_PRI_MAPPING_GET, dev_id, direction, pri_mapping);
    return rv;
}

sw_error_t
fal_athtag_port_mapping_set(a_uint32_t dev_id,
		fal_direction_t direction, fal_athtag_port_mapping_t *port_mapping)
{
    sw_error_t rv;

    rv = sw_uk_exec(SW_API_ATHTAG_PORT_MAPPING_SET, dev_id, direction, port_mapping);
    return rv;
}

sw_error_t
fal_athtag_port_mapping_get(a_uint32_t dev_id,
		fal_direction_t direction, fal_athtag_port_mapping_t *port_mapping)
{
    sw_error_t rv;

    rv = sw_uk_exec(SW_API_ATHTAG_PORT_MAPPING_GET, dev_id, direction, port_mapping);
    return rv;
}

sw_error_t
fal_port_athtag_rx_set(a_uint32_t dev_id, fal_port_t port_id, fal_athtag_rx_cfg_t *cfg)
{
    sw_error_t rv;

    rv = sw_uk_exec(SW_API_PORT_ATHTAG_RX_SET, dev_id, port_id, cfg);
    return rv;
}

sw_error_t
fal_port_athtag_rx_get(a_uint32_t dev_id, fal_port_t port_id, fal_athtag_rx_cfg_t *cfg)
{
    sw_error_t rv;

    rv = sw_uk_exec(SW_API_PORT_ATHTAG_RX_GET, dev_id, port_id, cfg);
    return rv;
}

sw_error_t
fal_port_athtag_tx_set(a_uint32_t dev_id, fal_port_t port_id, fal_athtag_tx_cfg_t *cfg)
{
    sw_error_t rv;

    rv = sw_uk_exec(SW_API_PORT_ATHTAG_TX_SET, dev_id, port_id, cfg);
    return rv;
}

sw_error_t
fal_port_athtag_tx_get(a_uint32_t dev_id, fal_port_t port_id, fal_athtag_tx_cfg_t *cfg)
{
    sw_error_t rv;

    rv = sw_uk_exec(SW_API_PORT_ATHTAG_TX_GET, dev_id, port_id, cfg);
    return rv;
}

sw_error_t
fal_athtag_rx_src_port_mapping_set(a_uint32_t dev_id, fal_port_t ath_src_port,
		fal_dest_info_t *int_dest_info)
{
    sw_error_t rv;

    rv = sw_uk_exec(SW_API_ATHTAG_RX_SRC_PORT_MAPPING_SET, dev_id, ath_src_port, int_dest_info);
    return rv;
}

sw_error_t
fal_athtag_rx_src_port_mapping_get(a_uint32_t dev_id, fal_port_t ath_src_port,
		fal_dest_info_t *int_dest_info)
{
    sw_error_t rv;

    rv = sw_uk_exec(SW_API_ATHTAG_RX_SRC_PORT_MAPPING_GET, dev_id, ath_src_port, int_dest_info);
    return rv;
}

sw_error_t
fal_athtag_rx_dest_port_mapping_set(a_uint32_t dev_id, fal_port_t ath_dest_port,
		fal_dest_info_t *int_dest_info)
{
    sw_error_t rv;

    rv = sw_uk_exec(SW_API_ATHTAG_RX_DEST_PORT_MAPPING_SET, dev_id, ath_dest_port, int_dest_info);
    return rv;
}

sw_error_t
fal_athtag_rx_dest_port_mapping_get(a_uint32_t dev_id, fal_port_t ath_dest_port,
		fal_dest_info_t *int_dest_info)
{
    sw_error_t rv;

    rv = sw_uk_exec(SW_API_ATHTAG_RX_DEST_PORT_MAPPING_GET, dev_id, ath_dest_port, int_dest_info);
    return rv;
}

sw_error_t
fal_athtag_rx_servcode_mapping_set(a_uint32_t dev_id, a_uint32_t ath_servcode,
		a_uint32_t int_servcode)
{
    sw_error_t rv;

    rv = sw_uk_exec(SW_API_ATHTAG_RX_SERVCODE_MAPPING_SET, dev_id, ath_servcode, int_servcode);
    return rv;
}

sw_error_t
fal_athtag_rx_servcode_mapping_get(a_uint32_t dev_id, a_uint32_t ath_servcode,
		a_uint32_t *int_servcode)
{
    sw_error_t rv;

    rv = sw_uk_exec(SW_API_ATHTAG_RX_SERVCODE_MAPPING_GET, dev_id, ath_servcode, int_servcode);
    return rv;
}

sw_error_t
fal_athtag_tx_src_port_mapping_set(a_uint32_t dev_id, fal_port_t int_src_port,
		fal_port_t ath_src_port)
{
    sw_error_t rv;

    rv = sw_uk_exec(SW_API_ATHTAG_TX_SRC_PORT_MAPPING_SET, dev_id, int_src_port, ath_src_port);
    return rv;
}

sw_error_t
fal_athtag_tx_src_port_mapping_get(a_uint32_t dev_id, fal_port_t int_src_port,
		fal_port_t *ath_src_port)
{
    sw_error_t rv;

    rv = sw_uk_exec(SW_API_ATHTAG_TX_SRC_PORT_MAPPING_GET, dev_id, int_src_port, ath_src_port);
    return rv;
}
