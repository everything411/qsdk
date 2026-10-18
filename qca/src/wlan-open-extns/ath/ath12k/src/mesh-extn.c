// SPDX-License-Identifier: BSD-3-Clause-Clear
/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 */

#include <linux/skbuff.h>
#include <linux/module.h>
#include <net/netlink.h>
#include <net/mac80211.h>
#include <net/genetlink.h>
#include <net/cfg80211.h>
#include <linux/etherdevice.h>
#include "../core.h"
#include "ath12k_cmn_extn.h"
#include "vendor_extn.h"
#include "../debug.h"
#include "../mac.h"
#include "esp_extn.h"
#include "ini.h"
#include "../cmn_defs.h"
#ifdef CPTCFG_QCN_EXTN_MESH_SUPPORT
#include "if_meta_hdr.h"
#include "mesh_util.h"
#endif
#include "../wifi7/dp_tx.h"
#include "../../../../../../net/mac80211/qcn_extns/cmn_extn.h"
#include "../core.h"
#include "../dp_htt.h"
#include "../dp_stats.h"
#include "../wifi7/hal.h"

u_int32_t ath12k_dp_mesh_rx_status_dump(struct ath12k_base *ab, struct mesh_recv_hdr_s *rs1)
{
	u_int32_t rate1 = rs1->rs_ratephy1;
	u_int32_t rate2 = rs1->rs_ratephy2 & 0xFFFFFF;
	u_int32_t rate1_1 = ((rate1 & 0xFFFFFF0) >> 4);
	u8 buf1[192] = {0}, *buf = (char *)&buf1;
	u32 buf_len = sizeof(buf);
	u32 len = 0;
	int i;

	if (!(rs1->rs_flags & IEEE80211_RX_FIRST_MSDU))
		return 0;

	pr_info("%s: rs_flags=0x%x ", __func__, rs1->rs_flags);
	pr_info("%s: frame is decrypted=0x%x keyix %d", __func__,
			(rs1->rs_flags & MESH_RX_DECRYPTED) ? 1 : 0, rs1->rs_keyix);

	/*Below fields only valid when skb->cb has enough space to store them*/
	pr_info("%s: rs_rssi=%d ", __func__, rs1->rs_rssi);
	pr_info("%s: rs_snr=%d ",  __func__, rs1->rs_snr);
	pr_info("%s: rs_ratephy1=0x%x ", __func__, rs1->rs_ratephy1);
	pr_info("%s: rs_ratephy2=0x%x ", __func__, rs1->rs_ratephy2);
	pr_info("%s: rs_ratephy3=0x%x ", __func__, rs1->rs_ratephy3);

	pr_info("%s: rs_band=%d", __func__, rs1->rs_band);
	pr_info("%s: rs_channel=%d", __func__,  rs1->rs_channel);

	for (i = 0; i < 32 ; i++)
		len += scnprintf(buf + len, buf_len - len, "%x", rs1->rs_decryptkey[i]);

	pr_info("%s: rs_key=0x%s\n", __func__, buf);

	if ((rs1->rs_flags & MESH_RXHDR_VER) == MESH_RXHDR_VER1) {
		pr_info("pkt type %d ", (rs1->rs_ratephy1 >> 16) & 0xFF);
		pr_info("mcs %d ", rs1->rs_ratephy1 & 0xFF);
		pr_info("nss %d ", (rs1->rs_ratephy1 >> 8) & 0xFF);
		pr_info("bw %d", (rs1->rs_ratephy1 >> 24) & 0xFF);
	} else {
		switch (rate1 & 0xF) { //preamble
		case 0: //CCK
		{
			switch (rate1_1)  { //l_sig_rate
			case 0x1:  //long 1M
				pr_info("CCK 1 Mbps long preamble");
				break;
			case 0x2: //long 2M
				pr_info("CCK 2 Mbps long preamble");
				break;
			case 0x3: //long 5.5M
				pr_info("CCK 5.5 Mbps long preamble");
				break;
			case 0x4: //long 11M
				pr_info("CCK 11 Mbps long preamble");
				break;
			case 0x5: //short 2M
				pr_info("CCK 2 Mbps short preamble");
				break;
			case 0x6: //short 5.5M
				pr_info("CCK 5.5 Mbps short preamble");
				break;
			case 0x7: //short 11M
				pr_info("CCK 11 Mbps short preamble");
				break;
			}
		}
		break;
		case 1: //OFDM
		{
			switch (rate1_1) { //l_sig_rate
			case 0x8:
				pr_info("OFDM 48 Mbps");
				break;

			case 0x9:
				pr_info("OFDM 24 Mbps");
				break;
			case 0xa:
				pr_info("OFDM 12 Mbps");
				break;

			case 0xb:
				pr_info("OFDM 6 Mbps");
				break;

			case 0xc:
				pr_info("OFDM 54 Mbps");
				break;

			case 0xd:
				pr_info("OFDM 36 Mbps");
				break;

			case 0xe:
				pr_info("OFDM 18 Mbps");
				break;

			case 0xf:
				pr_info("OFDM 9 Mbps");
				break;
			}
		}
		break;
		case 2:
		{
			if (rate1_1 & 0x80) //HT40
				pr_info("HT40 MCS%c", '0' + (rate1_1 & 0x1f));
			else // HT20
				pr_info("HT20 MCS%c", '0' + (rate1_1 & 0x1f));
		}
		break;
		case 3:
			switch (rate1_1 & 0x3) {
			case 0x0: // VHT20
				pr_info("VHT20 NSS%c MCS%c", '1' + ((rate1_1 >> 10) & 0x3),
						'0' + ((rate2 >> 4) & 0xf));

				break;

			case 0x1: // VHT40
				pr_info("VHT40 NSS%c MCS%c", '1' + ((rate1_1 >> 10) & 0x3),
						'0' + ((rate2 >> 4) & 0xf));

				break;

			case 0x2: // VHT80
				pr_info("VHT80 NSS%c MCS%c", '1' + ((rate1_1 >> 10) & 0x3),
							'0' + ((rate2 >> 4) & 0xf));

				break;
			}
			break;
		}
	}
	return 0;
}
EXPORT_SYMBOL(ath12k_dp_mesh_rx_status_dump);

