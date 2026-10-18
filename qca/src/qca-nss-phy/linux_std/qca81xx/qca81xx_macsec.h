/*
 * Copyright (c) 2024, Qualcomm Innovation Center, Inc. All rights reserved.
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


#ifndef _QCA81XX_PHY_MACSEC_H_
#define _QCA81XX_PHY_MACSEC_H_

#include <linux/bitfield.h>
#include <net/macsec.h>

/**********************MMD3********************************/
#define MACSEC_SYS_BASE				0xC000
#define MACSEC_SA_CONTROL_BASE			0xC0A0
#define MACSEC_TX_AN_BASE			0xC0A8
#define MACSEC_RX_AN_BASE			0xC0AC
#define MACSEC_TX_NPN_BASE			0xC100
#define MACSEC_RX_NPN_BASE			0xC300
#define MACSEC_SC_BIND_BASE			0xC500
#define MACSEC_TX_MIB_BASE			0xC700
#define MACSEC_RX_MIB_BASE			0xCA00
#define MACSEC_TX_SAK_BASE			0xD000
#define MACSEC_RX_SAK_BASE			0xD200
#define MACSEC_TX_EXTENDED_SAK_BASE		0xD400
#define MACSEC_RX_EXTENDED_SAK_BASE		0xD600
#define MACSEC_TX_SSCI_BASE			0xCD00
#define MACSEC_RX_SSCI_BASE			0xCD02
#define MACSEC_RX_KI_BASE			0xDA00
#define MACSEC_TX_KI_BASE			0xD800
#define MACSEC_TX_XPN_BASE			0xC180
#define MACSEC_RX_XPN_BASE			0xC380

/**********************MMD7********************************/
#define MACSEC_SHADOW_REGISTER			0x807f
#define MACSEC_SHADOW_DUPLEX_EN			BIT(8)
#define MACSEC_SHADOW_LEGACY_DUPLEX_EN		BIT(3)


/* system configure register */
#define MACSEC_SYS_CONFIG		(MACSEC_SYS_BASE + 0x00)
#define SYS_INCLUDED_SCI_EN			BIT(13)
#define SYS_REPLAY_PROTECT_EN			BIT(8)
#define SYS_USE_ES_EN				BIT(1)
#define SYS_USE_SCB_EN				BIT(0)

/* frame control register */
#define MACSEC_SYS_FRAME_CTRL		(MACSEC_SYS_BASE + 0x0B)
#define SYS_AES256_EN				BIT(7)
#define SYS_XPN_EN				BIT(6)
#define SYS_FRAME_PROTECT_EN			BIT(2)
#define SYS_FRAME_VALIDATE_M			GENMASK(1, 0)
		#define SYS_FRAME_VALIDATE_STRICT	0x0
		#define SYS_FRAME_VALIDATE_CHECK	0x1
		#define SYS_FRAME_VALIDATE_DIS		0x2

/* replay window for the process of replay protect. totally 32bitregister */
#define MACSEC_SYS_REPLAY_WIN_BASE	(MACSEC_SYS_BASE + 0x10)

/* packet control register */
#define MACSEC_SYS_PACKET_CTRL		(MACSEC_SYS_BASE + 0x1F)
#define SYS_SECY_LPBK_M				GENMASK(15, 14)
		#define SYS_MACSEC_EN			0x0
		#define SYS_BYPASS			0x3

/* controlled port status */
#define MACSEC_SYS_PORT_CTRL		(MACSEC_SYS_BASE + 0x20)
#define SYS_PORT_EN				BIT(15)

/* macsec sofware control register */
#define MACSEC_SOFTWARE_EN_CTRL			0xA200
#define SYS_SECY_SOFTWARE_EN_M			GENMASK(15, 14)
		#define SOFTWARE_BYPASS			0x1
		#define SOFTWARE_EN			0x3

/* SA TX/RX control registers */
#define MACSEC_TX_SA_CONTROL		(MACSEC_SA_CONTROL_BASE + 0x00)
#define MACSEC_RX_SA_CONTROL		(MACSEC_SA_CONTROL_BASE + 0x02)

