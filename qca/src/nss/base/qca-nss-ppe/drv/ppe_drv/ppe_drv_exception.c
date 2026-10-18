/*
 * Copyright (c) 2022-2025 Qualcomm Innovation Center, Inc. All rights reserved.
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

#include <fal/fal_sec.h>
#include <fal/fal_ctrlpkt.h>
#ifdef NSS_PPE_FEATURE_EXCEPTION_EDIT
#include <fal/fal_pktedit.h>
#endif
#include "ppe_drv.h"
#include "ppe_drv_cc_usr.h"

extern int l4_checksum_exception_enable;

/*
 * ppe_drv_exception_list
 *        PPE exception list to be enabled.
 *
 * Note: Exception list is defined using CPU codes.
 */
struct ppe_drv_exception ppe_drv_exception_list[] = {
	{
		PPE_DRV_CC_UNKNOWN_L2_PROT,
		FAL_MAC_RDT_TO_CPU,
		PPE_DRV_EXCEPTION_DEACCEL_EN,
		PPE_DRV_EXCEPTION_FLOW_TYPE_L2_FLOW_HIT
		| PPE_DRV_EXCEPTION_FLOW_TYPE_L3_FLOW_HIT
	},
	{
		PPE_DRV_CC_PPPOE_WRONG_VER_TYPE,
		FAL_MAC_RDT_TO_CPU,
		PPE_DRV_EXCEPTION_DEACCEL_EN,
		PPE_DRV_EXCEPTION_FLOW_TYPE_L2_FLOW_HIT
		| PPE_DRV_EXCEPTION_FLOW_TYPE_L3_FLOW_HIT
	},
	{
		PPE_DRV_CC_PPPOE_WRONG_CODE,
		FAL_MAC_RDT_TO_CPU,
		PPE_DRV_EXCEPTION_DEACCEL_EN,
		PPE_DRV_EXCEPTION_FLOW_TYPE_L2_FLOW_HIT
		| PPE_DRV_EXCEPTION_FLOW_TYPE_L3_FLOW_HIT
	},
	{
		PPE_DRV_CC_PPPOE_UNSUPPORTED_PPP_PROT,
		FAL_MAC_RDT_TO_CPU,
		PPE_DRV_EXCEPTION_DEACCEL_EN,
		PPE_DRV_EXCEPTION_FLOW_TYPE_L2_FLOW_HIT
		| PPE_DRV_EXCEPTION_FLOW_TYPE_L3_FLOW_HIT
	},
	{
		PPE_DRV_CC_IPV4_SMALL_IHL,
		FAL_MAC_RDT_TO_CPU,
		PPE_DRV_EXCEPTION_DEACCEL_DIS,
		PPE_DRV_EXCEPTION_FLOW_TYPE_L2_FLOW_HIT
		| PPE_DRV_EXCEPTION_FLOW_TYPE_L3_FLOW_HIT
	},
	{
		PPE_DRV_CC_IPV4_WITH_OPTION,
		FAL_MAC_RDT_TO_CPU,
		PPE_DRV_EXCEPTION_DEACCEL_EN,
		PPE_DRV_EXCEPTION_FLOW_TYPE_L2_FLOW_HIT
		| PPE_DRV_EXCEPTION_FLOW_TYPE_L3_FLOW_HIT
	},
	{
		PPE_DRV_CC_IPV4_HDR_INCOMPLETE,
		FAL_MAC_RDT_TO_CPU,
		PPE_DRV_EXCEPTION_DEACCEL_DIS,
		PPE_DRV_EXCEPTION_FLOW_TYPE_L2_FLOW_HIT
		| PPE_DRV_EXCEPTION_FLOW_TYPE_L3_FLOW_HIT
	},
	{
		PPE_DRV_CC_IPV4_BAD_TOTAL_LEN,
#ifdef PPE_V4_BAD_LEN_EXCEP_DIS
		FAL_MAC_DROP,
		PPE_DRV_EXCEPTION_DEACCEL_DIS,
#else
		FAL_MAC_RDT_TO_CPU,
		PPE_DRV_EXCEPTION_DEACCEL_DIS,
#endif
		PPE_DRV_EXCEPTION_FLOW_TYPE_L2_FLOW_HIT
		| PPE_DRV_EXCEPTION_FLOW_TYPE_L3_FLOW_HIT
	},
	{
		PPE_DRV_CC_IPV4_DATA_INCOMPLETE,
		FAL_MAC_RDT_TO_CPU,
		PPE_DRV_EXCEPTION_DEACCEL_DIS,
		PPE_DRV_EXCEPTION_FLOW_TYPE_L2_FLOW_HIT
		| PPE_DRV_EXCEPTION_FLOW_TYPE_L3_FLOW_HIT
	},
	{
		PPE_DRV_CC_IPV4_FRAG,
		FAL_MAC_RDT_TO_CPU,
		PPE_DRV_EXCEPTION_DEACCEL_EN,
		PPE_DRV_EXCEPTION_FLOW_TYPE_L2_FLOW_HIT
		| PPE_DRV_EXCEPTION_FLOW_TYPE_L3_FLOW_HIT
	},
	{
		PPE_DRV_CC_IPV4_SMALL_TTL,
		FAL_MAC_RDT_TO_CPU,
		PPE_DRV_EXCEPTION_DEACCEL_EN,
		PPE_DRV_EXCEPTION_FLOW_TYPE_L2_FLOW_HIT
		| PPE_DRV_EXCEPTION_FLOW_TYPE_L3_FLOW_HIT
	},
	{
		PPE_DRV_CC_IPV4_CHECKSUM_ERR,
		FAL_MAC_RDT_TO_CPU,
		PPE_DRV_EXCEPTION_DEACCEL_DIS,
		PPE_DRV_EXCEPTION_FLOW_TYPE_L2_FLOW_HIT
		| PPE_DRV_EXCEPTION_FLOW_TYPE_L3_FLOW_HIT
	},
	{
		PPE_DRV_CC_IPV4_ESP_HDR_INCOMPLETE,
		FAL_MAC_RDT_TO_CPU,
		PPE_DRV_EXCEPTION_DEACCEL_EN,
		PPE_DRV_EXCEPTION_FLOW_TYPE_L2_FLOW_HIT
		| PPE_DRV_EXCEPTION_FLOW_TYPE_L3_FLOW_HIT
	},
	{
		PPE_DRV_CC_IPV6_HDR_INCOMPLETE,
		FAL_MAC_RDT_TO_CPU,
		PPE_DRV_EXCEPTION_DEACCEL_DIS,
		PPE_DRV_EXCEPTION_FLOW_TYPE_L2_FLOW_HIT
		| PPE_DRV_EXCEPTION_FLOW_TYPE_L3_FLOW_HIT
	},
	{
		PPE_DRV_CC_IPV6_BAD_PAYLOAD_LEN,
		FAL_MAC_RDT_TO_CPU,
		PPE_DRV_EXCEPTION_DEACCEL_DIS,
		PPE_DRV_EXCEPTION_FLOW_TYPE_L2_FLOW_HIT
		| PPE_DRV_EXCEPTION_FLOW_TYPE_L3_FLOW_HIT
	},
	{
		PPE_DRV_CC_IPV6_DATA_INCOMPLETE,
		FAL_MAC_RDT_TO_CPU,
		PPE_DRV_EXCEPTION_DEACCEL_DIS,
		PPE_DRV_EXCEPTION_FLOW_TYPE_L2_FLOW_HIT
		| PPE_DRV_EXCEPTION_FLOW_TYPE_L3_FLOW_HIT
	},
	{
		PPE_DRV_CC_IPV6_SMALL_HOP_LIMIT,
		FAL_MAC_RDT_TO_CPU,
		PPE_DRV_EXCEPTION_DEACCEL_EN,
		PPE_DRV_EXCEPTION_FLOW_TYPE_L3_FLOW_HIT
	},
	{
		PPE_DRV_CC_IPV6_FRAG,
		FAL_MAC_RDT_TO_CPU,
		PPE_DRV_EXCEPTION_DEACCEL_EN,
		PPE_DRV_EXCEPTION_FLOW_TYPE_L2_FLOW_HIT
		| PPE_DRV_EXCEPTION_FLOW_TYPE_L3_FLOW_HIT
	},
	{
		PPE_DRV_CC_IPV6_ESP_HDR_INCOMPLETE,
		FAL_MAC_RDT_TO_CPU,
		PPE_DRV_EXCEPTION_DEACCEL_EN,
		PPE_DRV_EXCEPTION_FLOW_TYPE_L2_FLOW_HIT
		| PPE_DRV_EXCEPTION_FLOW_TYPE_L3_FLOW_HIT
	},
	{
		PPE_DRV_CC_TCP_HDR_INCOMPLETE,
		FAL_MAC_RDT_TO_CPU,
		PPE_DRV_EXCEPTION_DEACCEL_EN,
		PPE_DRV_EXCEPTION_FLOW_TYPE_L2_FLOW_HIT
		| PPE_DRV_EXCEPTION_FLOW_TYPE_L3_FLOW_HIT
	},
	{
		PPE_DRV_CC_TCP_SMALL_DATA_OFFSET,
		FAL_MAC_RDT_TO_CPU,
		PPE_DRV_EXCEPTION_DEACCEL_EN,
		PPE_DRV_EXCEPTION_FLOW_TYPE_L2_FLOW_HIT
		| PPE_DRV_EXCEPTION_FLOW_TYPE_L3_FLOW_HIT
	},
	{
		PPE_DRV_CC_TCP_FLAGS_0,
		FAL_MAC_RDT_TO_CPU,
		PPE_DRV_EXCEPTION_DEACCEL_EN,
		PPE_DRV_EXCEPTION_FLOW_TYPE_L2_FLOW_HIT
		| PPE_DRV_EXCEPTION_FLOW_TYPE_L3_FLOW_HIT
	},
	{
		PPE_DRV_CC_TCP_FLAGS_1,
		FAL_MAC_RDT_TO_CPU,
		PPE_DRV_EXCEPTION_DEACCEL_EN,
		PPE_DRV_EXCEPTION_FLOW_TYPE_L2_FLOW_HIT
		| PPE_DRV_EXCEPTION_FLOW_TYPE_L3_FLOW_HIT
	},
	{
		PPE_DRV_CC_TCP_FLAGS_2,
		FAL_MAC_RDT_TO_CPU,
		PPE_DRV_EXCEPTION_DEACCEL_EN,
		PPE_DRV_EXCEPTION_FLOW_TYPE_L2_FLOW_HIT
		| PPE_DRV_EXCEPTION_FLOW_TYPE_L3_FLOW_HIT
	},
	{
		PPE_DRV_CC_UDP_HDR_INCOMPLETE,
		FAL_MAC_RDT_TO_CPU,
		PPE_DRV_EXCEPTION_DEACCEL_EN,
		PPE_DRV_EXCEPTION_FLOW_TYPE_L2_FLOW_HIT
		| PPE_DRV_EXCEPTION_FLOW_TYPE_L3_FLOW_HIT
	},
	{
		PPE_DRV_CC_UDP_LITE_HDR_INCOMPLETE,
		FAL_MAC_RDT_TO_CPU,
		PPE_DRV_EXCEPTION_DEACCEL_EN,
		PPE_DRV_EXCEPTION_FLOW_TYPE_L2_FLOW_HIT
		| PPE_DRV_EXCEPTION_FLOW_TYPE_L3_FLOW_HIT
	},
};