bool ath12k_dp_mesh_rx_filter_mesh_packets(struct ath12k_base *ab,
					   struct ath12k_dp_peer *peer,
					   struct hal_rx_spd_data *rx_desc_data)
{
	struct ath12k_vif *ahvif = NULL;
	u32 rx_filter;

	ahvif = ath12k_vif_to_ahvif(peer->vif);
	rx_filter = ahvif->dp_vif.dp_extn.rx_filter;

	if (unlikely(rx_filter)) {
		if (rx_filter & MESH_FILTER_OUT_FROMDS)
			if (rx_desc_data->rx_msdu_info.fr_ds)
				return true;

		if (rx_filter & MESH_FILTER_OUT_TODS)
			if (rx_desc_data->rx_msdu_info.to_ds)
				return true;

		if (rx_filter & MESH_FILTER_OUT_NODS)
			if (!rx_desc_data->rx_msdu_info.fr_ds  &&
			    !rx_desc_data->rx_msdu_info.to_ds)
				return true;

		if (rx_filter & MESH_FILTER_OUT_RA) {
			if (rx_desc_data->rx_msdu_info.to_ds) {
				if (!memcmp(rx_desc_data->ad1,
					    ahvif->vif->addr,
					    MAC_ADDR_SIZE))
					return true;
			}
		}

		if (rx_filter & MESH_FILTER_OUT_TA) {
			if (rx_desc_data->rx_msdu_info.fr_ds) {
				if (!memcmp(rx_desc_data->ad2,
					    ahvif->vif->addr,
					    MAC_ADDR_SIZE))
					return true;
			}
		}
	}