/* TX Next PN register */
#define MACSEC_TX_NPN(channel)		(MACSEC_TX_NPN_BASE + channel * 4)

/* RX Next PN register */
#define MACSEC_RX_NPN(channel)		(MACSEC_RX_NPN_BASE + channel * 4)

/* SA Bind block */
/* destination address bind for tx */
#define MACSEC_SC_BIND_TXDA_BASE(channel)  \
((MACSEC_SC_BIND_BASE + channel * 0x20) + 0x00)
/* source address bind for tx */
#define MACSEC_SC_BIND_TXSA_BASE(channel)  \
((MACSEC_SC_BIND_BASE + channel * 0x20) + 0x03)
/* ethernet type bind for tx */
#define MACSEC_SC_BIND_TXETHERTYPE(channel) \
((MACSEC_SC_BIND_BASE + channel * 0x20) + 0x06)
/* vtag bind for tx */
#define MACSEC_SC_BIND_TXOUTVTAG(channel) \
((MACSEC_SC_BIND_BASE + channel * 0x20) + 0x07)
#define MACSEC_SC_BIND_TXINVTAG(channel) \
((MACSEC_SC_BIND_BASE + channel * 0x20) + 0x08)
/* sci bind for tx */
#define MACSEC_SC_BIND_TXSCI_BASE(channel) \
((MACSEC_SC_BIND_BASE + channel * 0x20) + 0x09)
/* tci bind for tx */
#define MACSEC_SC_BIND_TXTCI(channel) \
((MACSEC_SC_BIND_BASE + channel * 0x20) + 0x0d)
#define SC_BIND_TXOFFSET_M	GENMASK(13, 8)
#define SC_BIND_TXTCI_M		GENMASK(7, 0)
/* bind context for  tx */
#define MACSEC_SC_BIND_TXCTX(channel) \
((MACSEC_SC_BIND_BASE + channel * 0x20) + 0x0e)
#define SC_BIND_TX_IFBC		BIT(15)
#define SC_BIND_TXCTX_M		GENMASK(3, 0)
/* bind mask for tx */
#define MACSEC_SC_BIND_TXMASK(channel) \
((MACSEC_SC_BIND_BASE + channel * 0x20) + 0x0f)
#define SC_BIND_TX_VALID	BIT(15)
#define SC_BIND_TXMASK_M	GENMASK(7, 0)

/* destination address bind for rx */
#define MACSEC_SC_BIND_RXDA_BASE(channel) \
((MACSEC_SC_BIND_BASE + channel * 0x20) + 0x10)
/* source address bind for rx */
#define MACSEC_SC_BIND_RXSA_BASE(channel) \
((MACSEC_SC_BIND_BASE + channel * 0x20) + 0x13)
/* ethernet type bind for rx */
#define MACSEC_SC_BIND_RXETHERTYPE(channel) \
((MACSEC_SC_BIND_BASE + channel * 0x20) + 0x16)
/* vtag bind for rx */
#define MACSEC_SC_BIND_RXOUTVTAG(channel) \
((MACSEC_SC_BIND_BASE + channel * 0x20) + 0x17)
#define MACSEC_SC_BIND_RXINVTAG(channel) \
((MACSEC_SC_BIND_BASE + channel * 0x20) + 0x18)
/* sci bind for rx */
#define MACSEC_SC_BIND_RXSCI_BASE(channel) \
((MACSEC_SC_BIND_BASE + channel * 0x20) + 0x19)
/* tci bind for rx */
#define MACSEC_SC_BIND_RXTCI(channel) \
((MACSEC_SC_BIND_BASE + channel * 0x20) + 0x1D)
#define SC_BIND_RXOFFSET_M	GENMASK(13, 8)
#define SC_BIND_RXTCI_M		GENMASK(7, 0)
/* bind context for rx */
#define MACSEC_SC_BIND_RXCTX(channel) \
((MACSEC_SC_BIND_BASE + channel * 0x20) + 0x1E)
#define SC_BIND_RX_IFBC		BIT(15)
#define SC_BIND_RXCTX_M		GENMASK(3, 0)
/* bind mask for rx */
#define MACSEC_SC_BIND_RXMASK(channel) \
((MACSEC_SC_BIND_BASE + channel * 0x20) + 0x1F)
#define SC_BIND_RX_VALID	BIT(15)
#define SC_BIND_RXMASK_M	GENMASK(7, 0)