#ifdef PPE_TUNNEL_ENABLE
/*
 * ppe_drv_tun_exception_list
 *        PPE tunnel exception list to be enabled.
 */
struct ppe_drv_tun_exception ppe_drv_tun_exception_list[] = {
	/*
	 * For fragmented packets we only need the outer packet excepted to the host
	 * (decap exception mode 0) so that the networking stack can reassemble it.
	 * There is no need to de-accelerate the entire flow for these packets, hence
	 * deacceleration is disabled.
	 */
	{
		.code = PPE_DRV_CC_IPV4_FRAG,
		.action = FAL_MAC_RDT_TO_CPU,
		.deaccel_en = PPE_DRV_EXCEPTION_DEACCEL_DIS,
		.profile_exp_en = {
			PPE_DRV_EXCPN_TUN_PROFILE_EN,
			PPE_DRV_EXCPN_TUN_PROFILE_DIS,
			PPE_DRV_EXCPN_TUN_PROFILE_DIS,
			PPE_DRV_EXCPN_TUN_PROFILE_DIS,
		},
		.flush_required = false,
		.outer_exp_en = true,
	},
	{
		.code = PPE_DRV_CC_IPV6_FRAG,
		.action = FAL_MAC_RDT_TO_CPU,
		.deaccel_en = PPE_DRV_EXCEPTION_DEACCEL_DIS,
		.profile_exp_en = {
			PPE_DRV_EXCPN_TUN_PROFILE_EN,
			PPE_DRV_EXCPN_TUN_PROFILE_DIS,
			PPE_DRV_EXCPN_TUN_PROFILE_DIS,
			PPE_DRV_EXCPN_TUN_PROFILE_DIS,
		},
		.flush_required = false,
		.outer_exp_en = true,
	},
	/*
	 * Configure the GRE checksum error CPU code(220) exception mode to 0.
	 * We want the csum exception packets to be processed by linux GRE
	 * handler which will eventually drop it.
	 *
	 * We cannot use mode 1 here, as the inner packets will be sent to the linux networking stack
	 * and the packets with csum error will eventually be forwarded instead since networking stack
	 * has no information on the outer header.
	 */
	{
		.code = PPE_DRV_CC_GRE_CSUM,
		.action = FAL_MAC_RDT_TO_CPU,
		.deaccel_en = PPE_DRV_EXCEPTION_DEACCEL_DIS,
		.profile_exp_en = {
			PPE_DRV_EXCPN_TUN_PROFILE_DIS,
			PPE_DRV_EXCPN_TUN_PROFILE_EN,
			PPE_DRV_EXCPN_TUN_PROFILE_DIS,
			PPE_DRV_EXCPN_TUN_PROFILE_DIS,
		},
		.flush_required = false,
		.outer_exp_en = true,
	},
	/*
	 * The inner TCP flags are parsed for tunnel flows, so these exceptions
	 * require the decapsulated inner packet (decap exception mode 1) in order to
	 * identify the connection that needs to be flushed.
	 */
	{
		.code = PPE_DRV_CC_TCP_FLAGS_0,
		.action = FAL_MAC_RDT_TO_CPU,
		.deaccel_en = PPE_DRV_EXCEPTION_DEACCEL_EN,
		.profile_exp_en = {
			PPE_DRV_EXCPN_TUN_PROFILE_EN,
			PPE_DRV_EXCPN_TUN_PROFILE_EN,
			PPE_DRV_EXCPN_TUN_PROFILE_EN,
			PPE_DRV_EXCPN_TUN_PROFILE_EN,
		},
		.flush_required = true,
	},
	{
		.code = PPE_DRV_CC_TCP_FLAGS_1,
		.action = FAL_MAC_RDT_TO_CPU,
		.deaccel_en = PPE_DRV_EXCEPTION_DEACCEL_EN,
		.profile_exp_en = {
			PPE_DRV_EXCPN_TUN_PROFILE_EN,
			PPE_DRV_EXCPN_TUN_PROFILE_EN,
			PPE_DRV_EXCPN_TUN_PROFILE_EN,
			PPE_DRV_EXCPN_TUN_PROFILE_EN,
		},
		.flush_required = true,
	},
	{
		.code = PPE_DRV_CC_TCP_FLAGS_2,
		.action = FAL_MAC_RDT_TO_CPU,
		.deaccel_en = PPE_DRV_EXCEPTION_DEACCEL_EN,
		.profile_exp_en = {
			PPE_DRV_EXCPN_TUN_PROFILE_EN,
			PPE_DRV_EXCPN_TUN_PROFILE_EN,
			PPE_DRV_EXCPN_TUN_PROFILE_EN,
			PPE_DRV_EXCPN_TUN_PROFILE_EN,
		},
		.flush_required = true,
	},
};

