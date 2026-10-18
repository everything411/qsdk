/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: ISC
 */

/**
 * @addtogroup ppe_drv_cc_subsystem
 * @{
 */

#ifndef _PPE_DRV_CC_H_
#define _PPE_DRV_CC_H_

#include "ppe_drv_cc_usr.h"

struct ppe_drv;

#define PPE_DRV_CC_RANGE1			PPE_DRV_CC_UDP_LITE_CHECKSUM_ERR	/* CPU Code For Udp Lite Checksum Err */
#define PPE_DRV_CC_RANGE2			PPE_DRV_CC_RESERVE0			/* PPE_DRV_CC_RESERVE0 */
#define PPE_DRV_CC_RANGE3			PPE_DRV_CC_INNER_PACKET_TOO_SHORT	/* CPU Code For Inner Packet Too Short */
#define PPE_DRV_CC_RANGE4			PPE_DRV_CC_GRE_HDR			/* CPU Code For GRE Header */
#define PPE_DRV_CC_RANGE5			PPE_DRV_CC_PROGRAM5			/* CPU Code For Program5 */
#define PPE_DRV_EXP_RANGE1_BASE			1
#define PPE_DRV_EXP_RANGE2_BASE			144

/**
 * ppe_drv_cc_metadata
 *	Metadata for CPU codes.
 */
struct ppe_drv_cc_metadata {
	uint16_t cpu_code;			/**< CPU code for the packet */
	uint16_t acl_hw_index;			/**< Hardware ACL index */
	bool acl_index_valid;			/**< ACL rule Valid bit */
	bool fake_mac;				/**< Packet with fake MAC header */
	uint8_t src_vp_num;			/* Source VP number */
	bool is_src_vp;				/* Is source a VP */
};

typedef bool (*ppe_drv_cc_callback_t)(void *app_data, struct sk_buff *skb, void *cc_info);

/*
 * ppe_drv_cc_process_skbuff()
 *	Register callback for a specific CPU code
 *
 * @param[IN] cc_info		CPU code metadata.
 * @param[IN] skb		Socket buffer with CPU code.
 *
 * @return
 * true if packet is consumed by the API or false if the packet is not consumed.
 */
extern bool ppe_drv_cc_process_skbuff(struct ppe_drv_cc_metadata *cc_info, struct sk_buff *skb);

/*
 * ppe_drv_cc_ucast_qbase_profile_set()
 *	API to configure qbase profile for the particular CPU code
 *
 * @param[IN] cc		CPU code
 * @param[IN] qbase		Queue base for ucast qbase profile configuration
 *
 * @return
 * True if the configuration is successful
 */
extern bool ppe_drv_cc_ucast_qbase_profile_set(uint16_t cc, uint32_t qbase);

/*
 * ppe_drv_cc_unregister_cb()
 *	Unregister callback for a specific CPU code
 *
 * @param[IN] cc   CPU code number.
 *
 * @return
 * void
 */
extern void ppe_drv_cc_unregister_cb(ppe_drv_cc_t cc);

/*
 * ppe_drv_cc_register_cb()
 *	Register callback for a specific CPU code
 *
 * @param[IN] cc   Service code number.
 * @param[IN] cb   Callback API.
 * @param[IN] app_data   Application data to be passed to callback.
 *
 * @return
 * void
 */
extern void ppe_drv_cc_register_cb(ppe_drv_cc_t cc, ppe_drv_cc_callback_t cb, void *app_data);

/** @} */ /* end_addtogroup ppe_drv_cc_subsystem */

#endif /* _PPE_DRV_CC_H_ */