/* TX SAK block */
#define MACSEC_TX_SAK_KEY0(channel) \
((MACSEC_TX_SAK_BASE + channel * 0x10) + 0x00)
#define MACSEC_TX_SAK_KEY1(channel) \
((MACSEC_TX_SAK_BASE + channel * 0x10) + 0x01)
#define MACSEC_TX_SAK_KEY2(channel) \
((MACSEC_TX_SAK_BASE + channel * 0x10) + 0x02)
#define MACSEC_TX_SAK_KEY3(channel) \
((MACSEC_TX_SAK_BASE + channel * 0x10) + 0x03)
#define MACSEC_TX_SAK_KEY4(channel) \
((MACSEC_TX_SAK_BASE + channel * 0x10) + 0x04)
#define MACSEC_TX_SAK_KEY5(channel) \
((MACSEC_TX_SAK_BASE + channel * 0x10) + 0x05)
#define MACSEC_TX_SAK_KEY6(channel) \
((MACSEC_TX_SAK_BASE + channel * 0x10) + 0x06)
#define MACSEC_TX_SAK_KEY7(channel) \
((MACSEC_TX_SAK_BASE + channel * 0x10) + 0x07)

/* RX SAK block */
#define MACSEC_RX_SAK_KEY0(channel) \
((MACSEC_RX_SAK_BASE + channel * 0x10) + 0x00)
#define MACSEC_RX_SAK_KEY1(channel) \
((MACSEC_RX_SAK_BASE + channel * 0x10) + 0x01)
#define MACSEC_RX_SAK_KEY2(channel) \
((MACSEC_RX_SAK_BASE + channel * 0x10) + 0x02)
#define MACSEC_RX_SAK_KEY3(channel) \
((MACSEC_RX_SAK_BASE + channel * 0x10) + 0x03)
#define MACSEC_RX_SAK_KEY4(channel) \
((MACSEC_RX_SAK_BASE + channel * 0x10) + 0x04)
#define MACSEC_RX_SAK_KEY5(channel) \
((MACSEC_RX_SAK_BASE + channel * 0x10) + 0x05)
#define MACSEC_RX_SAK_KEY6(channel) \
((MACSEC_RX_SAK_BASE + channel * 0x10) + 0x06)
#define MACSEC_RX_SAK_KEY7(channel) \
((MACSEC_RX_SAK_BASE + channel * 0x10) + 0x07)

/* TX Extended SAK block */
#define MACSEC_TX_EXTENDED_SAK_KEY0(channel) \
((MACSEC_TX_EXTENDED_SAK_BASE + channel * 0x10) + 0x00)

/* RX Extended SAK block */
#define MACSEC_RX_EXTENDED_SAK_KEY0(channel) \
((MACSEC_RX_EXTENDED_SAK_BASE + channel * 0x10) + 0x00)

/* TX SSCI block */
#define MACSEC_SC_TX_SSCI0(channel) \
((MACSEC_TX_SSCI_BASE + channel * 0x4) + 0x00)

/* RX SSCI block */
#define MACSEC_SC_RX_SSCI0(channel) \
((MACSEC_RX_SSCI_BASE + channel * 0x4) + 0x00)

/* TX Key Identifier block */
#define MACSEC_TX_KI_KEY0(channel) \
((MACSEC_TX_KI_BASE + channel * 0x10) + 0x00)

/* RX Key Identifier block */
#define MACSEC_RX_KI_KEY0(channel) \
((MACSEC_RX_KI_BASE + channel * 0x10) + 0x00)

/* TX Next PN register */
#define MACSEC_TX_XPN(channel) \
(MACSEC_TX_XPN_BASE + channel * 4)

/* RX Next PN register */
#define MACSEC_RX_XPN(channel) \
(MACSEC_RX_XPN_BASE + channel * 4)