/*
 * ppe_drv_tun_exception_max()
 *	Returns number of tunnel exceptions configured.
 */
uint8_t ppe_drv_tun_exception_max(void)
{
	return ARRAY_SIZE(ppe_drv_tun_exception_list);
}

/*
 * ppe_drv_tun_exception_is_flush_required()
 *	Check if connection flush is required for the tunnel exception.
 */
bool ppe_drv_tun_exception_is_flush_required(ppe_drv_cc_t cc)
{
	uint32_t i;

	for (i = 0; i < ppe_drv_tun_exception_max(); i++) {
		if (ppe_drv_tun_exception_list[i].code == cc) {
			return ppe_drv_tun_exception_list[i].flush_required;
		}
	}

	return false;
}
#endif

/*
 * ppe_drv_exception_tcpflag_list
 *	TCP Flags to be enabled.
 */
static struct ppe_drv_exception_tcpflag ppe_drv_exception_tcpflag_list[] = {
	{
		PPE_DRV_TCP_FLAG_FIN
	},
	{
		PPE_DRV_TCP_FLAG_SYN
	},
	{
		PPE_DRV_TCP_FLAG_RST
	},
};

/*
 * ppe_drv_exception_max()
 *	Returns number of exception enabled in hardware.
 */
const uint8_t ppe_drv_exception_max()
{
	return sizeof(ppe_drv_exception_list) / sizeof(struct ppe_drv_exception);
}

