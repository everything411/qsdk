/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: ISC
 */

/**
 * @file ppe_drv_port_mgmt.h
 *      NSS PPE DRV Port Management definitions.
 */

#include <fal/fal_portvlan.h>
#include <fal/fal_api.h>
#include <ppe_drv_port_mgmt.h>

/*
 * ppe_drv_port_mgmt_entries_free()
 * 	Free the port mgmt memory.
 */
void ppe_drv_port_mgmt_entries_free(struct ppe_drv_port_mgmt *port_mgmt);

/*
 * ppe_drv_port_mgmt_entries_alloc()
 * 	Allocate the port mgmt memory.
 */
struct ppe_drv_port_mgmt *ppe_drv_port_mgmt_entries_alloc(void);