/* Tx MIB register */
#define TX_SA_PROTECTED_PKTS_BASE        (MACSEC_TX_MIB_BASE + 0)
#define TX_SA_ENCRYPTED_PKTS_BASE        (MACSEC_TX_MIB_BASE + 0x40)
#define TX_SC_PROTECTED_OCTETS_BASE      (MACSEC_TX_MIB_BASE + 0x80)
#define TX_SC_ENCRYPTED_OCTETS_BASE      (MACSEC_TX_MIB_BASE + 0xa0)
#define TX_UNTAGGED_PKTS                 (MACSEC_TX_MIB_BASE + 0xc0)
#define TX_TOO_LONG_PKTS                 (MACSEC_TX_MIB_BASE + 0xc2)
#define TX_SA_PROTECTED_PKTS_HIGH_BASE   (MACSEC_TX_MIB_BASE + 0x100)
#define TX_SA_ENCRYPTED_PKTS_HIGH_BASE   (MACSEC_TX_MIB_BASE + 0x140)
#define TX_SC_PROTECTED_OCTETS_HIGH_BASE (MACSEC_TX_MIB_BASE + 0x180)
#define TX_SC_ENCRYPTED_OCTETS_HIGH_BASE (MACSEC_TX_MIB_BASE + 0x1a0)
#define TX_UNTAGGED_PKTS_HIGH            (MACSEC_TX_MIB_BASE + 0x1c0)
#define TX_TOO_LONG_PKTS_HIGH            (MACSEC_TX_MIB_BASE + 0x1c2)

/* Rx MIB register */
#define RX_SA_UNUSED_PKTS_BASE           (MACSEC_RX_MIB_BASE + 0)
#define RX_SA_NOUSING_PKTS_BASE          (MACSEC_RX_MIB_BASE + 0x40)
#define RX_SA_NOTVALID_PKTS_BASE         (MACSEC_RX_MIB_BASE + 0x80)
#define RX_SA_INVALID_PKTS_BASE          (MACSEC_RX_MIB_BASE + 0xc0)
#define RX_SA_OK_PKTS_BASE               (MACSEC_RX_MIB_BASE + 0x100)
#define RX_SC_LATE_PKTS_BASE             (MACSEC_RX_MIB_BASE + 0x140)
#define RX_SC_DELAYED_PKTS_BASE          (MACSEC_RX_MIB_BASE + 0x160)
#define RX_SC_UNCHECKED_PKTS_BASE        (MACSEC_RX_MIB_BASE + 0x180)
#define RX_SC_VALIDATED_PKTS_BASE        (MACSEC_RX_MIB_BASE + 0x1a0)
#define RX_SC_DECRYPTED_PKTS_BASE        (MACSEC_RX_MIB_BASE + 0x1c0)
#define RX_UNTAGGED_PKTS                 (MACSEC_RX_MIB_BASE + 0x1e0)
#define RX_NO_TAG_PKTS                   (MACSEC_RX_MIB_BASE + 0x1e2)
#define RX_BAD_TAG_PKTS                  (MACSEC_RX_MIB_BASE + 0x1e4)
#define RX_UNKNOWN_SCI_PKTS              (MACSEC_RX_MIB_BASE + 0x1e6)
#define RX_NO_SCI_PKTS                   (MACSEC_RX_MIB_BASE + 0x1e8)
#define RX_OVERRUN_PKTS                  (MACSEC_RX_MIB_BASE + 0x1ea)
#define RX_SA_UNUSED_PKTS_HIGH_BASE      (MACSEC_RX_MIB_BASE + 0x400)
#define RX_SA_NOUSING_PKTS_HIGH_BASE     (MACSEC_RX_MIB_BASE + 0x440)
#define RX_SA_NOTVALID_PKTS_HIGH_BASE    (MACSEC_RX_MIB_BASE + 0x480)
#define RX_SA_INVALID_PKTS_HIGH_BASE     (MACSEC_RX_MIB_BASE + 0x4c0)
#define RX_SA_OK_PKTS_HIGH_BASE          (MACSEC_RX_MIB_BASE + 0x500)
#define RX_SC_LATE_PKTS_HIGH_BASE        (MACSEC_RX_MIB_BASE + 0x540)
#define RX_SC_DELAYED_PKTS_HIGH_BASE     (MACSEC_RX_MIB_BASE + 0x560)
#define RX_SC_UNCHECKED_PKTS_HIGH_BASE   (MACSEC_RX_MIB_BASE + 0x580)
#define RX_SC_VALIDATED_PKTS_HIGH_BASE   (MACSEC_RX_MIB_BASE + 0x5a0)
#define RX_SC_DECRYPTED_PKTS_HIGH_BASE   (MACSEC_RX_MIB_BASE + 0x5c0)
#define RX_UNTAGGED_PKTS_HIGH            (MACSEC_RX_MIB_BASE + 0x5e0)
#define RX_NO_TAG_PKTS_HIGH              (MACSEC_RX_MIB_BASE + 0x5e2)
#define RX_BAD_TAG_PKTS_HIGH             (MACSEC_RX_MIB_BASE + 0x5e4)
#define RX_UNKNOWN_SCI_PKTS_HIGH         (MACSEC_RX_MIB_BASE + 0x5e6)
#define RX_NO_SCI_PKTS_HIGH              (MACSEC_RX_MIB_BASE + 0x5e8)
#define RX_OVERRUN_PKTS_HIGH             (MACSEC_RX_MIB_BASE + 0x5ea)

