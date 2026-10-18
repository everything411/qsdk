/*
 * Copyright (c) 2012, 2016-2018, 2021, The Linux Foundation. All rights reserved.
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: ISC
 */

/**
 * @defgroup fal_sec FAL_SEC
 * @{
 */
#ifndef _FAL_SEC_H_
#define _FAL_SEC_H_

#ifdef __cplusplus
extern "C" {
#endif                          /* __cplusplus */

#include "sw.h"
#include "fal/fal_type.h"
#include "fal_tunnel.h"
#include "fal_sec_legacy.h"

/* Exception: L2 */
#define FAL_SEC_EXP_UNKNOWN_L2_PROT                  0
#define FAL_SEC_EXP_PPPOE_WRONG_VER_TYPE             1
#define FAL_SEC_EXP_PPPOE_WRONG_CODE                 2
#define FAL_SEC_EXP_PPPOE_UNSUPPORTED_PPP_PROT       3
/* Exception: IPv4 */
#define FAL_SEC_EXP_IPV4_WRONG_VER                   4
#define FAL_SEC_EXP_IPV4_SMALL_IHL                   5
#define FAL_SEC_EXP_IPV4_WITH_OPTION                 6
#define FAL_SEC_EXP_IPV4_HDR_INCOMPLETE              7
#define FAL_SEC_EXP_IPV4_BAD_TOTAL_LEN               8
#define FAL_SEC_EXP_IPV4_DATA_INCOMPLETE             9
#define FAL_SEC_EXP_IPV4_FRAG                        10
#define FAL_SEC_EXP_IPV4_PING_OF_DEATH               11
#define FAL_SEC_EXP_IPV4_SMALL_TTL                   12
#define FAL_SEC_EXP_IPV4_UNK_IP_PROT                 13
#define FAL_SEC_EXP_IPV4_CHECKSUM_ERR                14
#define FAL_SEC_EXP_IPV4_INV_SIP                     15
#define FAL_SEC_EXP_IPV4_INV_DIP                     16
#define FAL_SEC_EXP_IPV4_LAND_ATTACK                 17
#define FAL_SEC_EXP_IPV4_AH_HDR_INCOMPLETE           18
#define FAL_SEC_EXP_IPV4_AH_HDR_CROSS_BORDER         19
#define FAL_SEC_EXP_IPV4_ESP_HDR_INCOMPLETE          20
/* Exception: IPv6 */
#define FAL_SEC_EXP_IPV6_WRONG_VER                   21
#define FAL_SEC_EXP_IPV6_HDR_INCOMPLETE              22
#define FAL_SEC_EXP_IPV6_BAD_PAYLOAD_LEN             23
#define FAL_SEC_EXP_IPV6_DATA_INCOMPLETE             24
#define FAL_SEC_EXP_IPV6_WITH_EXT_HDR                25
#define FAL_SEC_EXP_IPV6_SMALL_HOP_LIMIT             26
#define FAL_SEC_EXP_IPV6_INV_SIP                     27
#define FAL_SEC_EXP_IPV6_INV_DIP                     28
#define FAL_SEC_EXP_IPV6_LAND_ATTACK                 29
#define FAL_SEC_EXP_IPV6_FRAG                        30
#define FAL_SEC_EXP_IPV6_PING_OF_DEATH               31
#define FAL_SEC_EXP_IPV6_WITH_MORE_EXT_HDR           32
#define FAL_SEC_EXP_IPV6_UNK_LAST_NEXT_HDR           33
#define FAL_SEC_EXP_IPV6_MOBILITY_HDR_INCOMPLETE     34
#define FAL_SEC_EXP_IPV6_MOBILITY_HDR_CROSS_BORDER   35
#define FAL_SEC_EXP_IPV6_AH_HDR_INCOMPLETE           36
#define FAL_SEC_EXP_IPV6_AH_HDR_CROSS_BORDER         37
#define FAL_SEC_EXP_IPV6_ESP_HDR_INCOMPLETE          38
#define FAL_SEC_EXP_IPV6_ESP_HDR_CROSS_BORDER        39
#define FAL_SEC_EXP_IPV6_OTHER_EXT_HDR_INCOMPLETE    40
#define FAL_SEC_EXP_IPV6_OTHER_EXT_HDR_CROSS_BORDER  41
/* Exception: L4 */
#define FAL_SEC_EXP_TCP_HDR_INCOMPLETE               42
#define FAL_SEC_EXP_TCP_HDR_CROSS_BORDER             43
#define FAL_SEC_EXP_TCP_SMAE_SP_DP                   44
#define FAL_SEC_EXP_TCP_SMALL_DATA_OFFSET            45
#define FAL_SEC_EXP_TCP_FLAGS_0                      46
#define FAL_SEC_EXP_TCP_FLAGS_1                      47
#define FAL_SEC_EXP_TCP_FLAGS_2                      48
#define FAL_SEC_EXP_TCP_FLAGS_3                      49
#define FAL_SEC_EXP_TCP_FLAGS_4                      50
#define FAL_SEC_EXP_TCP_FLAGS_5                      51
#define FAL_SEC_EXP_TCP_FLAGS_6                      52
#define FAL_SEC_EXP_TCP_FLAGS_7                      53
#define FAL_SEC_EXP_TCP_CHECKSUM_ERR                 54
#define FAL_SEC_EXP_UDP_HDR_INCOMPLETE               55
#define FAL_SEC_EXP_UDP_HDR_CROSS_BORDER             56
#define FAL_SEC_EXP_UDP_SMAE_SP_DP                   57
#define FAL_SEC_EXP_UDP_BAD_LEN                      58
#define FAL_SEC_EXP_UDP_DATA_INCOMPLETE              59
#define FAL_SEC_EXP_UDP_CHECKSUM_ERR                 60
#define FAL_SEC_EXP_UDP_LITE_HDR_INCOMPLETE          61
#define FAL_SEC_EXP_UDP_LITE_HDR_CROSS_BORDER        62
#define FAL_SEC_EXP_UDP_LITE_SMAE_SP_DP              63
#define FAL_SEC_EXP_UDP_LITE_CSM_COV_1_TO_7          64
#define FAL_SEC_EXP_UDP_LITE_CSM_COV_TOO_LONG        65
#define FAL_SEC_EXP_UDP_LITE_CSM_COV_CROSS_BORDER    66
#define FAL_SEC_EXP_UDP_LITE_CHECKSUM_ERR            67
/*Exception:  Reserved */
#define FAL_SEC_EXP_TUNNEL_DECAP_INNER_IPV4_PADDING  68
#define FAL_SEC_EXP_TUNNEL_DECAP_INNER_IPV6_PADDING  69
/* Exception:  Tunnel */
#define FAL_SEC_EXP_TUNNEL_DECAP_ECN                 70
#define FAL_SEC_EXP_INNER_PACKET_TOO_SHORT           71
#define FAL_SEC_EXP_VXLAN_HDR                        72
#define FAL_SEC_EXP_VXLAN_GPE_HDR                    73
#define FAL_SEC_EXP_GENEVE_HDR                       74
#define FAL_SEC_EXP_GRE_HDR                          75
#define FAL_SEC_EXP_GRE_CHECKSUM_ERR                 76
#define FAL_SEC_EXP_UNKNOWN_INNER_TYPE               77
#define FAL_SEC_EXP_FLAG_VXLAN                       78
#define FAL_SEC_EXP_FLAG_VXLAN_GPE                   79
#define FAL_SEC_EXP_FLAG_GRE                         80
#define FAL_SEC_EXP_FLAG_GENEVE                      81
#define FAL_SEC_EXP_PROGRAM0                         82
#define FAL_SEC_EXP_PROGRAM1                         83
#define FAL_SEC_EXP_PROGRAM2                         84
#define FAL_SEC_EXP_PROGRAM3                         85
#define FAL_SEC_EXP_PROGRAM4                         86
#define FAL_SEC_EXP_PROGRAM5                         87

#define FAL_SEC_L2_EXP_TUNL_CONTEXT_INVALID          0

typedef struct {
    fal_fwd_cmd_t cmd; /* action for the exception */
} fal_l2_excep_ctrl_t;

typedef enum
{
    FAL_FLOW_AWARE = 0,        /* flow aware, both hit and miss flow table*/
    FAL_FLOW_HIT = 1,          /* flow table match */
    FAL_FLOW_MISS = 2,         /* flow table miss */
    FAL_FLOW_EXP_TYPE_BUTT,
} fal_flow_excep_type_t;

typedef struct {
    fal_fwd_cmd_t cmd; /* action for the exception */
    a_bool_t deacclr_en; /* 0 for disable and 1 for enable */
    a_bool_t l3route_only_en; /*host/network route 0: disable and 1: enable*/
    a_bool_t l2fwd_only_en; /*l2 forward 0: disable and 1: enable*/
    a_bool_t l3flow_en; /* 0 for disable and 1 for enable */
    a_bool_t l2flow_en; /* 0 for disable and 1 for enable */
    a_bool_t multicast_en; /* 0 for disable and 1 for enable */
    fal_flow_excep_type_t l3flow_type;
        /* combination with l3flow_en to subdivide flow packet */
    fal_flow_excep_type_t l2flow_type;
        /* combination with l2flow_en to subdivide flow packet */
} fal_l3_excep_ctrl_t;

typedef struct {
    a_uint8_t small_ip4ttl; /* small ttl value checking */
    a_uint8_t small_ip6hoplimit; /*small hoplimit value for check*/
} fal_l3_excep_parser_ctrl;

#define TCP_FLAGS_MAX  8
typedef struct {
    a_uint8_t tcp_flags[TCP_FLAGS_MAX]; /*flag for exception*/
    a_uint8_t tcp_flags_mask[TCP_FLAGS_MAX]; /*flag mask*/
} fal_l4_excep_parser_ctrl;

#define EXP_PROFILE_ID_MAX  4
typedef struct {
    fal_fwd_cmd_t cmd; /* action for the exception */
    a_bool_t deacclr_en; /* 0: disable and 1: enable */
    a_bool_t profile_exp_en[EXP_PROFILE_ID_MAX];
          /* profileID(0~3) exception 0: disable and 1: enable */
} fal_tunnel_excep_ctrl_t;

typedef struct {
  a_bool_t entry_valid; /* 0 for Invalid and 1 for Valid */
  a_uint8_t comp_mode;  /* 0 for Equal and 1 for Not equal */
  fal_tunnel_overlay_type_t hdr_type; /* Tunnel header type */
  a_uint16_t flags;  /* Flags value for exception check */
  a_uint16_t mask;   /* Flags mask for exception check */
} fal_tunnel_flags_excep_parser_ctrl_t;

#ifndef IN_SEC_MINI
sw_error_t
fal_sec_l2_excep_ctrl_set(a_uint32_t dev_id, a_uint32_t excep_type, fal_l2_excep_ctrl_t *ctrl);

sw_error_t
fal_sec_l2_excep_ctrl_get(a_uint32_t dev_id, a_uint32_t excep_type, fal_l2_excep_ctrl_t *ctrl);

sw_error_t
fal_sec_l3_excep_parser_ctrl_set(a_uint32_t dev_id, fal_l3_excep_parser_ctrl *ctrl);

sw_error_t
fal_sec_l3_excep_parser_ctrl_get(a_uint32_t dev_id, fal_l3_excep_parser_ctrl *ctrl);
#endif

sw_error_t
fal_sec_tunnel_excep_ctrl_set(a_uint32_t dev_id, a_uint32_t excep_type, fal_tunnel_excep_ctrl_t *ctrl);

#ifndef IN_SEC_MINI
sw_error_t
fal_sec_tunnel_excep_ctrl_get(a_uint32_t dev_id, a_uint32_t excep_type, fal_tunnel_excep_ctrl_t *ctrl);

sw_error_t
fal_sec_tunnel_l3_excep_parser_ctrl_set(a_uint32_t dev_id, fal_l3_excep_parser_ctrl *ctrl);

sw_error_t
fal_sec_tunnel_l3_excep_parser_ctrl_get(a_uint32_t dev_id, fal_l3_excep_parser_ctrl *ctrl);
#endif

sw_error_t
fal_sec_tunnel_l4_excep_parser_ctrl_set(a_uint32_t dev_id, fal_l4_excep_parser_ctrl *ctrl);

#ifndef IN_SEC_MINI
sw_error_t
fal_sec_tunnel_l4_excep_parser_ctrl_get(a_uint32_t dev_id, fal_l4_excep_parser_ctrl *ctrl);

sw_error_t
fal_sec_tunnel_flags_excep_parser_ctrl_set(a_uint32_t dev_id, a_uint32_t index, fal_tunnel_flags_excep_parser_ctrl_t *ctrl);

sw_error_t
fal_sec_tunnel_flags_excep_parser_ctrl_get(a_uint32_t dev_id, a_uint32_t index, fal_tunnel_flags_excep_parser_ctrl_t *ctrl);
#endif

sw_error_t
fal_sec_l3_excep_ctrl_set(a_uint32_t dev_id, a_uint32_t excep_type, fal_l3_excep_ctrl_t *ctrl);

sw_error_t
fal_sec_l3_excep_ctrl_get(a_uint32_t dev_id, a_uint32_t excep_type, fal_l3_excep_ctrl_t *ctrl);

sw_error_t
fal_sec_l4_excep_parser_ctrl_set(a_uint32_t dev_id, fal_l4_excep_parser_ctrl *ctrl);

sw_error_t
fal_sec_l4_excep_parser_ctrl_get(a_uint32_t dev_id, fal_l4_excep_parser_ctrl *ctrl);

#ifdef __cplusplus
}
#endif                          /* __cplusplus */
#endif                          /* _FAL_SEC_H_ */

/**
 * @}
 */

