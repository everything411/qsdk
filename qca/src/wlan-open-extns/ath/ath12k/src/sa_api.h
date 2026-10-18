/* SPDX-License-Identifier: BSD-3-Clause-Clear */
/* Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.*/

#ifndef ATH12K_SA_API_H
#define ATH12K_SA_API_H

#include "../vendor.h"
#include "sa_api_defs.h"
#include "sa_core.h"

/**
 * ath12k_vendor_get_wiphy_sa_configs - Get Smart Antenna configuration
 * @wiphy: wiphy on which to apply the configuration
 * @params: vendor generic params carrying cfg id, value and length
 * @value: ouput pointer to carry the configuration value
 *
 * Fetches Smart Antenna configuration from userspace to the core. Depending on
 * the parameter, this may query radio-wide settings or peer-specific settings
 * and gets configuration using smart antenna algorithm hooks.
 *
 * Return: 0 on success, negative errno on failure.
 */
int ath12k_vendor_get_wiphy_sa_configs(struct wiphy *wiphy,
				       struct ath12k_wifi_generic_params *params,
				       u32 *value);

/**
 * ath12k_vendor_set_wiphy_sa_configs - Set Smart Antenna configuration
 * @wiphy: wiphy on which to apply the configuration
 * @params: vendor generic params carrying cfg id, value, length and flags
 *
 * Passes Smart Antenna configuration from userspace to the core. Depending on
 * the parameter, this may apply radio-wide settings or peer-specific settings
 * and can trigger corresponding WMI commands to firmware and/or callbacks into
 * the Smart Antenna algorithm hooks.
 *
 * Return: 0 on success, negative errno on failure.
 */
int ath12k_vendor_set_wiphy_sa_configs(struct wiphy *wiphy,
				       struct ath12k_wifi_generic_params *params);

/**
 * ath12k_smart_ant_is_enable - Query whether Smart Antenna is enabled
 * @ab: ath12k base pointer
 *
 * Checks the cached enable state for Smart Antenna feature based on host cfg
 * and firmware capability detection performed during wmi init.
 *
 * Return: true if Smart Antenna is enabled, false otherwise.
 */
bool ath12k_smart_ant_is_enable(struct ath12k_base *ab);

/**
 * ath12k_smart_ant_enable - Evaluate and set Smart Antenna enable state
 * @ab: ath12k base pointer
 *
 * Evaluates host configuration flags and firmware advertised services to decide
 * whether Smart Antenna should be enabled. Updates the internal base extension
 * state accordingly and emits an informational log.
 *
 * Return: true if Smart Antenna gets enabled, false otherwise.
 */
bool ath12k_smart_ant_enable(struct ath12k_base *ab);

/**
 * ath12k_register_smart_ant_ops - Register Smart Antenna algorithm hooks
 * @ops: algorithm operations to register
 *
 * Registers the implementation of Smart Antenna algorithm callbacks used by
 * the core for per-radio and per-peer operations such as antenna selection and
 * training planning. Typically called by a module providing the algorithm.
 *
 * Return: 0 on success, negative errno on failure.
 */
int ath12k_register_smart_ant_ops(struct ath12k_smart_ant_ops *ops);

/**
 * ath12k_unregister_smart_ant_ops - Unregister Smart Antenna algorithm hooks
 *
 * Unregisters the Smart Antenna algorithm when no radios are initialized
 * under Smart Antenna (based on an internal init counter). Intended to be
 * called by the provider module at unload time.
 *
 * Return: 0 on success, negative errno on failure.
 */
int ath12k_unregister_smart_ant_ops(void);

/**
 * ath12k_wmi_peer_ratecode_list_event_handler - Handle WMI RC list event
 * @ab: ath12k base context
 * @skb: event skb from firmware containing peer ratecode list
 *
 * Processes the peer ratecode list event from firmware and forwards it to the
 * Smart Antenna core handler to update per-peer ratecode state.
 */
static inline
void ath12k_wmi_peer_ratecode_list_event_handler(struct ath12k_base *ab,
						 struct sk_buff *skb)
{
	sa_core_wmi_peer_rc_list_ev_handler(ab, skb);
}

/**
 * ath12k_smart_ant_api_peer_disconnect - Notify SA of peer disconnection
 * @arsta: ath12k link sta corresponding to the peer
 *
 * Notifies Smart Antenna core that a peer has disconnected so that any
 * per-peer state can be cleaned up. This wraps
 * sa_core_peer_disconnect_notify().
 */
static inline
void ath12k_smart_ant_api_peer_disconnect(struct ath12k_link_sta *arsta)
{
	sa_core_peer_disconnect_notify(arsta);
}

/**
 * ath12k_smart_ant_api_start - Start Smart Antenna per-radio/per-vif context
 * @ar: ath12k radio context
 * @arvif: ath12k link vif for which SA is started
 * @init_flags: bitmask controlling which parts to initialize
 *
 * Initializes Smart Antenna core for the given radio and VIF according to
 * the requested initialization flags. This wraps sa_core_init().
 *
 * Return: 0 on success, negative errno on failure.
 */
static inline
int ath12k_smart_ant_api_start(struct ath12k *ar, struct ath12k_link_vif *arvif,
			       u8 init_flags)
{
	return sa_core_init(ar, arvif, init_flags);
}

/**
 * ath12k_smart_ant_api_stop - Stop Smart Antenna for a radio/VIF
 * @ar: ath12k radio context
 * @init_flags: bitmask controlling which parts to deinitialize
 *
 * Deinitializes Smart Antenna core for the given radio according to the
 * provided flags. This wraps sa_core_deinit().
 *
 * Return: 0 on success, negative errno on failure.
 */
static inline int ath12k_smart_ant_api_stop(struct ath12k *ar, u8 init_flags)
{
	return sa_core_deinit(ar, init_flags);
}

/**
 * ath12k_smart_ant_api_channel_change - Reconfigure Smart Antenna after AP channel change
 * @ar: ath12k radio context on which the channel changed
 * @re_init: true during channel change to do partial init.
 *           false during SSR handling to do full init with previous pdev configs
 *
 * Handles an AP channel change by tearing down and re-initialising the Smart
 * Antenna core in reconfiguration mode (SA_NEW_RECONFIG), then re-notifying
 * the algorithm module of every peer that is currently associated on the radio.
 * This allows the algorithm to update per-peer state (antenna selection,
 * training parameters) that depends on the new channel or bandwidth.
 *
 * Internally selects the first suitable AP or STA arvif on @ar for the
 * SA core reinitialisation.
 *
 * Must be called from process context with the wiphy lock held.
 *
 * Return: 0 on success, negative errno on failure.
 */
int ath12k_smart_ant_api_channel_change(struct ath12k *ar, bool re_init);

/**
 * ath12k_smart_ant_api_recovery_handler - Handle SSR event
 * @ar: ath12k radio context on which the SSR post action triggered
 *
 * Handle SSR by enabling Smart Antenna feature by doing full init sequence
 * with previous pdev configs. Doesn't need to handle STA connection explicitly
 * as it all STAs do reconnection.
 *
 * Return: none with internal error reporting
 */
static inline void ath12k_smart_ant_api_recovery_handler(struct ath12k *ar)
{
	ath12k_smart_ant_api_channel_change(ar, false);
}

#endif /* ATH12K_SA_API_H */