#define QCA_SECY_SC_MAX_NUM		16
#define QCA_GCM_AES_128_SAK_LEN		16
#define QCA_GCM_AES_256_SAK_LEN		32
#define SECY_AN_IDX_MAX_NUM		4
#define SECY_AN_TO_SA_MAPPING(an)	(an%2)
#define AN_MASK				0x3

#define SC_BIND_MASK_DA           0x00000001
#define SC_BIND_MASK_SA           0x00000002
#define SC_BIND_MASK_ETHERTYPE    0x00000004
#define SC_BIND_MASK_OUTER_VLAN   0x00000008
#define SC_BIND_MASK_INNER_VLAN   0x00000010
#define SC_BIND_MASK_BCAST        0x00000020
#define SC_BIND_MASK_TCI          0x00000040
#define SC_BIND_MASK_SCI          0x00000080
#define SC_BIND_MASK_VALID        0x00008000


struct secy_mac_t {
	u8  addr[6];
};

struct secy_rx_sc_policy_action_t {
	u32  rx_sc_index;
	u8   decryption_offset;
};

struct secy_rx_sc_policy_rule_t {
	bool  rule_valid;
	u32   rule_mask;
	struct secy_mac_t      mac_da;
	struct secy_mac_t      mac_sa;
	u16   ethtype;
	u16   outer_vlanid;
	u16   inner_vlanid;
	u64   rx_sci;
	u8    rx_tci;
	bool  bc_flag;
	struct secy_rx_sc_policy_action_t action;
};

struct secy_tx_sc_policy_action_t {
	u32   tx_sc_index;
	u64   tx_sci;
	u8    tx_tci;
	u8    encryption_offset;
};

struct secy_tx_sc_policy_rule_t {
	bool  rule_valid;
	u32   rule_mask;
	struct secy_mac_t      mac_da;
	struct secy_mac_t      mac_sa;
	u16   ethtype;
	u16   outer_vlanid;
	u16   inner_vlanid;
	bool  bc_flag;
	struct secy_tx_sc_policy_action_t action;
};

struct secy_sak_t {
	u8 sak[16];
	u8 sak1[16];
	u32 len;
};

struct secy_sa_ki_t {
	u8 ki[16];
};

struct secy_tx_sa_mib_t {
	/* The number of integrity protected
	 * but not encrypted packets
	 */
	u64 protected_pkts;
	/* The number of integrity protected
	 * and encrypted packets
	 */
	u64 encrypted_pkts;
};

struct secy_tx_sc_mib_t {
	/* The number of plain text octets that
	 * are integrity protected but not encrypted
	 */
	u64 protected_octets;
	/* The number of plain text octets that
	 * are integrity protected and encrypted
	 */
	u64 encrypted_octets;
};