	return false;
}
EXPORT_SYMBOL(ath12k_dp_mesh_rx_filter_mesh_packets);

/*
 * ath12k_dp_rx_fill_mesh_metadata: Process hal_rx_data and update mesh_recv_hdr_s.
 *
 * ath12k_dp_rx_fill_mesh_metadata() updates the mesh_recv_hdr_s information for the frame
 * and assigns it within the skb_cb->mhdr. This information shall be prepended before the
 * skb data before the frame passes through the mac80211 layer. skb->data would be adjusted
 * point to the actual data even though mesh_recv_hdr_s is populated. skb_push would be done
 * right before the delivery from the mac80211 layer to make skb->data point to the
 * mesh_recv_hdr_s. This allows the frame to be processed through the mac80211 easily
 * without much changes in the mac80211 layer.
 *
 */
void ath12k_dp_rx_fill_mesh_metadata(struct ath12k_pdev_dp *dp_pdev,
				     struct ath12k_dp_peer *peer,
				     struct sk_buff *skb,
				     struct hal_rx_spd_data *rx_desc)
{
	struct mesh_recv_hdr_s *rx_info;
	struct ath12k_base *ab = dp_pdev->dp->ab;
	u32 center_chan_freq;
	u8 primary_chan_num;
	u32 rate_mcs;
	u32 pkt_type;
	u32 nss;
	u32 bw;
	u8 klen = 0;
	int link_id;
	const u8 *peer_mac;
	bool pairwise;
	struct ath12k_dp_link_peer *link_peer = NULL;
	struct ath12k_vif *ahvif = NULL;
	int ret;

	ahvif = ath12k_vif_to_ahvif(peer->vif);

	link_peer = ath12k_dp_link_peer_find_by_peerid_index(dp_pdev->dp,
							     dp_pdev,
							     peer->peer_id);

	link_id = link_peer->link_id;

	/* fill recv mesh stats */
	rx_info = (struct mesh_recv_hdr_s *)(skb->data -
					     sizeof(struct mesh_recv_hdr_s));

	rx_info->rs_flags = MESH_RXHDR_VER1;
	if (rx_desc->rx_msdu_info.first_msdu)
		rx_info->rs_flags |= MESH_RX_FIRST_MSDU;

	if (rx_desc->rx_msdu_info.last_msdu)
		rx_info->rs_flags |= MESH_RX_LAST_MSDU;

	rx_info->rs_snr = link_peer->peer_stats.dp_mon_stats.avg_snr;

	if (rx_desc->tlv_info.is_decrypted &&
	    rx_desc->tlv_info.encrypt_info_valid &&
	    rx_desc->tlv_info.mpdu_frame_control_valid &&
	    rx_desc->tlv_info.encrypted) {
		rx_info->rs_flags |= MESH_RX_DECRYPTED;
		rx_info->rs_keyix = rx_desc->key_id_octate;
		pairwise = !rx_desc->rx_msdu_info.da_is_mcbc;
		peer_mac = pairwise ? link_peer->addr:NULL;

		ret = ieee80211_extn_get_key_material(ahvif->vif,
						link_id,
						peer_mac,
						pairwise,
						rx_info->rs_keyix,
						rx_info->rs_decryptkey,
						sizeof(rx_info->rs_decryptkey),
						&klen);
		if (ret)
			DP_PEER_MISC_STATS_INC(peer, mmesh_stat,
					       rxkey_lookp_up_fail, 0, 1);
		else
			DP_PEER_MISC_STATS_INC(peer, mmesh_stat,
					       rxkey_lookp_up_succ, 0, 1);
	}

	rx_info->rs_rssi = rx_info->rs_snr + ATH12K_DEFAULT_NOISE_FLOOR;

	primary_chan_num =  rx_desc->tlv_info.freq;
	center_chan_freq = rx_desc->tlv_info.freq  >> 16;