/*
 * ppe_drv_exception_tcpflag_max()
 *	Returns number of tcp flag exception enabled in hardware.
 */
const uint8_t ppe_drv_exception_tcpflag_max()
{
	return sizeof(ppe_drv_exception_tcpflag_list) / sizeof(struct ppe_drv_exception_tcpflag);
}

/*
 * ppe_drv_exception_cc2exp()
 *	CPU codes to Exception codes
 */
static inline uint8_t ppe_drv_exception_cc2exp(uint8_t cpu_code)
{
	if (cpu_code <= PPE_DRV_CC_RANGE1) {
		return (cpu_code - PPE_DRV_EXP_RANGE1_BASE);
	} else if ((cpu_code >= PPE_DRV_CC_RANGE2 && cpu_code <= PPE_DRV_CC_RANGE3) ||
			(cpu_code >= PPE_DRV_CC_RANGE4 && cpu_code <= PPE_DRV_CC_RANGE5)) {
		return (cpu_code - PPE_DRV_EXP_RANGE2_BASE);
	}

	return cpu_code;
}

#ifdef PPE_TUNNEL_ENABLE
/*
 * ppe_drv_exception_tun_config()
 *	Initialize tunnel specific exception configuration, TCP flag parsers, and command tables.
 */
static void ppe_drv_exception_tun_config(struct ppe_drv *p)
{
	fal_l4_excep_parser_ctrl tun_l4_ctrl = {0};
	sw_error_t err;
	uint8_t exp_code;
	uint32_t i, j;

	/*
	 * Configure the Tunnel L4 TCP flags parser for SYN, FIN, and RST.
	 * This ensures that inner TCP flags are parsed correctly for tunnel flows.
	 */
	tun_l4_ctrl.tcp_flags[0] = PPE_DRV_TCP_FLAG_FIN;
	tun_l4_ctrl.tcp_flags_mask[0] = PPE_DRV_TCP_FLAG_FIN;

	tun_l4_ctrl.tcp_flags[1] = PPE_DRV_TCP_FLAG_SYN;
	tun_l4_ctrl.tcp_flags_mask[1] = PPE_DRV_TCP_FLAG_SYN;

	tun_l4_ctrl.tcp_flags[2] = PPE_DRV_TCP_FLAG_RST;
	tun_l4_ctrl.tcp_flags_mask[2] = PPE_DRV_TCP_FLAG_RST;

	err = fal_sec_tunnel_l4_excep_parser_ctrl_set(PPE_DRV_SWITCH_ID, &tun_l4_ctrl);
	if (err != SW_OK) {
		ppe_drv_warn("%p: failed to configure Tunnel L4 TCP flags parser: %d", p, err);
	}

	/*
	 * Traverse through the tunnel exception list and configure each tunnel exception in SSDK.
	 */
	for (i = 0; i < ppe_drv_tun_exception_max(); i++) {
		struct ppe_drv_tun_exception *te = &ppe_drv_tun_exception_list[i];
		fal_tunnel_excep_ctrl_t tun_except_ctrl = {0};

		exp_code = ppe_drv_exception_cc2exp(te->code);
		if (exp_code >= PPE_DRV_TUN_EXCEPTION_CMD_MAX_ENTRY) {
			ppe_drv_warn("%p: tunnel exception code %u is out of bounds (max %u)", p, exp_code,
					PPE_DRV_TUN_EXCEPTION_CMD_MAX_ENTRY - 1);
			continue;
		}

		ppe_drv_trace("%p: configuring tunnel exception code: %u", p, exp_code);

		tun_except_ctrl.cmd = te->action;
		tun_except_ctrl.deacclr_en = te->deaccel_en;
		for (j = 0; j < PPE_DRV_EXCPN_TUN_PROFILE_ID_MAX; j++) {
			tun_except_ctrl.profile_exp_en[j] = te->profile_exp_en[j];
		}

		err = fal_sec_tunnel_excep_ctrl_set(PPE_DRV_SWITCH_ID, exp_code, &tun_except_ctrl);
		if (err != SW_OK) {
			ppe_drv_warn("%p: Failed to configure for tunnel exception code %u", p, exp_code);
		}

		/*
		 * Configure the decap exception mode. When outer_exp_en is set, the outer
		 * (still encapsulated) packet is excepted to the host (mode 0); otherwise the
		 * decapsulated inner packet is excepted (mode 1).
		 */
		err = fal_mgmtctrl_tunnel_decap_set(PPE_DRV_SWITCH_ID, te->code, !te->outer_exp_en);
		if (err != SW_OK) {
			ppe_drv_warn("%p: failed to set outer exception %u for tunnel exception code %u",
					p, te->outer_exp_en, te->code);
		}
	}
}
#endif