struct secy_tx_mib_t {
	/* The number of transmitted packets
	 * without the MAC security tag (SecTAG)
	 */
	u64 untagged_pkts;
	/* The number of transmitted packets discarded
	 * because the packet length is greater than the
	 * MTU of the Common Port interface
	 */
	u64 too_long;
};

struct secy_rx_sa_mib_t {
	/* For this SA which is not currently in use, the number
	 * of received packets that have been discarded, and have
	 * either the packets encrypted or the secyValidateFrames
	 * set to strict mode.
	 */
	u64 not_using_sa;
	/* For this SA which is not currently in use, the number
	 * of received, unencrypted, packets with secyValidateFrames
	 * not in the strict mode.
	 */
	u64 unused_sa;
	/* The number discarded packets with the condition that
	 * the packets are not valid and one of the following
	 * conditions are true: either secyValidateFrames in
	 * strict mode or the packets encrypted.
	 */
	u64 not_valid_pkts;
	/* The number of packets with the condition that the packets
	 * are not valid and secyValidateFrames is in check mode.
	 */
	u64 invalid_pkts;
	/* The number of octets of plaintext recovered from received
	 * packets that were integrity protected and encrypted.
	 */
	u64 ok_pkts;
};

struct secy_rx_sc_mib_t {
	/* The number of received packets that have been discarded
	 * with the condition : secyReplayProtect is equal to true
	 * and the PN of the packet is lower than the lower bound
	 * replay check PN.
	 */
	u64 late_pkts;
	/* The number of packets with the condition that the PN
	 * of the packets is lower than the lower bound replay
	 * protection PN.
	 */
	u64 delayed_pkts;
	/* The number of packets with the following condition:
	 * -secyValidateFrames is disabled or
	 * -secyValidateFrames is not disabled and the packet
	 *  is not encrypted and the integrity check has failed or
	 * -secyValidateFrames is not disable and the packet
	 *  is encrypted and integrity check has failed.
	 */
	u64 unchecked_pkts;
	/* The number of octets of plaintext recovered from
	 * received packets that were integrity protected
	 * but not encrypted.
	 */
	u64 validated_octets;
	/* The number of octets of plaintext recovered from
	 * received packets that were integrity protected
	 * and encrypted.
	 */
	u64 decrypted_octets;
};

struct secy_rx_mib_t {
	/* The number of received packets discarded
	 * without the MAC security tag (SecTAG) with
	 * secyValidateFrames which is in the strict mode.
	 */
	u64 notag_pkts;
	/* The number of received packets without
	 * the MAC security tag (SecTAG) with
	 * secyValidateFrames which is not in the strict mode.
	 */
	u64 untagged_pkts;
	/* The number of received packets discarded with
	 * an invalid SecTAG or a zero value PN or an invalid ICV.
	 */
	u64 bad_tag_pkts;
	/* The number of received packets discarded with
	 * unknown SCI information with the condition :
	 * secyValidateFrames is in the strict mode or
	 * the C bit in the SecTAG is set.
	 */
	u64 no_sci_pkts;
	/* The number of received packets with unknown SCI
	 * with the condition : secyValidateFrames is not in
	 * the strict mode and the C bit in the SecTAG is not set.
	 */
	u64 unknown_sci_pkts;
	/* The number of packets discarded because
	 * the number of received packets exceeded the
	 * cryptographic performance capabilities
	 */
	u64 overrun_packets;
};

struct qca_macsec_secy_sc_map_data {
	u32 hw_sc_idx;
	const struct macsec_secy  *sw_secy;
	const struct macsec_rx_sc *rx_sc;
};

struct qca_macsec_cfg_t {
	unsigned long txsc_idx_bits;
	unsigned long rxsc_idx_bits;
	struct qca_macsec_secy_sc_map_data secy_txsc[QCA_SECY_SC_MAX_NUM];
	struct qca_macsec_secy_sc_map_data secy_rxsc[QCA_SECY_SC_MAX_NUM];
};


int qca81xx_macsec_init(struct phy_device *phydev);

#endif /* _QCA81XX_PHY_MACSEC_H_ */

