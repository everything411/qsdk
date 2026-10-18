/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: ISC
 */

#ifdef CONFIG_IPQ_PON
#ifndef __NSS_DP_GEM_H__
#define __NSS_DP_GEM_H__

/*
 * nss_dp_gem_tx_info
 *	GEM Tx info structure
 */
struct nss_dp_gem_tx_info {
	struct net_device * src_dev;		/**< Source info. */
	uint8_t service_code;		/**< Service code. */
	bool src_dev_valid;		/**< Source info valid. */
};

/*
 * nss_dp_gem_rx_cb_t
 *	GEM Rx handler callback typedef
 */
typedef bool (*nss_dp_gem_rx_cb_t)(void *app_data, struct sk_buff *skb);

/*
 * nss_dp_gem_tx_cb_t
 *	GEM Tx handler callback typedef
 */
typedef bool (*nss_dp_gem_tx_cb_t)(void *app_data, struct sk_buff *skb, struct nss_dp_gem_tx_info *gem_txi);

/**
 * nss_dp_gem_rx_register_cb
 *	Register handler for GEM rx processing.
 *
 * @datatypes
 * nss_dp_gem_rx_cb_t
 *
 * @param[in] app_data Pointer to app_data.
 * @param[in] cb Pointer to GEM rx handler.
 *
 * @return
 * True or false.
 */
bool nss_dp_gem_rx_register_cb(void *app_data, nss_dp_gem_rx_cb_t cb);

/**
 * nss_dp_gem_rx_unregister_cb
 *	Unregister GEM handler for GEM rx processing.
 *
 * @datatypes
 * None.
 *
 * @return
 * None.
 */
void nss_dp_gem_rx_unregister_cb(void);

/**
 * nss_dp_gem_tx_register_cb
 *	Register handler for GEM tx processing.
 *
 * @datatypes
 * nss_dp_gem_tx_cb_t
 *
 * @param[in] app_data Pointer to app_data.
 * @param[in] cb Pointer to GEM tx handler.
 *
 * @return
 * True or false.
 */
bool nss_dp_gem_tx_register_cb(void *app_data, nss_dp_gem_tx_cb_t cb);

/**
 * nss_dp_gem_tx_unregister_cb
 *	Unregister GEM handler for GEM tx processing.
 *
 * @datatypes
 * None.
 *
 * @return
 * None.
 */
void nss_dp_gem_tx_unregister_cb(void);

#define NSS_DP_PON_TCONT_BITMAP_ALL 0xffffffff	/* Select all T-CONT slots for drainout */

/**
 * nss_dp_pon_drainout_cb_t
 *	PON TCONT drainout callback typedef for UMAC reset.
 *
 *	Registered by the PON driver pointing to
 *	pon_tcf_bwmrpt_drainout_tcont_by_bitmap.
 *
 * @param[in] tcont_bitmap   Bitmask of T-CONT internal IDs to drain.
 * @param[in] context_str    Debug label string for logging.
 * @return 0 on success, negative on error.
 */
typedef int (*nss_dp_pon_drainout_cb_t)(unsigned int tcont_bitmap, const char *context_str);

/**
 * nss_dp_pon_drainout_register_cb
 *	Register PON TCONT drainout callback.
 *
 * @param[in] cb	Callback function pointer (NULL to unregister).
 */
void nss_dp_pon_drainout_register_cb(nss_dp_pon_drainout_cb_t cb);

/**
 * nss_dp_pon_drainout_unregister_cb
 *	Unregister PON TCONT drainout callback.
 */
void nss_dp_pon_drainout_unregister_cb(void);

/**
 * nss_dp_pon_drainout
 *	PON TCONT drainout for UMAC reset.
 *
 *	Calls pon_tcf_bwmrpt_drainout_tcont_by_bitmap() to flush TCF SRAM
 *	and zero PPE TCONT credits for the specified T-CONTs.
 *	Safe to call from process context.
 *
 * @param[in] tcont_bitmap   Bitmask of T-CONT internal IDs to drain.
 * @param[in] context_str    Debug label string for logging.
 * @return 0 on success or callback not registered, negative on error.
 */
int nss_dp_pon_drainout(unsigned int tcont_bitmap, const char *context_str);

#endif /* __NSS_DP_GEM_H__ */
#endif
