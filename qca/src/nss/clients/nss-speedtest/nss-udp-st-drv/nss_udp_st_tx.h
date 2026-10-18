/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: ISC
 */

#ifndef __NSS_UDP_ST_TX_H
#define __NSS_UDP_ST_TX_H

#define NSS_UDP_ST_TX_DEFAULT_TIMEOUT 50	/* Default Tx test duration*/
#define NSS_UDP_ST_TX_DEFAULT_TIMER_FREQ 100	/* To caluculate pkt per 10 ms */
#define NSS_UDP_ST_TX_MAX_TIMER_FREQ 2000	/* Max supported frequency (500 us) */
#define NSS_UDP_ST_TX_DEFAULT_MAX_NUM_PKT 3600	/* Default max packets generated per rule per timer tick */
#define NSS_UDP_ST_MIN_HEADROOM 32		/* Min headroom needed */
#define NSS_UDP_ST_MIN_TAILROOM 32		/* Min tailroom needed */
#define NSS_UDP_ST_PROCESS_NAME_SZ 8		/* Size of buffer used to store process name */
#define NSS_UDP_ST_TUNNEL_ID_CONSTANT 64	/* Constant used to generate tunnel id */
#define NSS_UDP_ST_HW_OFFLOAD_PTR_ALIGN 0x20    /* Aligning skb to 32 bit for Hardware offload */

#ifdef NSS_UDP_ST_DRV_VP_ENABLE
void nss_udp_st_tun_destroy(struct net_device *dev);
#endif

bool nss_udp_st_tx_rate_change(uint32_t rate);

bool nss_udp_st_validate_rate(struct net_device *dev, uint32_t rate);

struct net_device *nss_udp_st_get_xmit_dev(void);

int nss_udp_st_pppoe_iface_config(struct net_device *dev);

bool nss_udp_st_tx_valid(void);

bool nss_udp_st_tx(void);

#ifdef NSS_UDP_ST_DRV_HW_OFFLOAD_ENABLE
bool nss_udp_st_tx_hw_offload(void);
#endif

void nss_udp_st_hrtimer_cleanup(void);

#endif /*NSS_UDP_ST_TX_H*/
