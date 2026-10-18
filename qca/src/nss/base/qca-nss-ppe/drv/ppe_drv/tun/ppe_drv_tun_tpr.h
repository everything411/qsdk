/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: ISC
 */

#ifndef _PPE_DRV_TUN_TPR_H_
#define _PPE_DRV_TUN_TPR_H_

#define PPE_DRV_TUN_TPR_ENTRY_MAX	8	/* Max TPR entries */
#define PPE_DRV_TUN_TPR_ENTRY_TYPE_IPV4	0	/* IP version type IPv4*/
#define PPE_DRV_TUN_TPR_ENTRY_TYPE_IPV6	1	/* IP version type IPv6*/

/*
 * ppe_drv_tun_tpr_ctx_type
 *	PPE tunnel TPR context type
 */
enum ppe_drv_tun_tpr_ctx_type {
	PPE_DRV_TUN_TPR_CONTEXT_TUNNEL,       /* Tunnel Type treated as Tuple context */
	PPE_DRV_TUN_TPR_CONTEXT_TUPLE_ID,     /* Tuple ID is treated as Tuple context */
};

/*
 * ppe_drv_tun_tpr_bitmap
 *      PPE tunnel TPR bitmap
 */
enum ppe_drv_tun_tpr_bitmap{
	PPE_DRV_TUN_TPR_KEY_SIP_EN = 0,
	PPE_DRV_TUN_TPR_KEY_DIP_EN,
	PPE_DRV_TUN_TPR_KEY_L4PROTO_EN,
	PPE_DRV_TUN_TPR_KEY_SPORT_EN,
	PPE_DRV_TUN_TPR_KEY_DPORT_EN,
};

/*
 * ppe_drv_tun_tpr_entry
 *      PPE tunnel TPR entry
 */
struct ppe_drv_tun_tpr_entry {
	uint32_t src_ip[4];	/* Src IP to be matched index 0 is used for IPv4 */
	uint32_t dest_ip[4];	/* Dest IP to be matched index 0 is used for IPv4 */
	uint32_t tuple_id;	/* tuple_id if context type is tuple id */
	uint32_t tunnel_type;	/* Tunnel type if context type is tunnel. Should be a valid tuple tunnel type */
	enum ppe_drv_tun_tpr_ctx_type ctx_type;	/* Context type*/
	uint16_t src_port;	/* Source port */
	uint16_t dest_port;	/* Destination port */
	uint8_t ip_version;	/*  IP version 0 - Ipv4, 1 - IPv6 */
	uint8_t tpr_bitmap;	/* index bitmap to match 3/5tuple information */
	uint8_t protocol;	/* Ip protocol */
};

/*
 * ppe_drv_tun_tpr_prsr_udf
 *      TPR Parser UDF configurations. 3 UDF instances of 16 bit each is supported
 */
struct ppe_drv_tun_tpr_prsr_udf {
	bool udf_en[3];		/* User defined field enabled */
	uint8_t udf_offset[3];	/* User defined field offset */
	uint16_t udf_val[3];	/* User defined field value */
	uint16_t udf_mask[3];	/* User defined field mask */
};

/*
 * ppe_drv_tun_tpr
 *      tunnel TPR structure
 */
struct ppe_drv_tun_tpr {
	uint8_t tpr_index;	/* Index */
	struct kref ref;	/* Reference Counter */
	struct ppe_drv_tun_tpr_entry tpre;	/* TPR entry */
};

/*
 * Function Prototypes
 */
struct ppe_drv_tun_tpr *ppe_drv_tun_tpr_entry_exists(struct ppe_drv *p, struct ppe_drv_tun_tpr_entry *tpr);
bool ppe_drv_tun_tpr_entry_configure(struct ppe_drv_tun_tpr_entry *tpre);
bool ppe_drv_tun_tpr_entry_deref(struct ppe_drv_tun_tpr  *tun_tpr);
void ppe_drv_tun_tpr_entry_ref(struct ppe_drv_tun_tpr  *tun_tpr);
struct ppe_drv_tun_tpr *ppe_drv_tun_tpr_entry_alloc(struct ppe_drv *p);
void ppe_drv_tun_tpr_free(struct ppe_drv_tun_tpr *tun_tpr);
struct ppe_drv_tun_tpr *ppe_drv_tun_tpr_alloc(struct ppe_drv *p);
bool ppe_drv_tun_rps_prgm_prsr_deconfigure(struct ppe_drv_tun_prgm_prsr *prsr);
#endif /* _PPE_DRV_TUN_TPR_H_ */