/*
 * ppe_drv_exception_l4_checksum_enable()
 *	Sets L4 exception for incorrect checksum in the packet.
 */
int ppe_drv_exception_l4_checksum_enable()
{
	fal_l3_excep_ctrl_t except_ctrl = {0};
	sw_error_t err;

	except_ctrl.l3flow_en = A_TRUE;
	except_ctrl.l3flow_type = FAL_FLOW_HIT;
	except_ctrl.l2flow_en = A_TRUE;
	except_ctrl.l2flow_type = FAL_FLOW_HIT;

	if (l4_checksum_exception_enable) {
		except_ctrl.cmd = FAL_MAC_RDT_TO_CPU;
		err = fal_sec_l3_excep_ctrl_set(PPE_DRV_SWITCH_ID, ppe_drv_exception_cc2exp(PPE_DRV_CC_UDP_CHECKSUM_ERR), &except_ctrl);
		if (err != SW_OK) {
			ppe_drv_warn("Failed to set UDP exception ctrl err: %d", err);
			return -1;
		}

		err = fal_sec_l3_excep_ctrl_set(PPE_DRV_SWITCH_ID, ppe_drv_exception_cc2exp(PPE_DRV_CC_TCP_CHECKSUM_ERR), &except_ctrl);
		if (err != SW_OK) {
			ppe_drv_warn("Failed to set TCP exception ctrl err: %d", err);
			except_ctrl.cmd = FAL_MAC_FRWRD;
			fal_sec_l3_excep_ctrl_set(PPE_DRV_SWITCH_ID, ppe_drv_exception_cc2exp(PPE_DRV_CC_UDP_CHECKSUM_ERR), &except_ctrl);
			return -1;
		}

	} else if (!l4_checksum_exception_enable){
		except_ctrl.cmd = FAL_MAC_FRWRD;
		err = fal_sec_l3_excep_ctrl_set(PPE_DRV_SWITCH_ID, ppe_drv_exception_cc2exp(PPE_DRV_CC_UDP_CHECKSUM_ERR), &except_ctrl);
		if (err != SW_OK) {
			ppe_drv_warn("Failed to set rdtcpu for UDP exception err: %d", err);
			return -1;
		}

		err = fal_sec_l3_excep_ctrl_set(PPE_DRV_SWITCH_ID, ppe_drv_exception_cc2exp(PPE_DRV_CC_TCP_CHECKSUM_ERR), &except_ctrl);
		if (err != SW_OK) {
			ppe_drv_warn("Failed to set rdtcpu for TCP exception err: %d", err);
			except_ctrl.cmd = FAL_MAC_RDT_TO_CPU;
			fal_sec_l3_excep_ctrl_set(PPE_DRV_SWITCH_ID, ppe_drv_exception_cc2exp(PPE_DRV_CC_UDP_CHECKSUM_ERR), &except_ctrl);
			return -1;
		}
	}

	return 0;
}

