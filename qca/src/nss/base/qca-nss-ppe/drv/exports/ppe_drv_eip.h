/*
 * Copyright (c) 2022-2023 Qualcomm Innovation Center, Inc. All rights reserved.
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

/**
 * @file ppe_drv_eip.h
 *	NSS PPE EIP definitions.
 */

#ifndef _PPE_DRV_EIP_H_
#define _PPE_DRV_EIP_H_

/**
 * @addtogroup ppe_drv_eip_subsystem
 * @{
 */

/*
 * EIP feature list
 */
#define PPE_DRV_EIP_FEATURE_MTU_DISABLE 0x1	/* Disable MTU for EIP VP interface */

/*
 * ppe_drv_eip_service
 *	Type of inline EIP service.
 */
typedef enum ppe_drv_eip_service {
	PPE_DRV_EIP_SERVICE_IIPSEC = 1,
	PPE_DRV_EIP_SERVICE_IDTLS,
	PPE_DRV_EIP_SERVICE_NONINLINE,
	PPE_DRV_EIP_SERVICE_FULL_INLINE,
} ppe_drv_eip_service_t;

/*
 * ppe_drv_eip_flow_lookup_mode
 *	Type of flow lookup for EIP.
 */
typedef enum ppe_drv_eip_flow_lookup_mode {
	PPE_DRV_EIP_FLOW_LOOKUP_MODE_TR = 0,	/**< FLowlookup type is Transform Record lookup. */
	PPE_DRV_EIP_FLOW_LOOKUP_MODE_FR,	/**< FLowlookup type is Flow Record lookup. */
} ppe_drv_eip_flow_lookup_mode_t;

/**
 * ppe_drv_eip_deinit
 *	Deinitialize EIP.
 *
 * @datatypes
 * ppe_drv_iface
 *
 * @param[in] iface   Pointer to the PPE interface for bridge.
 *
 * @return
 * Status of the operation.
 */
ppe_drv_ret_t ppe_drv_eip_deinit(struct ppe_drv_iface *iface);

/**
 * ppe_drv_eip_init
 *	Initialize EIP.
 *
 * @datatypes
 * ppe_drv_iface
 *
 * @param[in] iface   Pointer to the PPE interface for bridge.
 *
 * @return
 * Status of the operation.
 */
ppe_drv_ret_t ppe_drv_eip_init(struct ppe_drv_iface *iface);

/**
 * ppe_drv_eip_set_lookup_mode
 *	Set lookup mode to transform record lookup mode.
 *
 * @datatypes
 * ppe_drv_eip_flow_lookup_mode_t
 *
 * @params[IN] mode	EIP HW lookup mode
 *
 * @return
 * Status of the operation.
 */
ppe_drv_ret_t ppe_drv_eip_set_lookup_mode(ppe_drv_eip_flow_lookup_mode_t mode);

/**
 * ppe_drv_eip_sc_queue_config_set
 *	Configure ucast queue for service code PPE_DRV_SC_IPSEC_PPE2EIP_DECAP
 *
 * @datatypes
 *
 * @params
 *
 * @return
 */
void ppe_drv_eip_sc_queue_config_set(void);

/** @} */ /* end_addtogroup ppe_drv_eip_subsystem */

#endif /* _PPE_DRV_EIP_H_ */
