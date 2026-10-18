/*
 * Copyright (c) 2012, 2016-2018, 2021, The Linux Foundation. All rights reserved.
 *
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: ISC
 */

/**
 * @defgroup fal_port_vlan FAL_PORT_VLAN
 * @{
 */
#ifndef _FAL_PORT_VLAN_H_
#define _FAL_PORT_VLAN_H_

#ifdef __cplusplus
extern "C" {
#endif                          /* __cplusplus */

#include "sw.h"
#include "fal_type.h"
#include "fal_portvlan_legacy.h"

#if defined(SW_API_LOCK) && (!defined(HSL_STANDALONG))
#define FAL_PORTVLAN_API_LOCK FAL_API_LOCK
#define FAL_PORTVLAN_API_UNLOCK FAL_API_UNLOCK
#else
#define FAL_PORTVLAN_API_LOCK
#define FAL_PORTVLAN_API_UNLOCK
#endif

#define FAL_DEF_VLAN_STPID 0x88a8
#define FAL_DEF_VLAN_CTPID 0x8100

/*
* Vlan frame type match in vlan translation
*/
#define FAL_PORT_VLAN_XLT_MATCH_UNTAGGED    0x1
#define FAL_PORT_VLAN_XLT_MATCH_PRIO_TAG    0x2
#define FAL_PORT_VLAN_XLT_MATCH_TAGGED      0x4

typedef enum
{
	FAL_FRAMETYPE_ETHERNET = 0,
	FAL_FRAMETYPE_RFC_1024 = 1,
	FAL_FRAMETYPE_LLC_OTHER = 2,
	FAL_FRAMETYPE_ETHORRFC1024 = 3,
} fal_frametype_t;

typedef enum
{
	FAL_VID_XLT_CMD_UNCHANGED = 0,
	FAL_VID_XLT_CMD_ADDORREPLACE = 1,
	FAL_VID_XLT_CMD_DELETE = 2,
	FAL_VID_XLT_CMD_CPFRM_SVID = 3,
	FAL_VID_XLT_CMD_CPFRM_CVID = 4,
} fal_vid_xlt_cmd_t;

typedef enum
{
	FAL_PCP_XLT_CMD_UNCHANGED = 0,
	FAL_PCP_XLT_CMD_REPLACE = 1,
	FAL_PCP_XLT_CMD_CPFRM_SPCP = 2,
	FAL_PCP_XLT_CMD_CPFRM_CPCP = 3,
	FAL_PCP_XLT_CMD_MAPFRM_DSCP = 4,
	FAL_PCP_XLT_CMD_ADD_TAG_AND_REPLACE = 5,
	FAL_PCP_XLT_CMD_ADD_TAG_AND_CPFRM_SPCP = 6,
	FAL_PCP_XLT_CMD_ADD_TAG_AND_CPFRM_CPCP = 7,
	FAL_PCP_XLT_CMD_ADD_TAG_AND_MAPFRM_DSCP = 8,
} fal_pcp_xlt_cmd_t;

typedef enum
{
	FAL_DEI_XLT_CMD_UNCHANGED = 0,
	FAL_DEI_XLT_CMD_REPLACE = 1,
	FAL_DEI_XLT_CMD_CPFRM_SDEI = 2,
	FAL_DEI_XLT_CMD_CPFRM_CDEI = 3,
} fal_dei_xlt_cmd_t;

typedef enum
{
	FAL_TPID_IDX_XLT_CMD_UNCHANGED = 0,
	FAL_TPID_IDX_XLT_CMD_REPLACE = 1,
	FAL_TPID_IDX_XLT_CMD_CPFRM_STPID_IDX = 2,
	FAL_TPID_IDX_XLT_CMD_CPFRM_CTPID_IDX = 3,
} fal_tpid_idx_xlt_cmd_t;

typedef enum
{
	FAL_PORT_VLAN_ALL = 0,
	FAL_PORT_VLAN_INGRESS = 1,
	FAL_PORT_VLAN_EGRESS = 2,
} fal_port_vlan_direction_t;


/**
  @details  Fields description:

  o_vid - original vlan id
  s_vid - service vid id
  c_vid - custom vid id
  bi_dir - entry search direction
  forward_dir - entry search direction only be forward
  reverse_dir - entry search direction only be reverse
  o_vid_is_cvid - o_vid in entry means c_vid not s_vid
  s_vid_enable  - s_vid in entry is valid
  c_vid_enable  - c_vid in entry is valid
  one_2_one_vlan- the entry used for 1:1 vlan
  @brief This structure defines the vlan translation entry.

*/

typedef struct
{
	a_uint32_t o_vid;        /* original vid */
	a_uint32_t s_vid;        /* service vid */
	a_uint32_t c_vid;        /* customer vid */
	a_bool_t   bi_dir;       /* lookup can be forward and reverse */
	a_bool_t   forward_dir;  /* lookup direction only can be from o_vid to s_vid and/or c_vid*/
	a_bool_t   reverse_dir;  /* lookup direction only can be from s_vid and/or c_vid to o_vid*/
	a_bool_t   o_vid_is_cvid;
	a_bool_t   s_vid_enable; /* enable svid value or not */
	a_bool_t   c_vid_enable; /* enable cvid value or not */
	a_bool_t   one_2_one_vlan;

	/*direction check*/
	a_uint32_t trans_direction; /* 0 is for ingress, 1 is for egress */

	/*vsi check*/
	a_bool_t   vsi_valid; /* check if rule will include vsi value valid */
	a_bool_t   vsi_enable; /* check if rule will include vsi value */
	a_uint32_t   vsi; /* vsi value */

	/*protocol and ethernet type*/
	a_bool_t   protocol_enable; /* enable protocol value or not */
	a_uint16_t   protocol; /* protocol value */
	a_bool_t   frmtype_enable; /* enable frame type value or not */
	fal_frametype_t   frmtype; /* frame type value */

	/*tagged mode, bit 0 for untagged, bit 1 for priority tagged and bit 2 for tagged*/
	a_uint8_t   s_tagged; /* stag type is untagged/pri tagged/tagged */
	a_uint8_t   c_tagged; /* ctag type is untagged/pri tagged/tagged */

	/*cpcp and cdei*/
	a_bool_t   c_pcp_enable; /* check if rule will include customer pcp value */
	a_uint8_t   c_pcp; /* customer pcp value */
	a_bool_t   c_dei_enable; /* check if rule will include customer dei value */
	a_uint8_t   c_dei; /* customer dei value */
	/*spcp and sdei*/
	a_bool_t   s_pcp_enable; /* check if rule will include service pcp value */
	a_uint8_t   s_pcp; /* service pcp value */
	a_bool_t   s_dei_enable; /* check if rule will include service dei value */
	a_uint8_t   s_dei; /* service dei value */

	/*translation action*/
	/*counter action*/
	a_bool_t   counter_enable; /* check if action will enable counter_id */
	a_uint8_t   counter_id;  /* counter id */
	/*vsi action*/
	a_bool_t   vsi_action_enable; /* check if action will enable vsi action */
	a_uint8_t   vsi_action; /* vsi action */
	/*svid action*/
	fal_vid_xlt_cmd_t   svid_xlt_cmd; /* check if action will do svid xlt */
	a_uint16_t   svid_xlt; /* svid xlt operation */
	/*cvid action*/
	fal_vid_xlt_cmd_t   cvid_xlt_cmd; /* check if action will do cvid xlt */
	a_uint16_t   cvid_xlt; /* cvid xlt operation */
	/*swap svid and cvid action*/
	a_bool_t   swap_svid_cvid; /* check if action will do svid and cvid swap operation */
	/*spcp action*/
	a_bool_t   spcp_xlt_enable; /* check if action will enable spcp xlt */
	a_uint8_t   spcp_xlt; /* spcp xlt operation */
	/*cpcp action*/
	a_bool_t   cpcp_xlt_enable; /* check if action will enable cpcp xlt */
	a_uint8_t   cpcp_xlt; /* cpcp xlt operation */
	/*swap spcp and cpcp action*/
	a_bool_t   swap_spcp_cpcp; /* check if action will do spcp and cpcp swap operation */
	/*sdei action*/
	a_bool_t   sdei_xlt_enable; /* check if action will enable sdei xlt */
	a_uint8_t   sdei_xlt; /* sdei xlt operation */
	/*cdei action*/
	a_bool_t   cdei_xlt_enable; /* check if action will enable cdei xlt */
	a_uint8_t   cdei_xlt; /* cdei xlt operation */
	/*swap sdei and cdei action*/
	a_bool_t   swap_sdei_cdei; /* check if action will do sdei and cdei swap operation */

	/*port bitmap for this entry*/
	a_uint32_t port_bitmap; /* rule must be include port_bitmap */
} fal_vlan_trans_entry_t;

/**
  @brief This enum defines qinq working mode.
  */
typedef enum
{
	FAL_QINQ_CTAG_MODE = 0,
	FAL_QINQ_STAG_MODE,
	FAL_QINQ_MODE_BUTT
} fal_qinq_mode_t;

/**
  @brief This enum defines port role in qinq mode.
  */
typedef enum
{
	FAL_QINQ_EDGE_PORT = 0,
	FAL_QINQ_CORE_PORT,
	FAL_QINQ_PORT_ROLE_BUTT
} fal_qinq_port_role_t;

/**
  @brief This enum defines which port to use for qinq role.
  */
typedef enum
{
	FAL_QINQ_SEL_TNL_DECAP_SRC_VP = 0,
	FAL_QINQ_SEL_ORG_SRC_PORT,
	FAL_QINQ_SEL_PORT_TYPE_BUTT
} fal_qinq_port_sel_t;

#define FAL_FLG_TST(flag, field) \
    (((flag) & (field)) ? 1 : 0)

#define FAL_GLOBAL_QINQ_MODE_INGRESS_EN (0x1UL << 0)
#define FAL_GLOBAL_QINQ_MODE_EGRESS_EN (0x1UL << 1)
#define FAL_GLOBAL_QINQ_MODE_EGRESS_UNTOUCHED_FOR_CPU_CODE (0x1UL << 2)
	typedef struct {
	    a_uint32_t mask;/*bit 0 for ingress and bit 1 for egress*/
	    fal_qinq_mode_t ingress_mode; /* ingress direction mode */
	    fal_qinq_mode_t egress_mode; /* egress direction mode */
	    a_bool_t untouched_for_cpucode; /*egress untouched with cpu_code!=0 to cpu port 0 */
	} fal_global_qinq_mode_t;

#define FAL_PORT_QINQ_ROLE_INGRESS_EN (0x1UL << 0)
#define FAL_PORT_QINQ_ROLE_EGRESS_EN (0x1UL << 1)
#define FAL_PORT_QINQ_ROLE_TUNNEL_EN (0x1UL << 2)
typedef struct {
	a_uint32_t mask;/*bit 0 for ingress and bit 1 for egress*/
	fal_qinq_port_role_t ingress_port_role; /* port inress direction role */
	fal_qinq_port_role_t egress_port_role; /* port egress direction role */
	fal_qinq_port_role_t tunnel_port_role; /* tunnel ingress parse role
						* added for ipq95xx */
	fal_qinq_port_sel_t ingress_port_sel; /*which port used for ingress_port_role in tunnel decap scenario.
						* 0 is used to selcet srcvp of tunnel decap, 1 always selet original source port.
						* default: 0, will use srcvp of tunnel decap.
						* added for ipq5332*/
	a_bool_t in_port_ponmode_en; /* enable pon mode on ingress port. when pon mode enable, ppe would treat tag
						* to be inner tag(ctag) if there is only one tag parsed(core port + tpid match, would
						* treat single tag to be stag when pon mode disable).
						* added for ipq52xx & ipq96xx */
	a_bool_t tl_port_ponmode_en; /* enable pon mode on tunnel parsing port.
						* added for ipq52xx & ipq96xx */
} fal_port_qinq_role_t;

#define FAL_TPID_CTAG_EN (0x1UL << 0)
#define FAL_TPID_STAG_EN (0x1UL << 1)
#define FAL_EXT_TPID_CTAG_EN (0x1UL << 2)
#define FAL_EXT_TPID_STAG_EN (0x1UL << 3)
#define FAL_TUNNEL_TPID_CTAG_EN (0x1UL << 4)
#define FAL_TUNNEL_TPID_STAG_EN (0x1UL << 5)
#define FAL_EXT_TUNNEL_TPID_CTAG_EN (0x1UL << 6)
#define FAL_EXT_TUNNEL_TPID_STAG_EN (0x1UL << 7)
#define FAL_TPID_CTAG_MAP_EN (0x1UL << 8)
#define FAL_TPID_STAG_MAP_EN (0x1UL << 9)
#define FAL_TUNNEL_TPID_CTAG_MAP_EN (0x1UL << 10)
#define FAL_TUNNEL_TPID_STAG_MAP_EN (0x1UL << 11)
#define TPID_SIZE 4
typedef struct
{
      a_uint32_t mask; /* bit 0 for ctpid and bit 1 for stpid
                    * bit 2 for extra ctpid, bit 3 for extra stpid
                    * bit 4 for tunnel ctpid, bit 5 for tunnel stpid
                    * bit 6 for extra tunnel ctpid, bit 7 for extra tunnel stpid
                    * bit 8 for ctpid bitmap, bit 9 for stpid bitmap
                    * bit 10 for tunnel ctpid bitmap, bit 11 for tunnel stpid bitmap
                    * */

      union {
            struct {
                  a_uint16_t ctpid; /* customer tpid value, idx 0 for ipq52xx & ipq96xx */
                  a_uint16_t stpid; /* service tpid value, idx 1 for ipq52xx & ipq96xx */
                  a_uint16_t ext_ctpid; /* extra customer tpid value, idx 2 for ipq52xx & ipq96xx */
                  a_uint16_t ext_stpid; /* extra service tpid value, idx 3 for ipq52xx & ipq96xx */
            };
            a_uint16_t tpid_arr[TPID_SIZE]; /* for get tpid_index by tpid */
      };

      a_uint8_t ctpid_map; /* bitmap for ctpid, set when any tpid configured */
      a_uint8_t stpid_map; /* bitmap for stpid, set when any tpid configured */

      union {
            struct {
                  a_uint16_t tunnel_ctpid; /* tunnel customer tpid value, added for ipq95xx, idx 0 for ipq52xx & ipq96xx */
                  a_uint16_t tunnel_stpid; /* tunnel service tpid value, added for ipq95xx, idx 1 for ipq52xx & ipq96xx */
                  a_uint16_t ext_tunnel_ctpid; /* extra tunnel customer tpid value, added for ipq95xx, idx 2 for ipq52xx & ipq96xx */
                  a_uint16_t ext_tunnel_stpid; /* extra tunnel service tpid value, added for ipq95xx, idx 3 for ipq52xx & ipq96xx */
            };
            a_uint16_t tunnel_tpid_arr[TPID_SIZE]; /* for get tpid_index by tpid */
      };

      a_uint8_t tunnel_ctpid_map; /* bitmap for tunnel ctpid, set when any tunnel tpid configured */
      a_uint8_t tunnel_stpid_map; /* bitmap for tunnel stpid, set when any tunnel tpid configured */
} fal_tpid_t;

typedef struct {
	a_bool_t membership_filter; /* check if ingress will filter port in vlan filter */
	a_bool_t tagged_filter; /* check if ingress will filter tagged packet */
	a_bool_t untagged_filter; /* check if ingress will filter untagged packet */
	a_bool_t priority_filter; /* check if ingress will filter priority packet */
	a_bool_t ctag_tagged_filter; /* ingress filter ctag tagged packet or not,
				      * added for ipq95xx */
	a_bool_t ctag_untagged_filter; /* ingress filter ctag untagged packet or not,
					* added for ipq95xx */
	a_bool_t ctag_priority_filter; /* ingress filter ctag priority packet or not,
					* added for ipq95xx */
} fal_ingress_vlan_filter_t;

#define FAL_PORT_VLAN_TAG_CVID_EN (0x1UL << 0)
#define FAL_PORT_VLAN_TAG_SVID_EN (0x1UL << 1)
#define FAL_PORT_VLAN_TAG_CPCP_EN (0x1UL << 2)
#define FAL_PORT_VLAN_TAG_SPCP_EN (0x1UL << 3)
#define FAL_PORT_VLAN_TAG_CDEI_EN (0x1UL << 4)
#define FAL_PORT_VLAN_TAG_SDEI_EN (0x1UL << 5)
typedef struct
{
	a_uint32_t mask;	/*bit 0 for ctag vid;
				 *bit 1 for stag vid;
				 *bit 2 for ctag priority;
				 *bit 3 for stag priority;
				 *bit 4 for ctag dei;
				 *bit 5 for stag dei*/
	a_uint16_t cvid; /* customer vid value */
	a_uint16_t svid; /* service vid value */
	a_uint16_t cpri; /* customer pri value */
	a_uint16_t spri; /* service pri value */
	a_uint16_t cdei; /* customer dei value */
	a_uint16_t sdei; /* service dei value */
} fal_port_vlan_tag_t;

typedef struct {
	a_bool_t default_cvid_en; /* enable default cvid or not */
	a_bool_t default_svid_en; /* enable default svid or not */
} fal_port_default_vid_enable_t;

#define FAL_PORT_PROPAGATION_VID_EN (0x1UL << 0)
#define FAL_PORT_PROPAGATION_PCP_EN (0x1UL << 1)
#define FAL_PORT_PROPAGATION_DEI_EN (0x1UL << 2)
typedef struct
{
	a_uint32_t mask; /*bit 0 for vid;
			  *bit 1 for priority;
			  *bit 2 for dei*/
	fal_vlan_propagation_mode_t vid_propagation; /* enable vid propagation or not */
	fal_vlan_propagation_mode_t pri_propagation; /* enable pri propagation or not */
	fal_vlan_propagation_mode_t dei_propagation; /* enable dei propagation or not */
} fal_vlantag_propagation_t;

#define FAL_EGRESSMODE_CTAG_EN (0x1UL << 0)
#define FAL_EGRESSMODE_STAG_EN (0x1UL << 1)
typedef struct
{
	a_uint32_t mask; /*bit 0 for ctag and bit 1 for stag*/
	fal_pt_1q_egmode_t stag_mode; /* stag mode */
	fal_pt_1q_egmode_t ctag_mode; /* ctag mode */
} fal_vlantag_egress_mode_t;

typedef struct
{
	fal_pbmp_t port_bitmap; /* rule need know which ports matched this rule */
	a_uint8_t s_tagged; /* rule need know stag type(bit0-untagged/bit1-pri_tagged/bit2-tagged) */
	a_bool_t s_vid_enable; /* check if rule will include service vid value */
	a_uint32_t s_vid; /* service vid */
	a_bool_t s_pcp_enable; /* check if rule will include service pcp value */
	a_uint8_t s_pcp; /* service pcp value */
	a_bool_t s_dei_enable; /* check if rule will include service dei value */
	a_uint8_t s_dei; /* service dei value */
	a_uint8_t c_tagged; /* rule need know ctag type(bit0-untagged/bit1-pri_tagged/bit2-tagged) */
	a_bool_t c_vid_enable; /* check if rule will include customer vid value */
	a_uint32_t c_vid; /* customer vid */
	a_bool_t c_pcp_enable; /* check if rule will include customer pcp value */
	a_uint8_t c_pcp; /* customer pcp value */
	a_bool_t c_dei_enable; /* check if rule will include customer dei value */
	a_uint8_t c_dei; /* customert dei value */
	a_bool_t protocol_enable; /* check if rule will include protocol value,
							*contained both in hmsppe inxlt and egxlt */
	a_uint16_t protocol; /* protocol value,
							*contained both in hmsppe inxlt and egxlt */

	/* these two fields just for vlan ingress rule */
	a_bool_t frmtype_enable; /* check if rule will include frame type value */
	fal_frametype_t	frmtype; /* frame type value */

	/* these three fields just for vlan egress rule */
	a_bool_t vsi_valid; /* check if rule will include vsi value valid */
	a_bool_t vsi_enable; /* check if rule will include vsi value */
	a_uint32_t vsi; /* vsi value */

	/*vni fields match for ingress added by appe*/
	a_bool_t vni_resv_enable; /* check vni resv or not, added for ipq95xx */
	a_uint8_t vni_resv_type;  /* 0 for vni only, 1 for vni and reserver,
				   * added for ipq95xx
				   * */
	a_uint32_t vni_resv; /* vni or gre key filed value, added for ipq95xx*/

	/* added for hmsppe */
	a_bool_t stpid_idx_en; /* check if rule will include stpid index */
	a_uint8_t stpid_idx; /* stpid index */
	a_bool_t ctpid_idx_en; /* check if rule will include ctpid index */
	a_uint8_t ctpid_idx; /* ctpid index */

	a_uint8_t dhcp_type; /* Bit 0 = Non-DHCP ,Bit 1 = DHCP v4, Bit 2 = DHCP v6 */
	a_uint8_t mc_type; /* Bit 0 = Non_MC, Bit 1 = IP_MC, Bit 2 = NONIP_MC */

	a_uint32_t index; /* output, indicate the entry index*/
} fal_vlan_trans_adv_rule_t;

typedef struct
{
	a_bool_t swap_svid_cvid; /* check if action will do svid and cvid swap operation */
	fal_vid_xlt_cmd_t svid_xlt_cmd; /* check if action will do svid xlt operation */
	a_uint16_t svid_xlt; /* service vid xlt value */
	fal_vid_xlt_cmd_t cvid_xlt_cmd; /* check if action will do cvid xlt operation */
	a_uint16_t cvid_xlt; /* customer vid xlt value */
	a_bool_t swap_spcp_cpcp; /* check if action will do spcp and cpcp swap operation */
	fal_pcp_xlt_cmd_t spcp_xlt_cmd; /* check if action will do spcp xlt operation */
	a_uint8_t spcp_xlt; /* service pcp xlt value */
	fal_pcp_xlt_cmd_t cpcp_xlt_cmd; /* check if action will do cpcp xlt operation */
	a_uint8_t cpcp_xlt; /* customer pcp xlt value */
	a_bool_t swap_sdei_cdei; /* check if action will do sdei and cdei swap operation */
	fal_dei_xlt_cmd_t sdei_xlt_cmd; /* check if action will do sdei xlt operation */
	a_uint8_t sdei_xlt; /* service dei xlt value */
	fal_dei_xlt_cmd_t cdei_xlt_cmd; /* check if action will do cdei xlt operation */
	a_uint8_t cdei_xlt; /* customer dei xlt value */
	a_bool_t counter_enable; /* check if action will enable counter_id */
	a_uint8_t counter_id;  /* counter id */
	a_uint8_t counter_mode;  /* counter mode, add for hmsppe, 0 vlan counter tbl, 1 pon pm tbl */
	/* these two fields just for vlan ingress action */
	a_bool_t vsi_xlt_enable; /* check if action will enable vsi xlt */
	a_uint8_t vsi_xlt; /* vsi xlt value */
	/*src info fields for ingress added by appe*/
	a_bool_t src_info_enable; /* src info xlt or not, added for ipq95xx*/
	a_uint8_t src_info_type; /* 0 virtual port, 1 l3_if for tunnel payload,
				  * added for ipq95xx */
	a_uint32_t src_info; /* src info value, added for ipq95xx */
	/*vni fields for egress added by appe*/
	a_bool_t vni_resv_enable; /* vni xlat or not, added for ipq95xx*/
	a_uint32_t vni_resv;	/* vni xlt value, added for ipq95xx*/

	/* added for hmsppe */
	a_uint8_t tags_to_rm; /* 0,1,2, means to remove how many tags */
	fal_tpid_idx_xlt_cmd_t stpid_idx_xlt_cmd; /* check if action will do stpid idx xlt operation */
	a_uint8_t stpid_idx_xlt; /* stpid idx xlt value */
	fal_tpid_idx_xlt_cmd_t ctpid_idx_xlt_cmd; /* check if action will do ctpid idx xlt operation */
	a_uint8_t ctpid_idx_xlt; /* ctpid idx xlt value */
	a_uint8_t dscp_map_idx;	/* used to choose pcp value by index in IN_VLAN_DSCP_PBIT_MAP_TBL */
	fal_fwd_cmd_t fwd_cmd; /* action command, egress xlt only supports forward and drop */

	/* ingress xlt action for hmsppe */
	a_bool_t svc_code_en; /* check if action will enable service code xlt */
	a_uint8_t svc_code; /* service code xlt value */
	a_bool_t dst_valid; /* check if action will generate dest info */
	fal_dest_info_t dst_port; /* dest info, physical port or virtual port */
} fal_vlan_trans_adv_action_t;

#define FAL_PORTVLAN_TRANS_TBL_NUM(dir) \
    ((dir == FAL_PORT_VLAN_INGRESS) ? (XLT_RULE_TBL_NUM) : (EG_VLAN_XLT_RULE_NUM))

typedef struct
{
	a_uint32_t rx_packet_counter; /* ingress vlan translation pkt counter */
	a_uint64_t rx_byte_counter; /* ingress vlan translation byte counter */
	a_uint32_t tx_packet_counter; /* egress vlan translation pkt counter */
	a_uint64_t tx_byte_counter; /* egress vlan translation byte counter */
} fal_port_vlan_counter_t;

typedef struct {
	fal_direction_t dir; 		/* ipq52xx & ipq96xx, ingress <pre> or egress <post> for pre vp and post vp table */
	a_bool_t enable;	 		/* enable or not */
	a_uint8_t isol_group_id; 	/* isolation group id */
} fal_port_isol_ctrl_t;

typedef struct {
	a_bool_t bc_isol_en; /* isolation action enable for broadcast traffic */
	a_bool_t mc_isol_en; /* isolation action enable for multicast traffic */
} fal_port_isol_act_ctrl_t;

#define FAL_ISOL_ACT_ARR_SIZE 4

typedef struct {
	a_uint32_t act_bitmap[FAL_ISOL_ACT_ARR_SIZE];
} fal_port_isol_act_t;

typedef enum {
	FAL_ISOL_ACT_PPORT = 0,      /* isol action based on physical port */
	FAL_ISOL_ACT_GROUP,          /* isol action based on isolation profile(group) associating with VP */
	FAL_ISOL_ACT_BUTT            /* MAX check */
} fal_port_isol_act_type_t;

typedef struct {
	fal_port_isol_act_type_t isol_type;

	union {
		a_uint32_t pport_id;          /* physical port id */
		a_uint32_t isol_group_id;     /* isolation group id */
	};
} fal_port_isol_act_idx_t;

typedef struct {
	a_bool_t map_en; /* enable parsing private vlan */
	a_uint16_t tpid; /* private vlan TPID */
} fal_pvtvlan_rx_cfg_t;

typedef struct {
	a_bool_t ptmap_en; /* enable translate between private vlan and internal port,
			* parsing and remove private vlan to internal port at ingress, inserting
			* private vlan when egress on the internal port. */

	a_uint16_t vlan_tci; /* tag control info */
	a_uint16_t vlan_tci_mask; /* tag control info mask, effective for ingress parsing only */

	a_uint8_t int_port;/* internal port, pp or vp */
} fal_pvtvlan_map_t;

typedef struct {
	a_uint16_t tpid; /* private vlan TPID */
} fal_pvtvlan_tx_cfg_t;

sw_error_t
fal_private_vlan_rx_cfg_set(a_uint32_t dev_id, fal_port_t port_id, fal_pvtvlan_rx_cfg_t *cfg);

sw_error_t
fal_private_vlan_rx_cfg_get(a_uint32_t dev_id, fal_port_t port_id, fal_pvtvlan_rx_cfg_t *cfg);

sw_error_t
fal_private_vlan_mapping_set(a_uint32_t dev_id,
		fal_direction_t direction, fal_pvtvlan_map_t *port_mapping);

sw_error_t
fal_private_vlan_mapping_get(a_uint32_t dev_id,
		fal_direction_t direction, fal_pvtvlan_map_t *port_mapping);

sw_error_t
fal_private_vlan_tx_cfg_set(a_uint32_t dev_id, fal_pvtvlan_tx_cfg_t *cfg);

sw_error_t
fal_private_vlan_tx_cfg_get(a_uint32_t dev_id, fal_pvtvlan_tx_cfg_t *cfg);

sw_error_t
fal_port_isol_ctrl_set(a_uint32_t dev_id,
		fal_port_t port_id, fal_port_isol_ctrl_t *isol_ctrl);
sw_error_t
fal_port_isol_ctrl_get(a_uint32_t dev_id,
		fal_port_t port_id, fal_port_isol_ctrl_t *isol_ctrl);

sw_error_t
fal_port_isol_group_set(a_uint32_t dev_id,
		a_uint8_t isol_group_id, a_uint64_t *isol_group_bmp);
sw_error_t
fal_port_isol_group_get(a_uint32_t dev_id,
		a_uint8_t isol_group_id, a_uint64_t *isol_group_bmp);

sw_error_t
fal_port_isol_action_ctrl_set(a_uint32_t dev_id, fal_port_isol_act_ctrl_t *act_ctrl);
sw_error_t
fal_port_isol_action_ctrl_get(a_uint32_t dev_id, fal_port_isol_act_ctrl_t *act_ctrl);

sw_error_t
fal_port_isol_action_set(a_uint32_t dev_id,
		fal_port_isol_act_idx_t *isol_id, fal_port_isol_act_t *isol_act);
sw_error_t
fal_port_isol_action_get(a_uint32_t dev_id,
		fal_port_isol_act_idx_t *isol_id, fal_port_isol_act_t *isol_act);

sw_error_t fal_port_isol_member_set(a_uint32_t dev_id, fal_port_t port_id, fal_pbmp_t mem_port_map);
sw_error_t fal_port_isol_member_get(a_uint32_t dev_id, fal_port_t port_id, fal_pbmp_t *mem_port_map);

typedef struct {
	a_bool_t membership_filter; /* membership filter or not for vport */
} fal_egress_vlan_filter_t;

sw_error_t
fal_global_qinq_mode_set(a_uint32_t dev_id, fal_global_qinq_mode_t *mode);

sw_error_t
fal_global_qinq_mode_get(a_uint32_t dev_id, fal_global_qinq_mode_t *mode);

sw_error_t
fal_port_qinq_mode_set(a_uint32_t dev_id, fal_port_t port_id, fal_port_qinq_role_t *mode);

sw_error_t
fal_port_qinq_mode_get(a_uint32_t dev_id, fal_port_t port_id, fal_port_qinq_role_t *mode);

sw_error_t
fal_port_qinq_role_set(a_uint32_t dev_id, fal_port_t port_id, fal_qinq_port_role_t role);

sw_error_t
fal_port_qinq_role_get(a_uint32_t dev_id, fal_port_t port_id, fal_qinq_port_role_t * role);

sw_error_t
fal_qinq_mode_set(a_uint32_t dev_id, fal_qinq_mode_t mode);

sw_error_t
fal_qinq_mode_get(a_uint32_t dev_id, fal_qinq_mode_t * mode);

sw_error_t
fal_port_vsi_egmode_set(a_uint32_t dev_id, a_uint32_t vsi,
		a_uint32_t port_id, fal_pt_1q_egmode_t egmode);

sw_error_t
fal_port_vsi_egmode_get(a_uint32_t dev_id, a_uint32_t vsi,
		a_uint32_t port_id, fal_pt_1q_egmode_t * egmode);

sw_error_t
fal_ingress_tpid_set(a_uint32_t dev_id, fal_tpid_t *tpid);

sw_error_t
fal_ingress_tpid_get(a_uint32_t dev_id, fal_tpid_t *tpid);

sw_error_t
fal_egress_tpid_set(a_uint32_t dev_id, fal_tpid_t *tpid);

sw_error_t
fal_egress_tpid_get(a_uint32_t dev_id, fal_tpid_t *tpid);

#ifndef IN_PORTVLAN_MINI
sw_error_t
fal_port_vlan_trans_add(a_uint32_t dev_id, fal_port_t port_id, fal_vlan_trans_entry_t *entry);

sw_error_t
fal_port_vlan_trans_del(a_uint32_t dev_id, fal_port_t port_id, fal_vlan_trans_entry_t *entry);

sw_error_t
fal_port_vlan_trans_get(a_uint32_t dev_id, fal_port_t port_id, fal_vlan_trans_entry_t *entry);

sw_error_t
fal_port_vlan_trans_iterate(a_uint32_t dev_id, fal_port_t port_id,
		a_uint32_t * iterator, fal_vlan_trans_entry_t * entry);

sw_error_t
fal_port_egress_vlan_filter_set(a_uint32_t dev_id,
		fal_port_t port_id, fal_egress_vlan_filter_t *filter);
sw_error_t
fal_port_egress_vlan_filter_get(a_uint32_t dev_id,
		fal_port_t port_id, fal_egress_vlan_filter_t *filter);

sw_error_t
fal_port_vlan_counter_get(a_uint32_t dev_id, a_uint32_t cnt_index,
		fal_port_vlan_counter_t * counter);
sw_error_t
fal_port_vlan_counter_cleanup(a_uint32_t dev_id, a_uint32_t cnt_index);

sw_error_t
fal_port_tag_propagation_set(a_uint32_t dev_id, fal_port_t port_id,
		fal_port_vlan_direction_t direction,
		fal_vlantag_propagation_t *prop);

sw_error_t
fal_port_tag_propagation_get(a_uint32_t dev_id, fal_port_t port_id,
		fal_port_vlan_direction_t direction,
		fal_vlantag_propagation_t *prop);
#endif

sw_error_t
fal_port_vlan_vpgroup_set(a_uint32_t dev_id, a_uint32_t vport,
		fal_port_vlan_direction_t direction, a_uint32_t vpgroup_id);

sw_error_t
fal_port_vlan_vpgroup_get(a_uint32_t dev_id, a_uint32_t vport,
		fal_port_vlan_direction_t direction, a_uint32_t *vpgroup_id);

sw_error_t
fal_port_vlan_trans_adv_add(a_uint32_t dev_id, fal_port_t port_id,
		fal_port_vlan_direction_t direction,
		fal_vlan_trans_adv_rule_t * rule, fal_vlan_trans_adv_action_t * action);

sw_error_t
fal_port_vlan_trans_adv_del(a_uint32_t dev_id, fal_port_t port_id,
		fal_port_vlan_direction_t direction,
		fal_vlan_trans_adv_rule_t * rule, fal_vlan_trans_adv_action_t * action);

sw_error_t
fal_port_vlan_trans_adv_get(a_uint32_t dev_id, fal_port_vlan_direction_t direction,
		a_uint32_t index,
		fal_vlan_trans_adv_rule_t * rule, fal_vlan_trans_adv_action_t * action);

sw_error_t
fal_port_vlan_trans_adv_set(a_uint32_t dev_id, fal_port_vlan_direction_t direction,
		a_uint32_t index,
		fal_vlan_trans_adv_rule_t * rule, fal_vlan_trans_adv_action_t * action);

sw_error_t
fal_port_vlan_trans_adv_getfirst(a_uint32_t dev_id, fal_port_t port_id,
		fal_port_vlan_direction_t direction,
		fal_vlan_trans_adv_rule_t * rule, fal_vlan_trans_adv_action_t * action);

sw_error_t
fal_port_vlan_trans_adv_getnext(a_uint32_t dev_id, fal_port_t port_id,
		fal_port_vlan_direction_t direction,
		fal_vlan_trans_adv_rule_t * rule, fal_vlan_trans_adv_action_t * action);

sw_error_t
fal_portvlan_member_update(a_uint32_t dev_id, fal_port_t port_id,
		fal_pbmp_t mem_port_map);

sw_error_t
fal_portvlan_member_get(a_uint32_t dev_id, fal_port_t port_id,
		fal_pbmp_t * mem_port_map);

sw_error_t
fal_port_vlantag_egmode_set(a_uint32_t dev_id, fal_port_t port_id,
		fal_vlantag_egress_mode_t *port_egvlanmode);

sw_error_t
fal_port_vlantag_egmode_get(a_uint32_t dev_id, fal_port_t port_id,
		fal_vlantag_egress_mode_t *port_egvlanmode);

sw_error_t
fal_port_vlantag_vsi_egmode_enable(a_uint32_t dev_id,
		fal_port_t port_id, a_bool_t enable);

sw_error_t
fal_port_vlantag_vsi_egmode_status_get(a_uint32_t dev_id,
		fal_port_t port_id, a_bool_t * enable);

sw_error_t
fal_port_vlan_xlt_miss_cmd_set(a_uint32_t dev_id, fal_port_t port_id,
		fal_fwd_cmd_t cmd);

sw_error_t
fal_port_vlan_xlt_miss_cmd_get(a_uint32_t dev_id, fal_port_t port_id,
		fal_fwd_cmd_t *cmd);

sw_error_t
fal_port_default_vlantag_get(a_uint32_t dev_id, fal_port_t port_id,
		fal_port_vlan_direction_t direction,
		fal_port_default_vid_enable_t *default_vid_en,
		fal_port_vlan_tag_t *default_tag);

sw_error_t
fal_port_default_vlantag_set(a_uint32_t dev_id, fal_port_t port_id,
		fal_port_vlan_direction_t direction,
		fal_port_default_vid_enable_t *default_vid_en,
		fal_port_vlan_tag_t *default_tag);

sw_error_t
fal_portvlan_member_add(a_uint32_t dev_id, fal_port_t port_id,
		fal_port_t mem_port_id);

sw_error_t
fal_portvlan_member_del(a_uint32_t dev_id, fal_port_t port_id,
		fal_port_t mem_port_id);

sw_error_t
fal_port_invlan_mode_set(a_uint32_t dev_id, fal_port_t port_id,
		fal_pt_invlan_mode_t mode);

sw_error_t
fal_port_invlan_mode_get(a_uint32_t dev_id, fal_port_t port_id,
		fal_pt_invlan_mode_t * mode);

sw_error_t
fal_port_ingress_vlan_filter_set(a_uint32_t dev_id, fal_port_t port_id,
		fal_ingress_vlan_filter_t *filter);

sw_error_t
fal_port_ingress_vlan_filter_get(a_uint32_t dev_id, fal_port_t port_id,
		fal_ingress_vlan_filter_t *filter);

sw_error_t
fal_vlan_trans_dscp_pcp_mapping_set(a_uint32_t dev_id, fal_port_vlan_direction_t direction,
		a_uint8_t group_id, a_uint8_t dscp, a_uint8_t pcp);
sw_error_t
fal_vlan_trans_dscp_pcp_mapping_get(a_uint32_t dev_id, fal_port_vlan_direction_t direction,
		a_uint8_t group_id, a_uint8_t dscp, a_uint8_t *pcp);
#ifdef __cplusplus
}
#endif                          /* __cplusplus */
#endif                          /* _PORT_VLAN_H_ */
/**
 * @}
 */