/*
 * ppe_drv_exception_configure_internal()
 *	Configuring CPU code fields
 */
static void ppe_drv_exception_configure_internal(fal_l3_excep_ctrl_t *except_ctrl, fal_fwd_cmd_t action, uint16_t flow_type)
{
	except_ctrl->cmd = action;
	/*
	 * L2_Only - bridge flow, with flow disable or bypassed by sc.
	 */
	if ((flow_type & PPE_DRV_EXCEPTION_FLOW_TYPE_L2_ONLY)
			== PPE_DRV_EXCEPTION_FLOW_TYPE_L2_ONLY) {
		except_ctrl->l2fwd_only_en = A_TRUE;
		except_ctrl->l2flow_type = FAL_FLOW_AWARE;
	}

	/*
	 * L3_Only - routed flow, with flow disable or bypassed by sc.
	 */
	if ((flow_type & PPE_DRV_EXCEPTION_FLOW_TYPE_L3_ONLY)
			== PPE_DRV_EXCEPTION_FLOW_TYPE_L3_ONLY) {
		except_ctrl->l3route_only_en = A_TRUE;
		except_ctrl->l3flow_type = FAL_FLOW_AWARE;
	}

	/*
	 * L2_Flow - bridge flow with flow enabled.
	 */
	if ((flow_type & PPE_DRV_EXCEPTION_FLOW_TYPE_L2_FLOW)
			== PPE_DRV_EXCEPTION_FLOW_TYPE_L2_FLOW) {
		except_ctrl->l2flow_en = A_TRUE;
		except_ctrl->l2flow_type = FAL_FLOW_AWARE;
	}

	/*
	 * L3_Flow - routed flow with flow enabled.
	 */
	if ((flow_type & PPE_DRV_EXCEPTION_FLOW_TYPE_L3_FLOW)
			== PPE_DRV_EXCEPTION_FLOW_TYPE_L3_FLOW) {
		except_ctrl->l3flow_en = A_TRUE;
		except_ctrl->l3flow_type = FAL_FLOW_AWARE;
	}

	/*
	 * Multicast flows
	 */
	if ((flow_type & PPE_DRV_EXCEPTION_FLOW_TYPE_MULTICAST)
			== PPE_DRV_EXCEPTION_FLOW_TYPE_MULTICAST) {
		except_ctrl->multicast_en = A_TRUE;
		except_ctrl->l2flow_type = FAL_FLOW_AWARE;
	}

	/*
	 * L2_FLOW_HIT - bridged flow with flow match.
	 */
	if ((flow_type & PPE_DRV_EXCEPTION_FLOW_TYPE_L2_FLOW_HIT)
			== PPE_DRV_EXCEPTION_FLOW_TYPE_L2_FLOW_HIT) {
		except_ctrl->l2flow_en = A_TRUE;
		except_ctrl->l2flow_type = FAL_FLOW_HIT;
	}

	/*
	 * L3_FLOW_HIT - routed flow with flow match.
	 */
	if ((flow_type & PPE_DRV_EXCEPTION_FLOW_TYPE_L3_FLOW_HIT)
			== PPE_DRV_EXCEPTION_FLOW_TYPE_L3_FLOW_HIT) {
		except_ctrl->l3flow_en = A_TRUE;
		except_ctrl->l3flow_type = FAL_FLOW_HIT;
	}
}

/*
 * ppe_drv_exception_init()
 *	Initialize PPE exceptions
 */
