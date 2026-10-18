/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: ISC
 */

#ifndef __NSS_DP_UDP_ST_H__
#define __NSS_DP_UDP_ST_H__

/*
 * nss_dp_udp_st_rx_cb_t
 *      Udp st rx handler callback typedef
 */
typedef void (*nss_dp_udp_st_rx_cb_t) (struct sk_buff *skb);

/**
 * nss_dp_udp_st_rx_register_cb
 *      Register handler for udp st rx processing.
 *
 * @datatypes
 * nss_dp_udp_st_rx_cb_t
 *
 * @param[in] nss_dp_udp_st_rx_cb_t Pointer to udp st rx handler.
 *
 * @return
 * void.
 */
void nss_dp_udp_st_rx_register_cb(nss_dp_udp_st_rx_cb_t cb);

#endif  /** __NSS_DP_UDP_ST_H__ */