	if (center_chan_freq < ATH12K_MIN_5GHZ_FREQ)
		rx_info->rs_band = NL80211_BAND_2GHZ;
	else if (center_chan_freq < ATH12K_MIN_6GHZ_FREQ)
		rx_info->rs_band = NL80211_BAND_5GHZ;
	else
		rx_info->rs_band = NL80211_BAND_6GHZ;

	rx_info->rs_channel = primary_chan_num;
	pkt_type =  rx_desc->tlv_info.pkt_type;
	rate_mcs = rx_desc->tlv_info.rate_mcs;
	bw = rx_desc->tlv_info.bw;
	nss = rx_desc->tlv_info.nss;

	/*
	 * The MCS index does not start with 0 when NSS>1 in HT mode.
	 * MCS params for optional 20/40MHz, NSS=1~3, EQM(NSS>1):
	 * ------------------------------------------------------
	 *	 NSS     |   1   |   2    |    3    |    4
	 * ------------------------------------------------------
	 * MCS index: HT20 | 0 ~ 7 | 8 ~ 15 | 16 ~ 23 | 24 ~ 31
	 * ------------------------------------------------------
	 * MCS index: HT40 | 0 ~ 7 | 8 ~ 15 | 16 ~ 23 | 24 ~ 31
	 * ------------------------------------------------------
	 * Currently, the MAX_NSS=2. If NSS>2, MCS index = 8 * (NSS-1)
	 */
	if ((pkt_type == DOT11_N) && (nss == 2))
		rate_mcs += 8;

	rx_info->rs_ratephy1 = rate_mcs | (nss << 0x8) | (pkt_type << 16) |
				(bw << 24);

	ath12k_dbg_level(dp_pdev->dp->ab, ATH12K_DBG_MMESH, ATH12K_DBG_L2,
			"Mesh rx stats: flags %x, rssi %x, chn %x, rate %x, kix %x, snr %x\n",
			rx_info->rs_flags,
			rx_info->rs_rssi,
			rx_info->rs_channel,
			rx_info->rs_ratephy1,
			rx_info->rs_keyix,
			rx_info->rs_snr);

	if (ahvif->dp_vif.dp_extn.mdbg & MESH_DBG_RXHDR_DUMP)
		ath12k_dp_mesh_rx_status_dump(ab, rx_info);

	if (ath12k_dp_stats_enabled(dp_pdev))
		DP_PEER_MISC_STATS_INC(peer, mmesh_stat, rxhdr_updt, 0, 1);
}
EXPORT_SYMBOL(ath12k_dp_rx_fill_mesh_metadata);


int ath12k_dp_add_mesh_meta_hdr(struct sk_buff  *skb, struct ath12k_vif *ahvif, bool print,
				bool *checkhdr)
{
	struct ath12k_vif_mesh_params params = {0};
	int status = 0;

	if (mmeshsim && !ahvif->dp_vif.dp_extn.mhdr) {
		*checkhdr = false;
		return 0;
	}

	params.mhdr = ahvif->dp_vif.dp_extn.mhdr;
	params.mhdr_len = ahvif->dp_vif.dp_extn.mhdr_len;
	params.mdbg = ahvif->dp_vif.dp_extn.mdbg;

	status = add_mesh_meta_hdr(skb, &params);

	if (status) {
		*checkhdr = false;
		return -1;
	}

	*checkhdr = true;

	if (ahvif->dp_vif.dp_extn.mdbg & MESH_DBG_TXHDR_DUMP) {
		pr_info("### Tx mesh meta header after encap ###");
		print_hex_dump(KERN_INFO, "BUF: ", DUMP_PREFIX_OFFSET, 16, 1, skb->data,
			       sizeof(struct meta_hdr_s), false);
		pr_info("\n");
	}
	return 0;
}
EXPORT_SYMBOL(ath12k_dp_add_mesh_meta_hdr);
