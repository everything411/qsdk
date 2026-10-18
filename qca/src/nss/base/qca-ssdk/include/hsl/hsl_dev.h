/*
 * Copyright (c) 2012, 2019, 2021, The Linux Foundation. All rights reserved.
* Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
* SPDX-License-Identifier: ISC
*/

/*qca808x_start*/
#ifndef _HSL_DEV_H
#define _HSL_DEV_H

#ifdef __cplusplus
extern "C" {
#endif                          /* __cplusplus */

#include "hsl_api.h"
#include "ssdk_init.h"

a_uint32_t hsl_ppe_module_base_get(a_uint32_t dev_id, a_uint32_t ppe_reg_module);

#define NSS_PPE_INVALID_BASE_ADDR           0xF00000 
#define PRX_BASE_ADDR                       0xB000
#define NSS_PRX_CSR_BASE_ADDR               PRX_BASE_ADDR

enum {
	NSS_PPE_PTX = 0,
	NSS_PPE_L2 = 1,
	NSS_PPE_POLICER = 2,
	NSS_PPE_BM = 3,
	NSS_PPE_QM = 4,
	NSS_PPE_EDMA = 5,
	NSS_PPE_LPI= 6,
	/* Max entry to check bounds */
	NSS_PPE_MAX_MODULE_ENTRIES
};

#define EDMA_BASE_ADDR                      hsl_ppe_module_base_get(dev_id, NSS_PPE_EDMA)
#define IPE_L3_BASE_ADDR                    0x200000
#define QUEUE_MANAGER_BASE_ADDR             hsl_ppe_module_base_get(dev_id, NSS_PPE_QM)
#define TRAFFIC_MANAGER_BASE_ADDR           0x400000
#define INGRESS_POLICER_BASE_ADDR           hsl_ppe_module_base_get(dev_id, NSS_PPE_POLICER)
#define INGRESS_VLAN_BASE_ADDR              0x00f000
#define IPE_L2_BASE_ADDR                    hsl_ppe_module_base_get(dev_id, NSS_PPE_L2)
#define IPO_CSR_BASE_ADDR                   0x0b0000
#define PRE_IPO_CSR_BASE_ADDR               0x180000
#define IPR_CSR_BASE_ADDR                   0x1e0000
#define NSS_XGMAC_CSR_BASE_ADDR             0x500000
#define TUNNEL_LOOKUP_BASE_ADDR             0x300000
#define TUNNEL_PARSER_BASE_ADDR             0x1d0000
#define NSS_MAC_CSR_BASE_ADDR               0x001000
#define NSS_PTX_CSR_BASE_ADDR               hsl_ppe_module_base_get(dev_id, NSS_PPE_PTX)
#define NSS_BM_CSR_BASE_ADDR                hsl_ppe_module_base_get(dev_id, NSS_PPE_BM)
#define NSS_GLOBAL_BASE_ADDR                0x0
#define NSS_UNIPHY_BASE_ADDR                0x0
#define NSS_LPI_BASE_ADDR                   hsl_ppe_module_base_get(dev_id, NSS_PPE_LPI)

#define HSL_DEV_ID_CHECK(dev_id) \
do { \
    if (dev_id >= SW_MAX_NR_DEV) \
        return SW_OUT_OF_RANGE; \
} while (0)

#define HSL_PORT_ID_CHECK(port_id) \
do { \
    if (port_id >= SW_MAX_NR_PORT) \
        return SW_OUT_OF_RANGE; \
} while (0)

typedef struct {
	a_uint32_t dev_id;
	a_uint8_t cpu_port_nr;
	a_uint8_t nr_ports;
	a_uint8_t nr_phy;
	a_uint8_t nr_queue;
	a_uint16_t nr_vlans;
	a_bool_t hw_vlan_query;
	hsl_init_mode  cpu_mode;
	a_uint32_t wan_bmp;
} hsl_dev_t;

hsl_dev_t *hsl_dev_ptr_get(a_uint32_t dev_id);

ssdk_chip_type hsl_get_current_chip_type(a_uint32_t dev_id);

sw_error_t
hsl_dev_init(a_uint32_t dev_id, ssdk_init_cfg * cfg);

sw_error_t
hsl_dev_cleanup(a_uint32_t dev_id);

/*qca808x_end*/
sw_error_t
hsl_ssdk_cfg(a_uint32_t dev_id, ssdk_cfg_t *ssdk_cfg);

sw_error_t
hsl_access_mode_set(a_uint32_t dev_id, hsl_access_mode reg_mode);

a_uint32_t hsl_dev_inner_ports_get(a_uint32_t dev_id);
sw_error_t hsl_reg_modify(a_uint32_t dev_id, a_uint32_t reg, a_uint32_t mask,
	a_uint32_t value);
/*qca808x_start*/
#ifdef __cplusplus
}
#endif                          /* __cplusplus */
#endif                          /* _HSL_DEV_H */
/*qca808x_end*/

