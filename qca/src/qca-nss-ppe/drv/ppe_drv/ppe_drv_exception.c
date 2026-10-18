/*
 * Copyright (c) 2022-2024 Qualcomm Innovation Center, Inc. All rights reserved.
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
#include "ppe_drv.h"

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
		| PPE_DRV_EXCEPTION_FLOW_TYPE_TUNNEL_FLOW,
#ifdef PPE_TUNNEL_ENABLE
	/*
	 * Allow IPV6 Fragmentation packets to exception.
	 */
		FAL_MAC_RDT_TO_CPU,
		PPE_DRV_EXCEPTION_DEACCEL_EN,
		{
			PPE_DRV_EXCPN_TUN_PROFILE_EN,
			PPE_DRV_EXCPN_TUN_PROFILE_DIS,
			PPE_DRV_EXCPN_TUN_PROFILE_DIS,
			PPE_DRV_EXCPN_TUN_PROFILE_DIS,
		}
#endif
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
#ifdef PPE_TUNNEL_ENABLE
	/*
	 * Enable GRE Checksum exception.
	 */
	{
		PPE_DRV_CC_GRE_CSUM,
		PPE_DRV_EXCEPTION_FIELD_INVALID,
		PPE_DRV_EXCEPTION_FIELD_INVALID,
		PPE_DRV_EXCEPTION_FLOW_TYPE_TUNNEL_FLOW,
		FAL_MAC_RDT_TO_CPU,
		PPE_DRV_EXCEPTION_DEACCEL_DIS,
		{
			PPE_DRV_EXCPN_TUN_PROFILE_DIS,
			PPE_DRV_EXCPN_TUN_PROFILE_EN,
			PPE_DRV_EXCPN_TUN_PROFILE_DIS,
			PPE_DRV_EXCPN_TUN_PROFILE_DIS,
		}
	},
#endif
};

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
 * ppe_drv_exception_tun_init
 *	Initialize tunnel specific exception
 */
