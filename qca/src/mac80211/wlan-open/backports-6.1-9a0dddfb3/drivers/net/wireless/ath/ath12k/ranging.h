/* SPDX-License-Identifier: BSD-3-Clause-Clear */
/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 */

#ifndef ATH12K_RANGING_H
#define ATH12K_RANGING_H

/* RTT responder role bits */
#define ATH12K_RTT_11MC_RESPONDER_ROLE		BIT(0)
#define ATH12K_RTT_11AZ_NTB_RESPONDER_ROLE	BIT(1)
#define ATH12K_RTT_11AZ_TB_RESPONDER_ROLE	BIT(2)
#define ATH12K_RTT_RESPONDER_ROLE_MASK		(ATH12K_RTT_11MC_RESPONDER_ROLE | \
						 ATH12K_RTT_11AZ_NTB_RESPONDER_ROLE | \
						 ATH12K_RTT_11AZ_TB_RESPONDER_ROLE)

#define ATH12K_MAX_PASN_PEERS_PER_VAP	15
#define ATH12K_PASN_PEER_TYPE_UNSECURE	0
#define ATH12K_PASN_PEER_TYPE_SECURE	1
#define ATH12K_PASN_PEER_DELETE_NORMAL	0
#define ATH12K_PASN_PEER_DELETE_ALREADY_DELETED	BIT(0)
#define ATH12K_PASN_PEER_DELETE_FLUSH_KEYS	BIT(1)

/* PASN peer state flags (ATH12K_PASN_F_*) and WMI RTT PASN control bits */
#define ATH12K_WMI_RTT_PASN_LTF_KEYSEED_REQUIRED	BIT(1)
#define ATH12K_PASN_F_FW_CREATED		BIT(0)
#define ATH12K_PASN_F_AUTH_SUCCESS		BIT(1)
#define ATH12K_PASN_F_AUTH_STATUS_SENT		BIT(2)
#define ATH12K_PASN_F_SECURE_CTX		BIT(3)
#define ATH12K_PASN_F_LTF_KEYSEED		BIT(4)
#define ATH12K_PASN_F_FW_CREATE_PENDING		BIT(5)
#define ATH12K_WMI_RTT_PASN_SECURITY_MODE_MASK		0x3
#define ATH12K_WMI_RTT_PASN_SECURITY_MODE_NONE		0x0
#define ATH12K_WMI_RTT_PASN_SECURITY_MODE_MAC_SEC	0x1
#define ATH12K_WMI_RTT_PASN_SECURITY_MODE_MAC_PHY_SEC	0x2

/* AP iftype extended capability byte index and bits for 11az ranging */
#define ATH12K_EXT_CAPA_11AZ_RANGING_IDX		11
#define ATH12K_EXT_CAPA12_NTB_RANGING_RESPONDER	BIT(2)
#define ATH12K_EXT_CAPA12_TB_RANGING_RESPONDER		BIT(3)

struct ath12k;
struct ath12k_base;
struct ath12k_hw;

u32 ath12k_supported_rtt_responder_roles(struct ath12k_base *ab);
int ath12k_mac_setup_iftype_11az_ranging(struct ath12k_hw *ah);
void ath12k_mac_cleanup_iftype_11az_ranging(struct ath12k_hw *ah);
void ath12k_mac_set_ranging_ext_features(struct wiphy *wiphy, struct ath12k *ar);
void ath12k_rtt_reconfig_responder_role(struct ath12k *ar);

struct ath12k_rtt_pasn_peer;
struct ath12k_link_vif;

void ath12k_rtt_init_link_vif(struct ath12k_link_vif *arvif);
void ath12k_rtt_deinit_link_vif(struct ath12k_link_vif *arvif);
void ath12k_pasn_fw_peer_create_work(struct wiphy *wiphy,
				     struct wiphy_work *work);
int ath12k_pasn_fw_peer_create(struct ath12k_link_vif *arvif,
			       const u8 *peer_addr);
int ath12k_pasn_fw_peer_delete(struct ath12k_link_vif *arvif,
			       const u8 *peer_addr, bool skip_peer_del);
struct ath12k_rtt_pasn_peer *
ath12k_pasn_peer_find(struct ath12k_link_vif *arvif, const u8 *peer_addr);
u8 ath12k_pasn_peer_update_flags(struct ath12k_link_vif *arvif,
				 const u8 *peer_addr,
				 u8 set_mask, u8 clr_mask);
bool ath12k_pasn_peer_is_fw_created(struct ath12k_link_vif *arvif,
				    const u8 *peer_addr);
void ath12k_pasn_peer_delete(struct ath12k_link_vif *arvif,
			     const u8 *peer_addr);
void ath12k_pasn_peer_set_auth_status(struct ath12k_link_vif *arvif,
				      const u8 *src_addr, const u8 *peer_addr,
				      bool auth_success);
bool ath12k_pasn_peer_auth_status_sent(struct ath12k_link_vif *arvif,
				       const u8 *peer_addr);
void ath12k_pasn_peer_set_auth_status_sent(struct ath12k_link_vif *arvif,
					   const u8 *peer_addr);
void ath12k_pasn_peers_cleanup(struct ath12k_link_vif *arvif);
int ath12k_pasn_peer_create_or_update(struct ath12k_link_vif *arvif,
				      const u8 *src_addr, const u8 *peer_addr,
				      bool ltf_keyseed_required, u8 security_mode);

void ath12k_pasn_peer_set_secure_ctx(struct ath12k_link_vif *arvif,
				     const u8 *src_addr, const u8 *peer_addr,
				     bool installed);
void ath12k_pasn_peer_set_ltf_keyseed(struct ath12k_link_vif *arvif,
				      const u8 *peer_addr, bool installed);

#endif /* ATH12K_RANGING_H */
