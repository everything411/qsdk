/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: ISC
 */

#ifndef __NSS_UDP_ST_PPE_H
#define __NSS_UDP_ST_PPE_H

#include <ppe_vp_public.h>

struct ppe_drv_policer_acl;

#define NSS_UDP_ST_PPE_MAX_FLOWS	16	/* Maximum flows supported by hardware offload */

/*
 * nss_udp_st_ppe_dir
 *	enum to check tx/rx direction
 */
typedef enum nss_udp_st_ppe_dir {
	NSS_UDP_ST_PPE_TX_DIR,
	NSS_UDP_ST_PPE_RX_DIR,
} nss_udp_st_ppe_dir_t;

/*
 * nss_udp_st_ppe_ctx
 *	PPE context for UDP speedtest hardware offload.
 */
typedef struct nss_udp_st_ppe_ctx {
	struct net_device *vp_dev;		/* VP netdev */
	ppe_vp_num_t vp_num;			/* VP number */
	struct ppe_drv_policer_acl *policer_acl_ctx;	/* ACL policer context */
	uint16_t policer_rule_id;		/* Policer ID */
} nss_udp_st_ppe_ctx_t;

bool nss_udp_st_ppe_vp_alloc(uint8_t core_mask);
void nss_udp_st_ppe_vp_free(void);
void nss_udp_st_ppe_policer_init(void);
void nss_udp_st_ppe_policer_detach(void);
void nss_udp_st_ppe_policer_destroy(void);
ppe_vp_num_t nss_udp_st_ppe_vp_num_get(void);
struct net_device *nss_udp_st_ppe_vp_dev_get(void);
bool nss_udp_st_ppe_is_gem_port(struct net_device *dev);

int nss_udp_st_ppe_create_flows(nss_udp_st_ppe_dir_t dir);
void nss_udp_st_destroy_ppe_flow(struct nss_udp_st_rules *rules);

bool nss_udp_st_ppe_throughput_timer_start(void);
void nss_udp_st_ppe_throughput_timer_stop(void);
void nss_udp_st_ppe_reset_policer_stats(void);

#endif  /*NSS_UDP_ST_PPE_H*/