static void ppe_drv_exception_tun_init(struct ppe_drv *p)
{
	sw_error_t err;

	/*
	 * Configure the GRE checksum error CPU code(220) exception mode to 0.
	 * We want the csum exception packets to be processed by linux GRE
	 * handler which will eventually drop it.
	 *
	 * We cannot use mode 1 here, as the inner packets will be sent to the linux networking stack
	 * and the packets with csum error will eventually be forwarded instead since networking stack
	 * has no information on the outer header.
	 */
	err = fal_mgmtctrl_tunnel_decap_set(PPE_DRV_SWITCH_ID, PPE_DRV_CC_GRE_CSUM, A_FALSE);
	if (err != SW_OK) {
		ppe_drv_warn("%p: Faile to set exception mode for GRE CSUM error CPU Code(220) to 0\n", p);
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
 * ppe_drv_exception_init()
 *	Initialize PPE exceptions
 */
void ppe_drv_exception_init(void)
{
	struct ppe_drv *p = &ppe_drv_gbl;
	const uint8_t exception_max = ppe_drv_exception_max();
	const uint8_t l4_except_max = ppe_drv_exception_tcpflag_max();
	fal_l4_excep_parser_ctrl tcp_except_ctrl = {0};
	struct ppe_drv_exception_tcpflag *tcpflag;
	fal_l3_excep_ctrl_t except_ctrl = {0};
#ifdef PPE_TUNNEL_ENABLE
	fal_tunnel_excep_ctrl_t tun_except_ctrl = {0};
#endif
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

#ifdef PPE_TUNNEL_ENABLE
		if ((pe->flow_type & PPE_DRV_EXCEPTION_FLOW_TYPE_TUNNEL_FLOW) == PPE_DRV_EXCEPTION_FLOW_TYPE_TUNNEL_FLOW) {
			tun_except_ctrl.cmd = pe->tun_action;
			tun_except_ctrl.deacclr_en = pe->tun_deaccel_en;
			tun_except_ctrl.profile_exp_en[PPE_DRV_EXCPN_TUN_PROFILE_ID_0] = pe->tun_profile[PPE_DRV_EXCPN_TUN_PROFILE_ID_0];
			tun_except_ctrl.profile_exp_en[PPE_DRV_EXCPN_TUN_PROFILE_ID_1] = pe->tun_profile[PPE_DRV_EXCPN_TUN_PROFILE_ID_1];
			tun_except_ctrl.profile_exp_en[PPE_DRV_EXCPN_TUN_PROFILE_ID_2] = pe->tun_profile[PPE_DRV_EXCPN_TUN_PROFILE_ID_2];
			tun_except_ctrl.profile_exp_en[PPE_DRV_EXCPN_TUN_PROFILE_ID_3] = pe->tun_profile[PPE_DRV_EXCPN_TUN_PROFILE_ID_3];

			err = fal_sec_tunnel_excep_ctrl_set(PPE_DRV_SWITCH_ID, exp_code, &tun_except_ctrl);
			if (err != SW_OK) {
				ppe_drv_warn("%p: Failed to configure for tunnel exception code %u", p, exp_code);
			}
		}
#endif
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

		except_ctrl.cmd = pe->action;

		/*
		 * L2_Only - bridge flow, with flow disable or bypassed by sc.
		 */
		if ((pe->flow_type & PPE_DRV_EXCEPTION_FLOW_TYPE_L2_ONLY)
				== PPE_DRV_EXCEPTION_FLOW_TYPE_L2_ONLY) {
			except_ctrl.l2fwd_only_en = A_TRUE;
			except_ctrl.l2flow_type = FAL_FLOW_AWARE;
		}

		/*
		 * L3_Only - routed flow, with flow disable or bypassed by sc.
		 */
		if ((pe->flow_type & PPE_DRV_EXCEPTION_FLOW_TYPE_L3_ONLY)
				== PPE_DRV_EXCEPTION_FLOW_TYPE_L3_ONLY) {
			except_ctrl.l3route_only_en = A_TRUE;
			except_ctrl.l3flow_type = FAL_FLOW_AWARE;
		}

		/*
		 * L2_Flow - bridge flow with flow enabled.
		 */
		if ((pe->flow_type & PPE_DRV_EXCEPTION_FLOW_TYPE_L2_FLOW)
				== PPE_DRV_EXCEPTION_FLOW_TYPE_L2_FLOW) {
			except_ctrl.l2flow_en = A_TRUE;
			except_ctrl.l2flow_type = FAL_FLOW_AWARE;
		}

		/*
		 * L3_Flow - routed flow with flow enabled.
		 */
		if ((pe->flow_type & PPE_DRV_EXCEPTION_FLOW_TYPE_L3_FLOW)
				== PPE_DRV_EXCEPTION_FLOW_TYPE_L3_FLOW) {
			except_ctrl.l3flow_en = A_TRUE;
			except_ctrl.l3flow_type = FAL_FLOW_AWARE;
		}

		/*
		 * Multicast flows
		 */
		if ((pe->flow_type & PPE_DRV_EXCEPTION_FLOW_TYPE_MULTICAST)
				== PPE_DRV_EXCEPTION_FLOW_TYPE_MULTICAST) {
			except_ctrl.multicast_en = A_TRUE;
			except_ctrl.l2flow_type = FAL_FLOW_AWARE;
		}

		/*
		 * L2_FLOW_HIT - bridged flow with flow match.
		 */
		if ((pe->flow_type & PPE_DRV_EXCEPTION_FLOW_TYPE_L2_FLOW_HIT)
				== PPE_DRV_EXCEPTION_FLOW_TYPE_L2_FLOW_HIT) {
			except_ctrl.l2flow_en = A_TRUE;
			except_ctrl.l2flow_type = FAL_FLOW_HIT;
		}

		/*
		 * L3_FLOW_HIT - routed flow with flow match.
		 */
		if ((pe->flow_type & PPE_DRV_EXCEPTION_FLOW_TYPE_L3_FLOW_HIT)
				== PPE_DRV_EXCEPTION_FLOW_TYPE_L3_FLOW_HIT) {
			except_ctrl.l3flow_en = A_TRUE;
			except_ctrl.l3flow_type = FAL_FLOW_HIT;
		}

		/*
		 * Configure specific exception in PPE through SSDK.
		 */
		if ((pe->flow_type != PPE_DRV_EXCEPTION_FLOW_TYPE_TUNNEL_FLOW)) {
			err = fal_sec_l3_excep_ctrl_set(PPE_DRV_SWITCH_ID, exp_code, &except_ctrl);
			if (err != SW_OK) {
				ppe_drv_warn("%p: failed to configure L3 exception: %d", p, exp_code);
			}
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
	ppe_drv_exception_tun_init(p);
#endif
}