void ppe_drv_exception_init(void)
{
	struct ppe_drv *p = ppe_drv_gbl;
	const uint8_t exception_max = ppe_drv_exception_max();
	const uint8_t l4_except_max = ppe_drv_exception_tcpflag_max();
	fal_l4_excep_parser_ctrl tcp_except_ctrl = {0};
	struct ppe_drv_exception_tcpflag *tcpflag;
	fal_l3_excep_ctrl_t except_ctrl = {0};
	sw_error_t err;
	uint32_t i;
	uint8_t exp_code;

	/*
	 * Traverse through exception list and configure each exception
	 */
	for (i = 0; i < exception_max; i++) {
		struct ppe_drv_exception *pe = &ppe_drv_exception_list[i];
		exp_code = ppe_drv_exception_cc2exp(pe->code);

		ppe_drv_trace("%p: configuring exception code: %u flow_type: 0x%x",
				p, exp_code, pe->flow_type);

		/*
		 * Since our exception list is now defined using CPU code, we need to
		 * Convert CPU code into exception code for exception configuration.
		 * l3_excep_ctrl table only configures exceptions till number 71, hence
		 * We need to do necessary check to avoid configuring exception
		 * Table above 71.
		 */
		if (exp_code > ppe_drv_exception_cc2exp(PPE_DRV_CC_UDP_LITE_CHECKSUM_ERR) + 4) {
			ppe_drv_trace("%p: exception code greater than 71 not configured in l3 exception control table: %d\n", p, exp_code);
			continue;
		}

		/*
		 * Enable Exception
		 */
		if (!flow_deacclr_dis)
			except_ctrl.deacclr_en = pe->deaccel_en;

		ppe_drv_exception_configure_internal(&except_ctrl, pe->action, pe->flow_type);

		/*
		 * Configure specific exception in PPE through SSDK.
		 */
		err = fal_sec_l3_excep_ctrl_set(PPE_DRV_SWITCH_ID, exp_code, &except_ctrl);
		if (err != SW_OK) {
			ppe_drv_warn("%p: failed to configure L3 exception: %d", p, exp_code);
		}

		memset(&except_ctrl, 0, sizeof(except_ctrl));
	}

	/*
	 * TCP_FLAG_* are special cases, need to set ctrl and mask register.
	 *
	 * Note: We use independent exceptions for each TCP flags - this
	 * allow us to get a unique CPU code for each TCP flag and help us
	 * provide an explicit reason for exception.
	 */
	for (i = 0; i < l4_except_max; i++) {
		tcpflag = &ppe_drv_exception_tcpflag_list[i];
		tcp_except_ctrl.tcp_flags[i] =  tcpflag->flags;
		tcp_except_ctrl.tcp_flags_mask[i] = tcpflag->flags;
	}

	err = fal_sec_l4_excep_parser_ctrl_set(PPE_DRV_SWITCH_ID, &tcp_except_ctrl);
	if (err != SW_OK) {
		ppe_drv_warn("%p: failed to configure L4 exception: %p", p, &tcp_except_ctrl);
	}

#ifdef PPE_TUNNEL_ENABLE
	ppe_drv_exception_tun_config(p);
#endif
}

/*
 * ppe_drv_cc_exception_configure()
 *	Initialize PPE exceptions
 */
ppe_drv_cc_usr_ret_t ppe_drv_cc_exception_configure(struct ppe_drv_cc_usr_exception_info *info)
{
	struct ppe_drv *p = ppe_drv_gbl;
	struct ppe_drv_cc *pcc;
	fal_l4_excep_parser_ctrl tcp_except_ctrl = {0};
	fal_l3_excep_ctrl_t except_ctrl = {0};
	fal_ip_global_cfg_t cfg = {0};
#ifdef PPE_TUNNEL_ENABLE
	fal_tunnel_excep_ctrl_t tun_except_ctrl = {0};
#endif
	sw_error_t err;
	uint8_t exp_code;
	ppe_drv_cc_usr_ret_t ret;
	uint8_t profile __maybe_unused;
	uint8_t cpu_code;
	uint8_t flag;

	exp_code = ppe_drv_exception_cc2exp((uint8_t)info->code);
	ppe_drv_trace("%p: configuring exception code: %u flow_type: 0x%x",
			p, exp_code, info->flow_type);

	cpu_code = (uint8_t)info->code;
	pcc = &p->cc[cpu_code];

	/*
	 * Enable/disable flush for this CPU code as requested.
	 */
	pcc->flush = info->flush_en;

	/*
	 * Exception edit enable for PPE pkt edits with specific CPU code.
	 */
#ifdef NSS_PPE_FEATURE_EXCEPTION_EDIT
	err = fal_pktedit_en_set(PPE_DRV_SWITCH_ID, info->code, info->exception_edit_en);
	if (err != SW_OK) {
		ppe_drv_warn("%p: failed to configure exception edit enable - cpu_code:%d", p, info->code);
		ret = PPE_DRV_CC_USR_RET_EXCP_EDIT_CONFIGURE_FAILED;
		return ret;
	}
#endif

#ifdef PPE_TUNNEL_ENABLE
	if ((info->flow_type & PPE_DRV_EXCEPTION_FLOW_TYPE_TUNNEL_FLOW) == PPE_DRV_EXCEPTION_FLOW_TYPE_TUNNEL_FLOW) {
		uint32_t j;

		tun_except_ctrl.cmd = (fal_fwd_cmd_t)info->action;
		tun_except_ctrl.deacclr_en = info->deaccel_en;
		profile = info->tun_profile;
		if ((profile >> 0) & 1) {
			tun_except_ctrl.profile_exp_en[PPE_DRV_EXCPN_TUN_PROFILE_ID_0] = PPE_DRV_EXCPN_TUN_PROFILE_EN;
		} else if ((profile >> 1) & 1) {
			tun_except_ctrl.profile_exp_en[PPE_DRV_EXCPN_TUN_PROFILE_ID_1]  = PPE_DRV_EXCPN_TUN_PROFILE_EN;
		} else if ((profile >> 2) & 1) {
			tun_except_ctrl.profile_exp_en[PPE_DRV_EXCPN_TUN_PROFILE_ID_2] = PPE_DRV_EXCPN_TUN_PROFILE_EN;
		} else if ((profile >> 3) & 1) {
			tun_except_ctrl.profile_exp_en[PPE_DRV_EXCPN_TUN_PROFILE_ID_3] = PPE_DRV_EXCPN_TUN_PROFILE_EN;
		}

		err = fal_sec_tunnel_excep_ctrl_set(PPE_DRV_SWITCH_ID, exp_code, &tun_except_ctrl);
		if (err != SW_OK) {
			ppe_drv_warn("%p: Failed to configure for tunnel exception code %u", p, exp_code);
			ret = PPE_DRV_CC_USR_RET_TUNNEL_CONFIG_FAILED;
			return ret;
		}

		/*
		 * Synchronize the tunnel list entry's flush_required flag with the
		 * Netlink setting.
		 *
		 * Note: the decap exception mode is a static property of the tunnel
		 * exception and is configured only during initialization.
		 */
		for (j = 0; j < ppe_drv_tun_exception_max(); j++) {
			if (ppe_drv_tun_exception_list[j].code == info->code) {
				ppe_drv_tun_exception_list[j].flush_required = info->flush_en;
				break;
			}
		}

		if (j == ppe_drv_tun_exception_max()) {
			ppe_drv_info("%p: cpu code %u is not a handled tunnel exception, "
					"tunnel connection flush would not be attempted for it",
					p, info->code);
		}
	}
#else
	if (info->flow_type & PPE_DRV_EXCEPTION_FLOW_TYPE_TUNNEL_FLOW) {
		return PPE_DRV_CC_USR_RET_TUNNEL_CONFIG_FAILED;
	}
#endif

	/*
	 * Enable Exception
	 */
	except_ctrl.deacclr_en = info->deaccel_en;

	/*
	 * Since our exception list is now defined using CPU code, we need to
	 * Convert CPU code into exception code for exception configuration.
	 * l3_excep_ctrl table only configures exceptions till number 71, hence
	 * We need to do necessary check to avoid configuring exception
	 * Table above 71.
	 */
	if (exp_code > ppe_drv_exception_cc2exp(PPE_DRV_CC_UDP_LITE_CHECKSUM_ERR) + 4) {
		/*
		 * Configure the exception if it is of MTU FAIL type
		 */
		if (info->code == PPE_DRV_CC_L3_EXP_MTU_FAIL) {
			cfg.mtu_fail_action = (fal_fwd_cmd_t)info->action;
			cfg.mtu_deacclr_en = info->deaccel_en;
			if (fal_ip_global_ctrl_set(PPE_DRV_SWITCH_ID, &cfg) != SW_OK) {
				ppe_drv_warn("%p: IP global control configuration failed\n", p);
				ret = PPE_DRV_CC_USR_RET_L4_CONFIGURE_FAILED;
				return ret;
			}
			return PPE_DRV_CC_USR_RET_SUCCESS;
		}

		if (info->code == PPE_DRV_CC_L3_EXP_MRU_FAIL) {
			cfg.mru_fail_action = (fal_fwd_cmd_t)info->action;
			cfg.mru_deacclr_en = info->deaccel_en;
			if (fal_ip_global_ctrl_set(PPE_DRV_SWITCH_ID, &cfg) != SW_OK) {
				ppe_drv_warn("%p: failed to configure MRU exception\n", p);
				ret = PPE_DRV_CC_USR_RET_MRU_CONFIGURE_FAILED;
				return ret;
			}
			return PPE_DRV_CC_USR_RET_SUCCESS;
		}

		ppe_drv_trace("%p: exception code greater than 71 not configured in l3 exception control table: %d\n", p, exp_code);
		ret = PPE_DRV_CC_USR_RET_CONFIGURE_NOT_ALLOWED;
		return ret;
	}

	ppe_drv_exception_configure_internal(&except_ctrl, (fal_fwd_cmd_t)info->action, info->flow_type);

	/*
	 * Configure specific exception in PPE through SSDK.
	 */
	err = fal_sec_l3_excep_ctrl_set(PPE_DRV_SWITCH_ID, exp_code, &except_ctrl);
	if (err != SW_OK) {
		ppe_drv_warn("%p: failed to configure L3 exception: %d", p, exp_code);
		ret = PPE_DRV_CC_USR_RET_L3_CONFIGURE_FAILED;
		return ret;
	}

	/*
	 * TCP_FLAG_* are special cases, need to set ctrl and mask register.
	 */
	if ((info->code >= PPE_DRV_CC_USR_TCP_FLAG_MIN) && (info->code < PPE_DRV_CC_USR_TCP_FLAG_MAX)) {
		int index = (info->code) - PPE_DRV_CC_USR_TCP_FLAG_MIN;
		flag = (1 << index);
		tcp_except_ctrl.tcp_flags[index] =  flag;
		tcp_except_ctrl.tcp_flags_mask[index] = flag;

		err = fal_sec_l4_excep_parser_ctrl_set(PPE_DRV_SWITCH_ID, &tcp_except_ctrl);
		if (err != SW_OK) {
			ppe_drv_warn("%p: failed to configure L4 exception: %p", p, &tcp_except_ctrl);
			ret = PPE_DRV_CC_USR_RET_L4_CONFIGURE_FAILED;
			return ret;
		}
	}

	return PPE_DRV_CC_USR_RET_SUCCESS;
}
EXPORT_SYMBOL(ppe_drv_cc_exception_configure);
