/* SPDX-License-Identifier: BSD-3-Clause-Clear */
/*
 * Copyright (c) 2018-2021 The Linux Foundation. All rights reserved.
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 */

#include "../core.h"

#ifndef ATH12K_HAL_DESC_H
#define ATH12K_HAL_DESC_H

#define HAL_TLV_HDR_TAG		GENMASK(9, 0)
#define HAL_TLV_HDR_LEN		GENMASK(25, 10)
#define HAL_TLV_USR_ID		GENMASK(31, 26)

#define HAL_TLV_64_HDR_LENGTH		GENMASK(21, 10)
#define HAL_TLV_64_HDR_SRC_LINK_ID	GENMASK(24, 22)

enum hal_tlv_tag {
	HAL_MACTX_CBF_START                                    = 0 /* 0x0 */,
	HAL_PHYRX_DATA                                         = 1 /* 0x1 */,
	HAL_PHYRX_CBF_DATA_RESP                                = 2 /* 0x2 */,
	HAL_PHYRX_ABORT_REQUEST                                = 3 /* 0x3 */,
	HAL_PHYRX_USER_ABORT_NOTIFICATION                      = 4 /* 0x4 */,
	HAL_MACTX_DATA_RESP                                    = 5 /* 0x5 */,
	HAL_MACTX_CBF_DATA                                     = 6 /* 0x6 */,
	HAL_MACTX_CBF_DONE                                     = 7 /* 0x7 */,
	HAL_PHYRX_LMR_DATA_RESP                                = 8 /* 0x8 */,
	HAL_RXPCU_TO_UCODE_START                               = 9 /* 0x9 */,
	HAL_RXPCU_TO_UCODE_DELIMITER_FOR_FULL_MPDU             = 10 /* 0xa */,
	HAL_RXPCU_TO_UCODE_FULL_MPDU_DATA                      = 11 /* 0xb */,
	HAL_RXPCU_TO_UCODE_FCS_STATUS                          = 12 /* 0xc */,
	HAL_RXPCU_TO_UCODE_MPDU_DELIMITER                      = 13 /* 0xd */,
	HAL_RXPCU_TO_UCODE_DELIMITER_FOR_MPDU_HEADER           = 14 /* 0xe */,
	HAL_RXPCU_TO_UCODE_MPDU_HEADER_DATA                    = 15 /* 0xf */,
	HAL_RXPCU_TO_UCODE_END                                 = 16 /* 0x10 */,
	HAL_PHYRX_RSSI_LEGACY_20MHZ                            = 17 /* 0x11 */,
	HAL_PHYRX_NC_ABORT_REQUEST                             = 18 /* 0x12 */,
	HAL_PHYRX_PKT_END_20MHZ                                = 19 /* 0x13 */,
	HAL_PHYRX_NC_DATA                                      = 20 /* 0x14 */,
	HAL_PHYRX_PRECODING_CBF_DATA_RESP                      = 21 /* 0x15 */,
	HAL_MACTX_MU_UPLINK_COMMON_PUNC                        = 39 /* 0x27 */,
	HAL_MACRX_CBF_READ_REQUEST                             = 64 /* 0x40 */,
	HAL_MACRX_CBF_DATA_REQUEST                             = 65 /* 0x41 */,
	HAL_MACRX_EXPECT_NDP_RECEPTION                         = 66 /* 0x42 */,
	HAL_MACRX_FREEZE_CAPTURE_CHANNEL                       = 67 /* 0x43 */,
	HAL_MACRX_NDP_TIMEOUT                                  = 68 /* 0x44 */,
	HAL_MACRX_ABORT_ACK                                    = 69 /* 0x45 */,
	HAL_MACRX_REQ_IMPLICIT_FB                              = 70 /* 0x46 */,
	HAL_MACRX_CHAIN_MASK                                   = 71 /* 0x47 */,
	HAL_MACRX_NAP_USER                                     = 72 /* 0x48 */,
	HAL_MACRX_ABORT_REQUEST                                = 73 /* 0x49 */,
	HAL_PHYTX_OTHER_TRANSMIT_INFO16                        = 74 /* 0x4a */,
	HAL_PHYTX_ABORT_ACK                                    = 75 /* 0x4b */,
	HAL_PHYTX_ABORT_REQUEST                                = 76 /* 0x4c */,
	HAL_PHYTX_PKT_END                                      = 77 /* 0x4d */,
	HAL_PHYTX_PPDU_HEADER_INFO_REQUEST                     = 78 /* 0x4e */,
	HAL_PHYTX_REQUEST_CTRL_INFO                            = 79 /* 0x4f */,
	HAL_PHYTX_DATA_REQUEST                                 = 80 /* 0x50 */,
	HAL_PHYTX_BF_CV_LOADING_DONE                           = 81 /* 0x51 */,
	HAL_PHYTX_NAP_ACK                                      = 82 /* 0x52 */,
	HAL_PHYTX_NAP_DONE                                     = 83 /* 0x53 */,
	HAL_PHYTX_OFF_ACK                                      = 84 /* 0x54 */,
	HAL_PHYTX_ON_ACK                                       = 85 /* 0x55 */,
	HAL_PHYTX_SYNTH_OFF_ACK                                = 86 /* 0x56 */,
	HAL_PHYTX_DEBUG16                                      = 87 /* 0x57 */,
	HAL_MACTX_ABORT_REQUEST                                = 88 /* 0x58 */,
	HAL_MACTX_ABORT_ACK                                    = 89 /* 0x59 */,
	HAL_MACTX_PKT_END                                      = 90 /* 0x5a */,
	HAL_MACTX_PRE_PHY_DESC                                 = 91 /* 0x5b */,
	HAL_MACTX_BF_PARAMS_COMMON                             = 92 /* 0x5c */,
	HAL_MACTX_BF_PARAMS_PER_USER                           = 93 /* 0x5d */,
	HAL_MACTX_PREFETCH_CV                                  = 94 /* 0x5e */,
	HAL_MACTX_USER_DESC_COMMON                             = 95 /* 0x5f */,
	HAL_MACTX_USER_DESC_PER_USER                           = 96 /* 0x60 */,
	HAL_EXAMPLE_USER_TLV_16                                = 97 /* 0x61 */,
	HAL_EXAMPLE_TLV_16                                     = 98 /* 0x62 */,
	HAL_MACTX_PHY_OFF                                      = 99 /* 0x63 */,
	HAL_MACTX_PHY_ON                                       = 100 /* 0x64 */,
	HAL_MACTX_SYNTH_OFF                                    = 101 /* 0x65 */,
	HAL_MACTX_EXPECT_CBF_COMMON                            = 102 /* 0x66 */,
	HAL_MACTX_EXPECT_CBF_PER_USER                          = 103 /* 0x67 */,
	HAL_MACTX_PHY_DESC                                     = 104 /* 0x68 */,
	HAL_MACTX_L_SIG_A                                      = 105 /* 0x69 */,
	HAL_MACTX_L_SIG_B                                      = 106 /* 0x6a */,
	HAL_MACTX_HT_SIG                                       = 107 /* 0x6b */,
	HAL_MACTX_VHT_SIG_A                                    = 108 /* 0x6c */,
	HAL_MACTX_VHT_SIG_B_SU20                               = 109 /* 0x6d */,
	HAL_MACTX_VHT_SIG_B_SU40                               = 110 /* 0x6e */,
	HAL_MACTX_VHT_SIG_B_SU80                               = 111 /* 0x6f */,
	HAL_MACTX_VHT_SIG_B_SU160                              = 112 /* 0x70 */,
	HAL_MACTX_VHT_SIG_B_MU20                               = 113 /* 0x71 */,
	HAL_MACTX_VHT_SIG_B_MU40                               = 114 /* 0x72 */,
	HAL_MACTX_VHT_SIG_B_MU80                               = 115 /* 0x73 */,
	HAL_MACTX_VHT_SIG_B_MU160                              = 116 /* 0x74 */,
	HAL_MACTX_SERVICE                                      = 117 /* 0x75 */,
	HAL_MACTX_HE_SIG_A_SU                                  = 118 /* 0x76 */,
	HAL_MACTX_HE_SIG_A_MU_DL                               = 119 /* 0x77 */,
	HAL_MACTX_HE_SIG_A_MU_UL                               = 120 /* 0x78 */,
	HAL_MACTX_HE_SIG_B1_MU                                 = 121 /* 0x79 */,
	HAL_MACTX_HE_SIG_B2_MU                                 = 122 /* 0x7a */,
	HAL_MACTX_HE_SIG_B2_OFDMA                              = 123 /* 0x7b */,
	HAL_MACTX_DELETE_CV                                    = 124 /* 0x7c */,
	HAL_MACTX_MU_UPLINK_COMMON                             = 125 /* 0x7d */,
	HAL_MACTX_MU_UPLINK_USER_SETUP                         = 126 /* 0x7e */,
	HAL_MACTX_OTHER_TRANSMIT_INFO                          = 127 /* 0x7f */,
	HAL_MACTX_PHY_NAP                                      = 128 /* 0x80 */,
	HAL_MACTX_DEBUG                                        = 129 /* 0x81 */,
	HAL_PHYRX_ABORT_ACK                                    = 130 /* 0x82 */,
	HAL_PHYRX_GENERATED_CBF_DETAILS                        = 131 /* 0x83 */,
	HAL_PHYRX_RSSI_LEGACY                                  = 132 /* 0x84 */,
	HAL_PHYRX_RSSI_HT                                      = 133 /* 0x85 */,
	HAL_PHYRX_USER_INFO                                    = 134 /* 0x86 */,
	HAL_PHYRX_PKT_END                                      = 135 /* 0x87 */,
	HAL_PHYRX_DEBUG                                        = 136 /* 0x88 */,
	HAL_PHYRX_CBF_TRANSFER_DONE                            = 137 /* 0x89 */,
	HAL_PHYRX_CBF_TRANSFER_ABORT                           = 138 /* 0x8a */,
	HAL_PHYRX_L_SIG_A                                      = 139 /* 0x8b */,
	HAL_PHYRX_L_SIG_B                                      = 140 /* 0x8c */,
	HAL_PHYRX_HT_SIG                                       = 141 /* 0x8d */,
	HAL_PHYRX_VHT_SIG_A                                    = 142 /* 0x8e */,
	HAL_PHYRX_VHT_SIG_B_SU20                               = 143 /* 0x8f */,
	HAL_PHYRX_VHT_SIG_B_SU40                               = 144 /* 0x90 */,
	HAL_PHYRX_VHT_SIG_B_SU80                               = 145 /* 0x91 */,
	HAL_PHYRX_VHT_SIG_B_SU160                              = 146 /* 0x92 */,
	HAL_PHYRX_VHT_SIG_B_MU20                               = 147 /* 0x93 */,
	HAL_PHYRX_VHT_SIG_B_MU40                               = 148 /* 0x94 */,
	HAL_PHYRX_VHT_SIG_B_MU80                               = 149 /* 0x95 */,
	HAL_PHYRX_VHT_SIG_B_MU160                              = 150 /* 0x96 */,
	HAL_PHYRX_HE_SIG_A_SU                                  = 151 /* 0x97 */,
	HAL_PHYRX_HE_SIG_A_MU_DL                               = 152 /* 0x98 */,
	HAL_PHYRX_HE_SIG_A_MU_UL                               = 153 /* 0x99 */,
	HAL_PHYRX_HE_SIG_B1_MU                                 = 154 /* 0x9a */,
	HAL_PHYRX_HE_SIG_B2_MU                                 = 155 /* 0x9b */,
	HAL_PHYRX_HE_SIG_B2_OFDMA                              = 156 /* 0x9c */,
	HAL_PHYRX_OTHER_RECEIVE_INFO                           = 157 /* 0x9d */,
	HAL_PHYRX_COMMON_USER_INFO                             = 158 /* 0x9e */,
	HAL_PHYRX_DATA_DONE                                    = 159 /* 0x9f */,
	HAL_COEX_TX_REQ                                        = 160 /* 0xa0 */,
	HAL_DUMMY                                              = 161 /* 0xa1 */,
	HAL_EXAMPLE_TLV_32_NAME                                = 162 /* 0xa2 */,
	HAL_MPDU_LIMIT                                         = 163 /* 0xa3 */,
	HAL_NA_LENGTH_END                                      = 164 /* 0xa4 */,
	HAL_OLE_BUF_STATUS                                     = 165 /* 0xa5 */,
	HAL_PCU_PPDU_SETUP_DONE                                = 166 /* 0xa6 */,
	HAL_PCU_PPDU_SETUP_END                                 = 167 /* 0xa7 */,
	HAL_PCU_PPDU_SETUP_INIT                                = 168 /* 0xa8 */,
	HAL_PCU_PPDU_SETUP_START                               = 169 /* 0xa9 */,
	HAL_PDG_FES_SETUP                                      = 170 /* 0xaa */,
	HAL_PDG_RESPONSE                                       = 171 /* 0xab */,
	HAL_PDG_TX_REQ                                         = 172 /* 0xac */,
	HAL_SCH_WAIT_INSTR                                     = 173 /* 0xad */,
	HAL_MACTX_SWITCH_TO_MAIN                               = 174 /* 0xae */,
	HAL_PHYTX_LINK_STATE                                   = 175 /* 0xaf */,
	HAL_AUX_PPDU_END                                       = 176 /* 0xb0 */,
	HAL_TQM_GEN_MPDU_LENGTH_LIST                           = 177 /* 0xb1 */,
	HAL_TQM_GEN_MPDU_LENGTH_LIST_STATUS                    = 178 /* 0xb2 */,
	HAL_TQM_GEN_MPDUS                                      = 179 /* 0xb3 */,
	HAL_TQM_GEN_MPDUS_STATUS                               = 180 /* 0xb4 */,
	HAL_TQM_REMOVE_MPDU                                    = 181 /* 0xb5 */,
	HAL_TQM_REMOVE_MPDU_STATUS                             = 182 /* 0xb6 */,
	HAL_TQM_REMOVE_MSDU                                    = 183 /* 0xb7 */,
	HAL_TQM_REMOVE_MSDU_STATUS                             = 184 /* 0xb8 */,
	HAL_TQM_UPDATE_TX_MPDU_COUNT                           = 185 /* 0xb9 */,
	HAL_TQM_WRITE_CMD                                      = 186 /* 0xba */,
	HAL_OFDMA_TRIGGER_DETAILS                              = 187 /* 0xbb */,
	HAL_TX_DATA                                            = 188 /* 0xbc */,
	HAL_TX_FES_SETUP                                       = 189 /* 0xbd */,
	HAL_RX_PACKET                                          = 190 /* 0xbe */,
	HAL_EXPECTED_RESPONSE                                  = 191 /* 0xbf */,
	HAL_TX_MPDU_END                                        = 192 /* 0xc0 */,
	HAL_TX_MPDU_START                                      = 193 /* 0xc1 */,
	HAL_TX_MSDU_END                                        = 194 /* 0xc2 */,
	HAL_TX_MSDU_START                                      = 195 /* 0xc3 */,
	HAL_TX_SW_MODE_SETUP                                   = 196 /* 0xc4 */,
	HAL_TXPCU_BUFFER_STATUS                                = 197 /* 0xc5 */,
	HAL_TXPCU_USER_BUFFER_STATUS                           = 198 /* 0xc6 */,
	HAL_DATA_TO_TIME_CONFIG                                = 199 /* 0xc7 */,
	HAL_EXAMPLE_USER_TLV_32                                = 200 /* 0xc8 */,
	HAL_MPDU_INFO                                          = 201 /* 0xc9 */,
	HAL_PDG_USER_SETUP                                     = 202 /* 0xca */,
	HAL_TX_11AH_SETUP                                      = 203 /* 0xcb */,
	HAL_REO_UPDATE_RX_REO_QUEUE_STATUS                     = 204 /* 0xcc */,
	HAL_TX_PEER_ENTRY                                      = 205 /* 0xcd */,
	HAL_TX_RAW_OR_NATIVE_FRAME_SETUP                       = 206 /* 0xce */,
	HAL_EXAMPLE_USER_TLV_44                                = 207 /* 0xcf */,
	HAL_TX_FLUSH                                           = 208 /* 0xd0 */,
	HAL_TX_FLUSH_REQ                                       = 209 /* 0xd1 */,
	HAL_TQM_WRITE_CMD_STATUS                               = 210 /* 0xd2 */,
	HAL_TQM_GET_MPDU_QUEUE_STATS                           = 211 /* 0xd3 */,
	HAL_TQM_GET_MSDU_FLOW_STATS                            = 212 /* 0xd4 */,
	HAL_EXAMPLE_USER_CTLV_44                               = 213 /* 0xd5 */,
	HAL_TX_FES_STATUS_START                                = 214 /* 0xd6 */,
	HAL_TX_FES_STATUS_USER_PPDU                            = 215 /* 0xd7 */,
	HAL_TX_FES_STATUS_USER_RESPONSE                        = 216 /* 0xd8 */,
	HAL_TX_FES_STATUS_END                                  = 217 /* 0xd9 */,
	HAL_RX_TRIG_INFO                                       = 218 /* 0xda */,
	HAL_RXPCU_TX_SETUP_CLEAR                               = 219 /* 0xdb */,
	HAL_RX_FRAME_BITMAP_REQ                                = 220 /* 0xdc */,
	HAL_RX_FRAME_BITMAP_ACK                                = 221 /* 0xdd */,
	HAL_COEX_RX_STATUS                                     = 222 /* 0xde */,
	HAL_RX_START_PARAM                                     = 223 /* 0xdf */,
	HAL_RX_PPDU_START                                      = 224 /* 0xe0 */,
	HAL_RX_PPDU_END                                        = 225 /* 0xe1 */,
	HAL_RX_MPDU_START                                      = 226 /* 0xe2 */,
	HAL_RX_MPDU_END                                        = 227 /* 0xe3 */,
	HAL_RX_MSDU_START                                      = 228 /* 0xe4 */,
	HAL_RX_MSDU_END                                        = 229 /* 0xe5 */,
	HAL_RX_ATTENTION                                       = 230 /* 0xe6 */,
	HAL_RECEIVED_RESPONSE_INFO                             = 231 /* 0xe7 */,
	HAL_RX_PHY_SLEEP                                       = 232 /* 0xe8 */,
	HAL_RX_HEADER                                          = 233 /* 0xe9 */,
	HAL_RX_PEER_ENTRY                                      = 234 /* 0xea */,
	HAL_RX_FLUSH                                           = 235 /* 0xeb */,
	HAL_RX_RESPONSE_REQUIRED_INFO                          = 236 /* 0xec */,
	HAL_RX_FRAMELESS_BAR_DETAILS                           = 237 /* 0xed */,
	HAL_TQM_GET_MPDU_QUEUE_STATS_STATUS                    = 238 /* 0xee */,
	HAL_TQM_GET_MSDU_FLOW_STATS_STATUS                     = 239 /* 0xef */,
	HAL_TX_CBF_INFO                                        = 240 /* 0xf0 */,
	HAL_PCU_PPDU_SETUP_USER                                = 241 /* 0xf1 */,
	HAL_RX_MPDU_PCU_START                                  = 242 /* 0xf2 */,
	HAL_RX_PM_INFO                                         = 243 /* 0xf3 */,
	HAL_RX_USER_PPDU_END                                   = 244 /* 0xf4 */,
	HAL_RX_PRE_PPDU_START                                  = 245 /* 0xf5 */,
	HAL_RX_PREAMBLE                                        = 246 /* 0xf6 */,
	HAL_TX_FES_SETUP_COMPLETE                              = 247 /* 0xf7 */,
	HAL_TX_LAST_MPDU_FETCHED                               = 248 /* 0xf8 */,
	HAL_TXDMA_STOP_REQUEST                                 = 249 /* 0xf9 */,
	HAL_RXPCU_SETUP                                        = 250 /* 0xfa */,
	HAL_RXPCU_USER_SETUP                                   = 251 /* 0xfb */,
	HAL_TX_FES_STATUS_ACK_OR_BA                            = 252 /* 0xfc */,
	HAL_TQM_ACKED_MPDU                                     = 253 /* 0xfd */,
	HAL_COEX_TX_RESP                                       = 254 /* 0xfe */,
	HAL_COEX_TX_STATUS                                     = 255 /* 0xff */,
	HAL_MACTX_COEX_PHY_CTRL                                = 256 /* 0x100 */,
	HAL_COEX_STATUS_BROADCAST                              = 257 /* 0x101 */,
	HAL_RESPONSE_START_STATUS                              = 258 /* 0x102 */,
	HAL_RESPONSE_END_STATUS                                = 259 /* 0x103 */,
	HAL_CRYPTO_STATUS                                      = 260 /* 0x104 */,
	HAL_RECEIVED_TRIGGER_INFO                              = 261 /* 0x105 */,
	HAL_COEX_TX_STOP_CTRL                                  = 262 /* 0x106 */,
	HAL_RX_PPDU_ACK_REPORT                                 = 263 /* 0x107 */,
	HAL_RX_PPDU_NO_ACK_REPORT                              = 264 /* 0x108 */,
	HAL_SCH_COEX_STATUS                                    = 265 /* 0x109 */,
	HAL_SCHEDULER_COMMAND_STATUS                           = 266 /* 0x10a */,
	HAL_SCHEDULER_RX_PPDU_NO_RESPONSE_STATUS               = 267 /* 0x10b */,
	HAL_TX_FES_STATUS_PROT                                 = 268 /* 0x10c */,
	HAL_TX_FES_STATUS_START_PPDU                           = 269 /* 0x10d */,
	HAL_TX_FES_STATUS_START_PROT                           = 270 /* 0x10e */,
	HAL_TXPCU_PHYTX_DEBUG32                                = 271 /* 0x10f */,
	HAL_TXPCU_PHYTX_OTHER_TRANSMIT_INFO32                  = 272 /* 0x110 */,
	HAL_TX_MPDU_COUNT_TRANSFER_END                         = 273 /* 0x111 */,
	HAL_WHO_ANCHOR_OFFSET                                  = 274 /* 0x112 */,
	HAL_WHO_ANCHOR_VALUE                                   = 275 /* 0x113 */,
	HAL_WHO_CCE_INFO                                       = 276 /* 0x114 */,
	HAL_WHO_COMMIT                                         = 277 /* 0x115 */,
	HAL_WHO_COMMIT_DONE                                    = 278 /* 0x116 */,
	HAL_WHO_FLUSH                                          = 279 /* 0x117 */,
	HAL_WHO_L2_LLC                                         = 280 /* 0x118 */,
	HAL_WHO_L2_PAYLOAD                                     = 281 /* 0x119 */,
	HAL_WHO_L3_CHECKSUM                                    = 282 /* 0x11a */,
	HAL_WHO_L3_INFO                                        = 283 /* 0x11b */,
	HAL_WHO_L4_CHECKSUM                                    = 284 /* 0x11c */,
	HAL_WHO_L4_INFO                                        = 285 /* 0x11d */,
	HAL_WHO_MSDU                                           = 286 /* 0x11e */,
	HAL_WHO_MSDU_MISC                                      = 287 /* 0x11f */,
	HAL_WHO_PACKET_DATA                                    = 288 /* 0x120 */,
	HAL_WHO_PACKET_HDR                                     = 289 /* 0x121 */,
	HAL_WHO_PPDU_END                                       = 290 /* 0x122 */,
	HAL_WHO_PPDU_START                                     = 291 /* 0x123 */,
	HAL_WHO_TSO                                            = 292 /* 0x124 */,
	HAL_WHO_WMAC_HEADER_PV0                                = 293 /* 0x125 */,
	HAL_WHO_WMAC_HEADER_PV1                                = 294 /* 0x126 */,
	HAL_WHO_WMAC_IV                                        = 295 /* 0x127 */,
	HAL_MPDU_INFO_END                                      = 296 /* 0x128 */,
	HAL_MPDU_INFO_BITMAP                                   = 297 /* 0x129 */,
	HAL_TX_QUEUE_EXTENSION                                 = 298 /* 0x12a */,
	HAL_SCHEDULER_SELFGEN_RESPONSE_STATUS                  = 299 /* 0x12b */,
	HAL_TQM_UPDATE_TX_MPDU_COUNT_STATUS                    = 300 /* 0x12c */,
	HAL_TQM_ACKED_MPDU_STATUS                              = 301 /* 0x12d */,
	HAL_TQM_ADD_MSDU_STATUS                                = 302 /* 0x12e */,
	HAL_TQM_LIST_GEN_DONE                                  = 303 /* 0x12f */,
	HAL_WHO_TERMINATE                                      = 304 /* 0x130 */,
	HAL_TX_LAST_MPDU_END                                   = 305 /* 0x131 */,
	HAL_TX_CV_DATA                                         = 306 /* 0x132 */,
	HAL_PPDU_TX_END                                        = 307 /* 0x133 */,
	HAL_PROT_TX_END                                        = 308 /* 0x134 */,
	HAL_MPDU_INFO_GLOBAL_END                               = 309 /* 0x135 */,
	HAL_TQM_SCH_INSTR_GLOBAL_END                           = 310 /* 0x136 */,
	HAL_RX_PPDU_END_USER_STATS                             = 311 /* 0x137 */,
	HAL_RX_PPDU_END_USER_STATS_EXT                         = 312 /* 0x138 */,
	HAL_REO_GET_QUEUE_STATS                                = 313 /* 0x139 */,
	HAL_REO_FLUSH_QUEUE                                    = 314 /* 0x13a */,
	HAL_REO_FLUSH_CACHE                                    = 315 /* 0x13b */,
	HAL_REO_UNBLOCK_CACHE                                  = 316 /* 0x13c */,
	HAL_REO_GET_QUEUE_STATS_STATUS                         = 317 /* 0x13d */,
	HAL_REO_FLUSH_QUEUE_STATUS                             = 318 /* 0x13e */,
	HAL_REO_FLUSH_CACHE_STATUS                             = 319 /* 0x13f */,
	HAL_REO_UNBLOCK_CACHE_STATUS                           = 320 /* 0x140 */,
	HAL_TQM_FLUSH_CACHE                                    = 321 /* 0x141 */,
	HAL_TQM_UNBLOCK_CACHE                                  = 322 /* 0x142 */,
	HAL_TQM_FLUSH_CACHE_STATUS                             = 323 /* 0x143 */,
	HAL_TQM_UNBLOCK_CACHE_STATUS                           = 324 /* 0x144 */,
	HAL_RX_PPDU_END_STATUS_DONE                            = 325 /* 0x145 */,
	HAL_RX_STATUS_BUFFER_DONE                              = 326 /* 0x146 */,
	HAL_SCHEDULER_MLO_SW_MSG_STATUS                        = 327 /* 0x147 */,
	HAL_SCHEDULER_TXOP_DURATION_TRIGGER                    = 328 /* 0x148 */,
	HAL_TX_DATA_SYNC                                       = 329 /* 0x149 */,
	HAL_PHYRX_CBF_READ_REQUEST_ACK                         = 330 /* 0x14a */,
	HAL_TQM_GET_MPDU_HEAD_INFO                             = 331 /* 0x14b */,
	HAL_TQM_SYNC_CMD                                       = 332 /* 0x14c */,
	HAL_TQM_GET_MPDU_HEAD_INFO_STATUS                      = 333 /* 0x14d */,
	HAL_TQM_SYNC_CMD_STATUS                                = 334 /* 0x14e */,
	HAL_TQM_THRESHOLD_DROP_NOTIFICATION_STATUS             = 335 /* 0x14f */,
	HAL_TQM_GEN_MPDUS_PART2_STATUS                         = 336 /* 0x150 */,
	HAL_REO_FLUSH_TIMEOUT_LIST                             = 337 /* 0x151 */,
	HAL_REO_FLUSH_TIMEOUT_LIST_STATUS                      = 338 /* 0x152 */,
	HAL_REO_DESCRIPTOR_THRESHOLD_REACHED_STATUS            = 339 /* 0x153 */,
	HAL_SCHEDULER_RX_SIFS_RESPONSE_TRIGGER_STATUS          = 340 /* 0x154 */,
	HAL_EXAMPLE_USER_TLV_32_NAME                           = 341 /* 0x155 */,
	HAL_RX_PPDU_START_USER_INFO                            = 342 /* 0x156 */,
	HAL_RX_RING_MASK                                       = 343 /* 0x157 */,
	HAL_COEX_MAC_NAP                                       = 344 /* 0x158 */,
	HAL_RXPCU_PPDU_END_INFO                                = 345 /* 0x159 */,
	HAL_WHO_MESH_CONTROL                                   = 346 /* 0x15a */,
	HAL_PDG_SW_MODE_BW_START                               = 347 /* 0x15b */,
	HAL_PDG_SW_MODE_BW_END                                 = 348 /* 0x15c */,
	HAL_PDG_WAIT_FOR_MAC_REQUEST                           = 349 /* 0x15d */,
	HAL_PDG_WAIT_FOR_PHY_REQUEST                           = 350 /* 0x15e */,
	HAL_SCHEDULER_END                                      = 351 /* 0x15f */,
	HAL_RX_PPDU_START_DROPPED                              = 352 /* 0x160 */,
	HAL_RX_PPDU_END_DROPPED                                = 353 /* 0x161 */,
	HAL_RX_PPDU_END_STATUS_DONE_DROPPED                    = 354 /* 0x162 */,
	HAL_RX_MPDU_START_DROPPED                              = 355 /* 0x163 */,
	HAL_RX_MSDU_START_DROPPED                              = 356 /* 0x164 */,
	HAL_RX_MSDU_END_DROPPED                                = 357 /* 0x165 */,
	HAL_RX_MPDU_END_DROPPED                                = 358 /* 0x166 */,
	HAL_RX_ATTENTION_DROPPED                               = 359 /* 0x167 */,
	HAL_TXPCU_USER_SETUP                                   = 360 /* 0x168 */,
	HAL_RXPCU_USER_SETUP_EXT                               = 361 /* 0x169 */,
	HAL_CMD_PART_0_END                                     = 362 /* 0x16a */,
	HAL_MACTX_SYNTH_ON                                     = 363 /* 0x16b */,
	HAL_SCH_CRITICAL_TLV_REFERENCE                         = 364 /* 0x16c */,
	HAL_TQM_MPDU_GLOBAL_START                              = 365 /* 0x16d */,
	HAL_EXAMPLE_TLV_32                                     = 366 /* 0x16e */,
	HAL_TQM_UPDATE_TX_MSDU_FLOW                            = 367 /* 0x16f */,
	HAL_TQM_UPDATE_TX_MPDU_QUEUE_HEAD                      = 368 /* 0x170 */,
	HAL_TQM_UPDATE_TX_MSDU_FLOW_STATUS                     = 369 /* 0x171 */,
	HAL_TQM_UPDATE_TX_MPDU_QUEUE_HEAD_STATUS               = 370 /* 0x172 */,
	HAL_REO_UPDATE_RX_REO_QUEUE                            = 371 /* 0x173 */,
	HAL_TQM_2_SCH_MPDU_AVAILABLE                           = 372 /* 0x174 */,
	HAL_PDG_TRIG_RESPONSE                                  = 373 /* 0x175 */,
	HAL_TRIGGER_RESPONSE_TX_DONE                           = 374 /* 0x176 */,
	HAL_ABORT_FROM_PHYRX_DETAILS                           = 375 /* 0x177 */,
	HAL_SCH_TQM_TLV_WRAPPER                                = 376 /* 0x178 */,
	HAL_MPDUS_AVAILABLE                                    = 377 /* 0x179 */,
	HAL_RECEIVED_RESPONSE_INFO_PART2                       = 378 /* 0x17a */,
	HAL_PHYRX_TX_START_TIMING                              = 379 /* 0x17b */,
	HAL_TXPCU_PREAMBLE_DONE                                = 380 /* 0x17c */,
	HAL_NDP_PREAMBLE_DONE                                  = 381 /* 0x17d */,
	HAL_SCH_TQM_CMD_WRAPPER_RBO_DROP                       = 382 /* 0x17e */,
	HAL_SCH_TQM_CMD_WRAPPER_CONT_DROP                      = 383 /* 0x17f */,
	HAL_MACTX_CLEAR_PREV_TX_INFO                           = 384 /* 0x180 */,
	HAL_TX_PUNCTURE_GROUP_SETUP                            = 385 /* 0x181 */,
	HAL_R2R_STATUS_END                                     = 386 /* 0x182 */,
	HAL_MACTX_PREFETCH_CV_COMMON                           = 387 /* 0x183 */,
	HAL_END_OF_FLUSH_MARKER                                = 388 /* 0x184 */,
	HAL_MACTX_MU_UPLINK_USER_SETUP_PUNC                    = 390 /* 0x186 */,
	HAL_RECEIVED_RESPONSE_USER_7_0                         = 391 /* 0x187 */,
	HAL_RECEIVED_RESPONSE_USER_15_8                        = 392 /* 0x188 */,
	HAL_RECEIVED_RESPONSE_USER_23_16                       = 393 /* 0x189 */,
	HAL_RECEIVED_RESPONSE_USER_31_24                       = 394 /* 0x18a */,
	HAL_RECEIVED_RESPONSE_USER_36_32                       = 395 /* 0x18b */,
	HAL_TX_LOOPBACK_SETUP                                  = 396 /* 0x18c */,
	HAL_PHYRX_OTHER_RECEIVE_INFO_RU_DETAILS                = 397 /* 0x18d */,
	HAL_SCH_WAIT_INSTR_TX_PATH                             = 398 /* 0x18e */,
	HAL_MACTX_OTHER_TRANSMIT_INFO_TX2TX                    = 399 /* 0x18f */,
	HAL_MACTX_OTHER_TRANSMIT_INFO_EMUPHY_SETUP             = 400 /* 0x190 */,
	HAL_PHYRX_OTHER_RECEIVE_INFO_EVM_DETAILS               = 401 /* 0x191 */,
	HAL_TX_WUR_DATA                                        = 402 /* 0x192 */,
	HAL_RX_PPDU_END_START                                  = 403 /* 0x193 */,
	HAL_RX_PPDU_END_MIDDLE                                 = 404 /* 0x194 */,
	HAL_RX_PPDU_END_LAST                                   = 405 /* 0x195 */,
	HAL_MACTX_BACKOFF_BASED_TRANSMISSION                   = 406 /* 0x196 */,
	HAL_MACTX_OTHER_TRANSMIT_INFO_DL_OFDMA_TX              = 407 /* 0x197 */,
	HAL_SRP_INFO                                           = 408 /* 0x198 */,
	HAL_OBSS_SR_INFO                                       = 409 /* 0x199 */,
	HAL_SCHEDULER_SW_MSG_STATUS                            = 410 /* 0x19a */,
	HAL_HWSCH_RXPCU_MAC_INFO_ANNOUNCEMENT                  = 411 /* 0x19b */,
	HAL_RXPCU_SETUP_COMPLETE                               = 412 /* 0x19c */,
	HAL_MACTX_MCC_SWITCH                                   = 413 /* 0x19d */,
	HAL_MACTX_MCC_SWITCH_BACK                              = 414 /* 0x19e */,
	HAL_PHYTX_MCC_SWITCH_ACK                               = 415 /* 0x19f */,
	HAL_PHYTX_MCC_SWITCH_BACK_ACK                          = 416 /* 0x1a0 */,
	HAL_PHYTX_EMLSR_PRE_SWITCH_ACK                         = 417 /* 0x1a1 */,
	HAL_LMR_TX_END                                         = 418 /* 0x1a2 */,
	HAL_PHYRX_OTHER_RECEIVE_INFO_MU_RSSI_COMMON            = 419 /* 0x1a3 */,
	HAL_PHYRX_OTHER_RECEIVE_INFO_MU_RSSI_USER              = 420 /* 0x1a4 */,
	HAL_MACTX_OTHER_TRANSMIT_INFO_SCH_DETAILS              = 421 /* 0x1a5 */,
	HAL_PHYRX_OTHER_RECEIVE_INFO_108P_EVM_DETAILS          = 422 /* 0x1a6 */,
	HAL_SCH_TLV_WRAPPER                                    = 423 /* 0x1a7 */,
	HAL_SCHEDULER_STATUS_WRAPPER                           = 424 /* 0x1a8 */,
	HAL_MULTI_MPDU_INFO                                    = 425 /* 0x1a9 */,
	HAL_MACTX_11AZ_USER_DESC_PER_USER                      = 426 /* 0x1aa */,
	HAL_MACTX_U_SIG_EHT_SU_MU                              = 427 /* 0x1ab */,
	HAL_MACTX_U_SIG_EHT_TB                                 = 428 /* 0x1ac */,
	HAL_COEX_TLV_ACC_TLV_TAG0_CFG                          = 429 /* 0x1ad */,
	HAL_COEX_TLV_ACC_TLV_TAG1_CFG                          = 430 /* 0x1ae */,
	HAL_COEX_TLV_ACC_TLV_TAG2_CFG                          = 431 /* 0x1af */,
	HAL_PHYRX_U_SIG_EHT_SU_MU                              = 432 /* 0x1b0 */,
	HAL_PHYRX_U_SIG_EHT_TB                                 = 433 /* 0x1b1 */,
	HAL_COEX_TLV_ACC_TLV_TAG3_CFG                          = 434 /* 0x1b2 */,
	HAL_COEX_TLV_ACC_TLV_TAG_CGIM_CFG                      = 435 /* 0x1b3 */,
	HAL_MACRX_LMR_READ_REQUEST                             = 437 /* 0x1b5 */,
	HAL_MACRX_LMR_DATA_REQUEST                             = 438 /* 0x1b6 */,
	HAL_PHYRX_LMR_TRANSFER_DONE                            = 439 /* 0x1b7 */,
	HAL_PHYRX_LMR_TRANSFER_ABORT                           = 440 /* 0x1b8 */,
	HAL_PHYRX_LMR_READ_REQUEST_ACK                         = 441 /* 0x1b9 */,
	HAL_MACRX_SECURE_LTF_SEQ_PTR                           = 442 /* 0x1ba */,
	HAL_PHYRX_USER_INFO_MU_UL                              = 443 /* 0x1bb */,
	HAL_MPDU_QUEUE_OVERVIEW                                = 444 /* 0x1bc */,
	HAL_SCHEDULER_NAV_INFO                                 = 445 /* 0x1bd */,
	HAL_MACTX_OTHER_TRANSMIT_INFO_ENABLE_RX                = 446 /* 0x1be */,
	HAL_LMR_PEER_ENTRY                                     = 447 /* 0x1bf */,
	HAL_LMR_MPDU_START                                     = 448 /* 0x1c0 */,
	HAL_LMR_DATA                                           = 449 /* 0x1c1 */,
	HAL_LMR_MPDU_END                                       = 450 /* 0x1c2 */,
	HAL_REO_GET_QUEUE_1K_STATS_STATUS                      = 451 /* 0x1c3 */,
	HAL_RX_FRAME_1K_BITMAP_ACK                             = 452 /* 0x1c4 */,
	HAL_TX_FES_STATUS_1K_BA                                = 453 /* 0x1c5 */,
	HAL_TQM_ACKED_1K_MPDU                                  = 454 /* 0x1c6 */,
	HAL_MACRX_INBSS_OBSS_IND                               = 455 /* 0x1c7 */,
	HAL_PHYRX_LOCATION                                     = 456 /* 0x1c8 */,
	HAL_MLO_TX_NOTIFICATION_SU                             = 457 /* 0x1c9 */,
	HAL_MLO_TX_NOTIFICATION_MU                             = 458 /* 0x1ca */,
	HAL_MLO_TX_REQ_SU                                      = 459 /* 0x1cb */,
	HAL_MLO_TX_REQ_MU                                      = 460 /* 0x1cc */,
	HAL_MLO_TX_RESP                                        = 461 /* 0x1cd */,
	HAL_MLO_RX_NOTIFICATION                                = 462 /* 0x1ce */,
	HAL_MLO_BKOFF_TRUNC_REQ                                = 463 /* 0x1cf */,
	HAL_MLO_TBTT_NOTIFICATION                              = 464 /* 0x1d0 */,
	HAL_MLO_MESSAGE                                        = 465 /* 0x1d1 */,
	HAL_MLO_TS_SYNC_MSG                                    = 466 /* 0x1d2 */,
	HAL_MLO_FES_SETUP                                      = 467 /* 0x1d3 */,
	HAL_MLO_PDG_FES_SETUP_SU                               = 468 /* 0x1d4 */,
	HAL_MLO_PDG_FES_SETUP_MU                               = 469 /* 0x1d5 */,
	HAL_MPDU_INFO_1K_BITMAP                                = 470 /* 0x1d6 */,
	HAL_MON_BUF_ADDR                                       = 471 /* 0x1d7 */,
	HAL_TX_FRAG_STATE                                      = 472 /* 0x1d8 */,
	HAL_MACTX_OTHER_TRANSMIT_INFO_PHY_CV_RESET             = 473 /* 0x1d9 */,
	HAL_MACTX_OTHER_TRANSMIT_INFO_SW_PEER_IDS              = 474 /* 0x1da */,
	HAL_MACTX_EHT_SIG_USR_OFDMA                            = 475 /* 0x1db */,
	HAL_MACTX_U_SIG_UHR_SU_MU                              = 476 /* 0x1dc */,
	HAL_MACTX_U_SIG_UHR_TB                                 = 477 /* 0x1dd */,
	HAL_PHYRX_LOCATION_PART2                               = 478 /* 0x1de */,
	HAL_PHYTX_LOCATION_PART2                               = 479 /* 0x1df */,
	HAL_MLO_FES_TERMINATE                                  = 480 /* 0x1e0 */,
	HAL_PHYRX_EHT_SIG_USR_OFDMA                            = 481 /* 0x1e1 */,
	HAL_PHYRX_PKT_END_PART1                                = 483 /* 0x1e3 */,
	HAL_MACTX_EXPECT_NDP_RECEPTION                         = 484 /* 0x1e4 */,
	HAL_MACTX_SECURE_LTF_SEQ_PTR                           = 485 /* 0x1e5 */,
	HAL_MLO_PDG_BKOFF_TRUNC_NOTIFY                         = 487 /* 0x1e7 */,
	HAL_PHYRX_11AZ_INTEGRITY_DATA                          = 488 /* 0x1e8 */,
	HAL_PHYTX_LOCATION                                     = 489 /* 0x1e9 */,
	HAL_PHYTX_11AZ_INTEGRITY_DATA                          = 490 /* 0x1ea */,
	HAL_MACTX_EHT_SIG_USR_SU                               = 492 /* 0x1ec */,
	HAL_MACTX_EHT_SIG_USR_MU_MIMO                          = 493 /* 0x1ed */,
	HAL_PHYRX_EHT_SIG_USR_SU                               = 494 /* 0x1ee */,
	HAL_PHYRX_EHT_SIG_USR_MU_MIMO                          = 495 /* 0x1ef */,
	HAL_PHYRX_GENERIC_U_SIG                                = 496 /* 0x1f0 */,
	HAL_PHYRX_GENERIC_EHT_OR_UHR_SIG                       = 497 /* 0x1f1 */,
	HAL_OVERWRITE_RESP_START                               = 498 /* 0x1f2 */,
	HAL_OVERWRITE_RESP_PREAMBLE_INFO                       = 499 /* 0x1f3 */,
	HAL_OVERWRITE_RESP_FRAME_INFO                          = 500 /* 0x1f4 */,
	HAL_OVERWRITE_RESP_END                                 = 501 /* 0x1f5 */,
	HAL_RXPCU_EARLY_RX_INDICATION                          = 502 /* 0x1f6 */,
	HAL_MON_DROP                                           = 503 /* 0x1f7 */,
	HAL_MACRX_MU_UPLINK_COMMON_SNIFF                       = 504 /* 0x1f8 */,
	HAL_MACRX_MU_UPLINK_USER_SETUP_SNIFF                   = 505 /* 0x1f9 */,
	HAL_MACRX_MU_UPLINK_USER_SEL_SNIFF                     = 506 /* 0x1fa */,
	HAL_MACRX_MU_UPLINK_FCS_STATUS_SNIFF                   = 507 /* 0x1fb */,
	HAL_MACTX_PREFETCH_CV_DMA                              = 508 /* 0x1fc */,
	HAL_MACTX_PREFETCH_CV_PER_USER                         = 509 /* 0x1fd */,
	HAL_PHYRX_OTHER_RECEIVE_INFO_ALL_SIGB_DETAILS          = 510 /* 0x1fe */,
	HAL_MACTX_BF_PARAMS_UPDATE_COMMON                      = 511 /* 0x1ff */,
	HAL_MACTX_BF_PARAMS_UPDATE_PER_USER                    = 512 /* 0x200 */,
	HAL_RANGING_USER_DETAILS                               = 513 /* 0x201 */,
	HAL_PHYTX_CV_CORR_STATUS                               = 514 /* 0x202 */,
	HAL_PHYTX_CV_CORR_COMMON                               = 515 /* 0x203 */,
	HAL_PHYTX_CV_CORR_USER                                 = 516 /* 0x204 */,
	HAL_MACTX_CV_CORR_COMMON                               = 517 /* 0x205 */,
	HAL_MACTX_CV_CORR_MAC_INFO_GROUP                       = 518 /* 0x206 */,
	HAL_BW_PUNCTURE_EVAL_WRAPPER                           = 519 /* 0x207 */,
	HAL_MACTX_RX_NOTIFICATION_FOR_PHY                      = 520 /* 0x208 */,
	HAL_MACTX_TX_NOTIFICATION_FOR_PHY                      = 521 /* 0x209 */,
	HAL_MACTX_MU_UPLINK_COMMON_PER_BW                      = 522 /* 0x20a */,
	HAL_MACTX_MU_UPLINK_USER_SETUP_PER_BW                  = 523 /* 0x20b */,
	HAL_RX_PPDU_END_USER_STATS_EXT2                        = 524 /* 0x20c */,
	HAL_FW2SW_MON                                          = 525 /* 0x20d */,
	HAL_WSI_DIRECT_MESSAGE                                 = 526 /* 0x20e */,
	HAL_MACTX_EMLSR_PRE_SWITCH                             = 527 /* 0x20f */,
	HAL_MACTX_EMLSR_SWITCH                                 = 528 /* 0x210 */,
	HAL_MACTX_EMLSR_SWITCH_BACK                            = 529 /* 0x211 */,
	HAL_PHYTX_EMLSR_SWITCH_ACK                             = 530 /* 0x212 */,
	HAL_PHYTX_EMLSR_SWITCH_BACK_ACK                        = 531 /* 0x213 */,
	HAL_EVENT_DETAILS                                      = 546 /* 0x222 */,
	HAL_MACTX_UHR_SIG_USR_SU                               = 578 /* 0x242 */,
	HAL_MACTX_UHR_SIG_USR_MU_MIMO                          = 579 /* 0x243 */,
	HAL_MACTX_UHR_SIG_USR_OFDMA                            = 580 /* 0x244 */,
	HAL_MACTX_ELR_SIG_UHR_SU                               = 581 /* 0x245 */,
	HAL_COEX_BLACKOUT_DETAILS                              = 582 /* 0x246 */,
	HAL_PEER_BLACKOUT_DETAILS                              = 583 /* 0x247 */,
	HAL_MACTX_OTHER_TRANSMIT_INFO_RTT_SELF_CAL             = 584 /* 0x248 */,
	HAL_HDR_CTRL_PROT_PEER_ENTRY                           = 585 /* 0x249 */,
	HAL_HDR_CTRL_PROT_MPDU_START                           = 586 /* 0x24a */,
	HAL_HDR_CTRL_PROT_DATA                                 = 587 /* 0x24b */,
	HAL_HDR_CTRL_PROT_MPDU_END                             = 588 /* 0x24c */,
	HAL_HDR_CTRL_PROT_MIC_STATUS                           = 589 /* 0x24d */,
	HAL_HDR_CTRL_PROT_PEER_INFO                            = 590 /* 0x24e */,
	HAL_HDR_CTRL_PROT_PEER_ENTRY_DONE                      = 591 /* 0x24f */,
	HAL_MACTX_U_SIG_UHR_ELR_SU                             = 592 /* 0x250 */,
	HAL_MACTX_U_SIG_UHR_COBF                               = 593 /* 0x251 */,
	HAL_MACTX_UHR_SIG_USR_COBF                             = 594 /* 0x252 */,
	HAL_TQM_GEN_AND_LIST_MPDUS                             = 595 /* 0x253 */,
	HAL_TQM_ADD_MSDU                                       = 596 /* 0x254 */,
	HAL_TLV_ROUTING_CTRL_CFG                               = 597 /* 0x255 */,
	HAL_MCSS_SETUP                                         = 598 /* 0x256 */,
	HAL_TQM_SAM_MPDU_QUEUE_STATUS                          = 599 /* 0x257 */,
	HAL_TQM_SAM_MSDU_QUEUE_STATUS                          = 600 /* 0x258 */,
	HAL_HWSCH_SAM_STA_PWR_STATE                            = 601 /* 0x259 */,
	HAL_HWSCH_SAM_ARBITRATION_REQUEST                      = 602 /* 0x25a */,
	HAL_FW_SAM_BITMAP_REQUEST                              = 603 /* 0x25b */,
	HAL_FW_SAM_MPDU_QUEUE_PROGRAMMING                      = 604 /* 0x25c */,
	HAL_FW_SAM_MSDU_QUEUE_PROGRAMMING                      = 605 /* 0x25d */,
	HAL_FW_SAM_PEER_PROGRAMMING                            = 606 /* 0x25e */,
	HAL_SW_SAM_TOKEN_BUCKET_PROGRAMMING                    = 607 /* 0x25f */,
	HAL_SW_SAM_MPDU_QUEUE_CLEAR_PROGRAMMING                = 608 /* 0x260 */,
	HAL_SW_SAM_MSDU_QUEUE_CLEAR_PROGRAMMING                = 609 /* 0x261 */,
	HAL_SAM_CMD_STATUS                                     = 610 /* 0x262 */,
	HAL_SAM_HWSCH_ARBITRATION_RESPONSE                     = 611 /* 0x263 */,
	HAL_SAM_HWSCH_HW_CMD_AVAILABLE_STATUS                  = 612 /* 0x264 */,
	HAL_SAM_FW_BITMAP_REQUEST_RESPONSE                     = 613 /* 0x265 */,
	HAL_SAM_SAM_BITMAP_REQUEST_RESPONSE                    = 614 /* 0x266 */,
	HAL_SAM_FIRST_SUMMARY_BITMAP_CHANGE_SEEN               = 615 /* 0x267 */,
	HAL_SAM_BITMAP_REQUEST_SUMMARY                         = 616 /* 0x268 */,
	HAL_SAM_MPDU_QUEUE_SUMMARY                             = 617 /* 0x269 */,
	HAL_SAM_MSDU_QUEUE_SUMMARY                             = 618 /* 0x26a */,
	HAL_SAM_MPDU_QUEUE_BITMAP                              = 619 /* 0x26b */,
	HAL_SAM_MSDU_QUEUE_BITMAP                              = 620 /* 0x26c */,
	HAL_SAM_MPDU_QUEUE_DETAILS                             = 621 /* 0x26d */,
	HAL_SAM_MSDU_QUEUE_DETAILS                             = 622 /* 0x26e */,
	HAL_SAM_TOKEN_BUCKET_DETAILS                           = 623 /* 0x26f */,
	HAL_SAM_PEER_PS_STATE_BITMAP                           = 624 /* 0x270 */,
	HAL_SAM_CMD_TLV_OVERWRITE                              = 625 /* 0x271 */,
	HAL_SAM_CMD_FIELD_OVERWRITE                            = 626 /* 0x272 */,
	HAL_WSI_HWSCH_SAM_ARBITRATION_REQUEST                  = 627 /* 0x273 */,
	HAL_WSI_HWSCH_SAM_ARBITRATION_RESPONSE                 = 628 /* 0x274 */,
	HAL_WSI_MLO_TS_SYNC_MSG                                = 629 /* 0x275 */,
	HAL_MACRX_PRECODING_CBF_READ_REQUEST                   = 630 /* 0x276 */,
	HAL_MACRX_PRECODING_CBF_DATA_REQUEST                   = 631 /* 0x277 */,
	HAL_PHYRX_PRECODING_CBF_READ_REQUEST_ACK               = 632 /* 0x278 */,
	HAL_MCSS_TO_FW_MSG                                     = 633 /* 0x279 */,
	HAL_PHYRX_PRECODING_CBF_TRANSFER_DONE                  = 634 /* 0x27a */,
	HAL_PHYRX_PRECODING_CBF_TRANSFER_ABORT                 = 635 /* 0x27b */,
	HAL_MULTI_PEER_BLACKOUT_DETAILS                        = 636 /* 0x27c */,
	HAL_TQM_FW_COMPLETION                                  = 637 /* 0x27d */,
	HAL_MCSS_OFDMA_SETUP                                   = 638 /* 0x27e */,
	HAL_MCSS_C_TDMA_C_RTWT_SETUP                           = 639 /* 0x27f */,
	HAL_NO_SAM_TEMPLATE_OVERWRITE_BEFORE_THIS_POINT        = 640 /* 0x280 */,
	HAL_FW_SAM_GROUP_PROPERTIES_PROGRAMMING                = 641 /* 0x281 */,
	HAL_MCSS_CO_BF_SETUP                                   = 642 /* 0x282 */,
	HAL_PHYRX_RSSI_LEGACY_PART2                            = 643 /* 0x283 */,
	HAL_HWSCH_SAM_REWIND_LAST_ARBITRATION                  = 644 /* 0x284 */,
	HAL_MACTX_U_SIG_UHR_COSR                               = 645 /* 0x285 */,
	HAL_MACTX_UHR_SIG_USR_COSR                             = 646 /* 0x286 */,
	HAL_SW_SAM_PEER_CLEAR_PROGRAMMING                      = 647 /* 0x287 */,
	HAL_OVERWRITE_CFP_INFO                                 = 648 /* 0x288 */,
	HAL_MACTX_OTHER_TRANSMIT_INFO_ENABLE_LONG_DISTANCE     = 649 /* 0x289 */,
	HAL_TQM_FES_SYNC_CMD                                   = 650 /* 0x28a */,
	HAL_PHYRX_UHR_SIG_USR_OFDMA                            = 651 /* 0x28b */,
	HAL_TCL_DATA_CMD                                       = 1023,
	HAL_TLV_BASE                                           = 1024 /* 0x3ff */
};

#define HAL_RX_MPDU_DESC_INFO_INFO0_MSDU_COUNT				GENMASK(7, 0)
#define HAL_RX_MPDU_DESC_INFO_INFO0_FRAGMENT_FLAG			BIT(8)
#define HAL_RX_MPDU_DESC_INFO_INFO0_BAR_FRAME_FLAG			BIT(11)
#define HAL_RX_MPDU_DESC_INFO_INFO0_PN_FIELDS_CONTAIN_VALID_INFO	BIT(12)
#define HAL_RX_MPDU_DESC_INFO_INFO0_RAW_MPDU				BIT(13)
#define HAL_RX_MPDU_DESC_INFO_INFO0_TID					GENMASK(31, 28)

struct hal_rx_mpdu_desc {
	__le32 info0;
	__le32 peer_meta_data;
} __packed;

/* hal_rx_mpdu_desc
 *		Producer: RXDMA
 *		Consumer: REO/SW/FW
 *
 * msdu_count
 *		The number of MSDUs within the MPDU
 *
 * fragment_flag
 *		When set, this MPDU is a fragment and REO should forward this
 *		fragment MPDU to the REO destination ring without any reorder
 *		checks, pn checks or bitmap update. This implies that REO is
 *		forwarding the pointer to the MSDU link descriptor.
 *
 * mpdu_retry_bit
 *		The retry bit setting from the MPDU header of the received frame
 *
 * ampdu_flag
 *		Indicates the MPDU was received as part of an A-MPDU.
 *
 * bar_frame
 *		Indicates the received frame is a BAR frame. After processing,
 *		this frame shall be pushed to SW or deleted.
 *
 * valid_pn
 *		When not set, REO will not perform a PN sequence number check.
 *
 * raw_mpdu
 *		Field only valid when first_msdu_in_mpdu_flag is set. Indicates
 *		the contents in the MSDU buffer contains a 'RAW' MPDU. This
 *		'RAW' MPDU might be spread out over multiple MSDU buffers.
 *
 * more_fragment_flag
 *		The More Fragment bit setting from the MPDU header of the
 *		received frame
 *
 * src_info
 *		Source (Virtual) device/interface info associated with this peer.
 *		This field gets passed on by REO to PPE in the EDMA descriptor.
 *
 * mpdu_qos_control_valid
 *		When set, the MPDU has a QoS control field
 *
 * tid
 *		Field only valid when mpdu_qos_control_valid is set
 */

enum hal_rx_msdu_desc_reo_dest_ind {
	HAL_RX_MSDU_DESC_REO_DEST_IND_SW0,
	HAL_RX_MSDU_DESC_REO_DEST_IND_SW1,
	HAL_RX_MSDU_DESC_REO_DEST_IND_SW2,
	HAL_RX_MSDU_DESC_REO_DEST_IND_SW3,
	HAL_RX_MSDU_DESC_REO_DEST_IND_SW4,
	HAL_RX_MSDU_DESC_REO_DEST_IND_RELEASE,
	HAL_RX_MSDU_DESC_REO_DEST_IND_FW,
	HAL_RX_MSDU_DESC_REO_DEST_IND_SW5,
	HAL_RX_MSDU_DESC_REO_DEST_IND_SW6,
	HAL_RX_MSDU_DESC_REO_DEST_IND_SW7,
	HAL_RX_MSDU_DESC_REO_DEST_IND_SW8,
	HAL_RX_MSDU_DESC_REO_DEST_IND_SW9,
	HAL_RX_MSDU_DESC_REO_DEST_IND_PPE,
	HAL_RX_MSDU_DESC_REO_DEST_IND_PPE1,
	HAL_RX_MSDU_DESC_REO_DEST_IND_PPE2,
	HAL_RX_MSDU_DESC_REO_DEST_IND_FW_MGMT,
	HAL_RX_MSDU_DESC_REO_DEST_IND_SW10,
	HAL_RX_MSDU_DESC_REO_DEST_IND_SW11,
};

#define HAL_RX_MSDU_DESC_INFO_INFO0_FIRST_MSDU_IN_MPDU_FLAG		BIT(0)
#define HAL_RX_MSDU_DESC_INFO_INFO0_LAST_MSDU_IN_MPDU_FLAG		BIT(1)
#define HAL_RX_MSDU_DESC_INFO_INFO0_MSDU_CONTINUATION			BIT(2)
#define HAL_RX_MSDU_DESC_INFO_INFO0_MSDU_LENGTH_OR_FLOW_IDX_LSB		GENMASK(16, 3)
#define HAL_RX_MSDU_DESC_INFO_INFO0_SA_IS_VALID				BIT(18)
#define HAL_RX_MSDU_DESC_INFO_INFO0_DA_IS_VALID				BIT(19)

#define HAL_RX_MSDU_PKT_LENGTH_GET(val)		\
	(u32_get_bits((val), HAL_RX_MSDU_DESC_INFO_INFO0_MSDU_LENGTH_OR_FLOW_IDX_LSB))

struct hal_rx_msdu_desc {
	__le32 info0;
} __packed;

#define HAL_RX_MSDU_EXT_DESC_INFO_INFO0_REO_DESTINATION_INDICATION	GENMASK(4, 0)
struct hal_rx_msdu_ext_desc_info {
	__le32 info0;
	__le32 rsvd0;
} __packed;

enum hal_reo_dest_rel_src_module {
	HAL_REO_REL_SRC_MODULE_RXDMA = 1,
	HAL_REO_REL_SRC_MODULE_REO = 2,
	HAL_REO_REL_SRC_MODULE_FW = 5,
	HAL_REO_REL_SRC_MODULE_SW = 6,
};

struct hal_rx_msdu_stream_desc_info {
	__le16 rsvd0;
} __packed;

#define HAL_RX_MPDU_EXT_DESC_INFO_INFO0_MGMT_PKT			BIT(0)
#define HAL_RX_MPDU_EXT_DESC_INFO_INFO0_RXDMA_PUSH_REASON		GENMASK(2, 1)
#define HAL_RX_MPDU_EXT_DESC_INFO_INFO0_RXDMA_ERROR_CODE		GENMASK(7, 3)
#define HAL_RX_MPDU_EXT_DESC_INFO_INFO0_REO_DEST_BUFFER_TYPE		BIT(8)
#define HAL_RX_MPDU_EXT_DESC_INFO_INFO0_RELEASE_SOURCE_MODULE		GENMASK(11, 9)
#define HAL_RX_MPDU_EXT_DESC_INFO_INFO0_SRC_LINK_ID			GENMASK(21, 19)
#define HAL_RX_MPDU_EXT_DESC_INFO_INFO0_REO_PUSH_REASON			GENMASK(23, 22)
#define HAL_RX_MPDU_EXT_DESC_INFO_INFO0_REO_ERROR_CODE			GENMASK(28, 24)

struct hal_rx_mpdu_ext_desc_info {
	__le32 info0;
};

#define HAL_REO_DESTINATION_RING_INFO0_COOKIE_CONVERSION_STATUS		BIT(0)
#define HAL_REO_DESTINATION_RING_INFO0_SW_BUFFER_COOKIE			GENMASK(21, 2)

struct hal_reo_dest_ring {
	struct ath12k_buffer_addr buf_addr_info;
	struct hal_rx_mpdu_desc rx_mpdu_info;
	struct hal_rx_mpdu_ext_desc_info rx_mpdu_ext_info;
	union {
		struct hal_rx_msdu_desc rx_msdu_info;
		__le32 info2;
	};
	__le32 info0; /* %HAL_REO_DEST_RING_INFO0_ */
	struct hal_rx_msdu_stream_desc_info rx_msdu_stream_info;
	__le16 rsvd0;
} __packed;

struct hal_reo_to_ppe_ring {
	__le32 buffer_addr_lo;
	__le32 rsvd0;
	__le32 opaque_lo;
	__le32 opaque_hi;
	__le16 src_info;
	__le16 dst_info;
	__le16 data_length;
	__le16 rsvd1;
	__le16 rsvd2;
	__le16 mss_hash_value_ptp_tag;
	__le32 rsvd3;
} __packed;

enum hal_reo_entr_rxdma_push_reason {
	HAL_REO_ENTR_RING_RXDMA_PUSH_REASON_ERR_DETECTED,
	HAL_REO_ENTR_RING_RXDMA_PUSH_REASON_ROUTING_INSTRUCTION,
	HAL_REO_ENTR_RING_RXDMA_PUSH_REASON_RX_FLUSH,
};

enum hal_rx_reo_dest_ring {
	HAL_RX_REO_DEST_RING_TCL,
	HAL_RX_REO_DEST_RING_SW1,
	HAL_RX_REO_DEST_RING_SW2,
	HAL_RX_REO_DEST_RING_SW3,
	HAL_RX_REO_DEST_RING_SW4,
	HAL_RX_REO_DEST_RING_RELEASE,
	HAL_RX_REO_DEST_RING_FW,
	HAL_RX_REO_DEST_RING_SW5,
	HAL_RX_REO_DEST_RING_SW6,
	HAL_RX_REO_DEST_RING_SW7,
	HAL_RX_REO_DEST_RING_SW8,
};

#define HAL_RX_REO_MPDU_DESC_INFO_INFO0_MSDU_COUNT			GENMASK(7, 0)
#define HAL_RX_REO_MPDU_DESC_INFO_INFO1_SRC_LINK_ID			GENMASK(12, 10)

struct hal_rx_reo_mpdu_desc_info {
	__le32 info0;
	__le32 peer_meta_data;
	__le32 info1;
	__le32 pn_31_0;
	__le16 pn_47_32;
	__le16 rsvd0;
	__le32 rx_preamble_timestamp;
	__le16 phy_lmac_latency;
	__le16 rsvd1;
} __packed;

#define HAL_REO_ENTRANCE_RING_INFO0_RX_REO_QUEUE_DESC_ADDR_39_32	GENMASK(7, 0)
#define HAL_REO_ENTRANCE_RING_INFO0_REO_DESTINATION_INDICATION		GENMASK(26, 22)

#define HAL_REO_ENTRANCE_RING_INFO1_MPDU_SEQUENCE_NUMBER		GENMASK(30, 19)

struct hal_reo_entrance_ring {
	struct ath12k_buffer_addr buf_addr_info;
	struct hal_rx_reo_mpdu_desc_info rx_reo_mpdu_info;
	__le32 rx_reo_queue_desc_addr_31_0;
	__le32 info0;
	__le32 info1;
	__le16 phy_ppdu_id;
	__le16 rsvd0;
	__le16 receive_queue_number;
	__le16 rsvd1;
	__le32 rsvd2[2];
} __packed;

#define HAL_REO_CMD_HDR_INFO0_STATUS_REQUIRED	BIT(0)

struct hal_reo_cmd_hdr {
	__le16 reo_cmd_number;
	__le16 info0;
} __packed;

#define HAL_REO_GET_QUEUE_STATS_INFO0_QUEUE_ADDR_HI	GENMASK(7, 0)
#define HAL_REO_GET_QUEUE_STATS_INFO0_CLEAR_STATS	BIT(8)

struct hal_reo_get_queue_stats {
	struct hal_reo_cmd_hdr cmd;
	__le32 queue_addr_lo;
	__le16 info0;
	__le16 rsvd0;
	__le32 rsvd1[7];
} __packed;

/* hal_reo_get_queue_stats
 *		Producer: SW
 *		Consumer: REO
 *
 * cmd
 *		Details for command execution tracking purposes.
 *
 * queue_addr_lo
 *		Address (lower 32 bits) of the REO queue descriptor.
 *
 * queue_addr_hi
 *		Address (upper 8 bits) of the REO queue descriptor.
 *
 * clear_stats
 *		Clear stats settings. When set, Clear the stats after
 *		generating the status.
 *
 *		Following stats will be cleared.
 *		Timeout_count
 *		Forward_due_to_bar_count
 *		Duplicate_count
 *		Frames_in_order_count
 *		BAR_received_count
 *		MPDU_Frames_processed_count
 *		MSDU_Frames_processed_count
 *		Total_processed_byte_count
 *		Late_receive_MPDU_count
 *		window_jump_2k
 *		Hole_count
 */

#define HAL_REO_FLUSH_QUEUE_INFO0_DESC_ADDR_HI		GENMASK(7, 0)

struct hal_reo_flush_queue {
	struct hal_reo_cmd_hdr cmd;
	__le32 desc_addr_lo;
	__le16 info0;
	__le16 rsvd0;
	__le32 rsvd1[7];
} __packed;

#define HAL_REO_FLUSH_CACHE_INFO0_CACHE_ADDR_HI		GENMASK(7, 0)
#define HAL_REO_FLUSH_CACHE_INFO0_FWD_ALL_MPDUS		BIT(8)
#define HAL_REO_FLUSH_CACHE_INFO0_BLOCK_RESRC_IDX	GENMASK(11, 10)
#define HAL_REO_FLUSH_CACHE_INFO0_FLUSH_WO_INVALIDATE	BIT(12)
#define HAL_REO_FLUSH_CACHE_INFO0_BLOCK_CACHE_USAGE	BIT(13)
#define HAL_REO_FLUSH_CACHE_INFO0_FLUSH_ALL		BIT(14)
#define HAL_REO_FLUSH_CACHE_INFO0_FLUSH_QUEUE_1K_DESC	BIT(15)

struct hal_reo_flush_cache {
	struct hal_reo_cmd_hdr cmd;
	__le32 cache_addr_lo;
	__le16 info0;
	__le16 rsvd0;
	__le32 rsvd1[7];
} __packed;

#define HAL_TCL_DATA_CMD_INFO0_BUF_OR_EXT_DESC_TYPE		BIT(1)
#define HAL_TCL_DATA_CMD_INFO0_BANK_ID				GENMASK(9, 2)
#define HAL_TCL_DATA_CMD_INFO0_VDEV_ID				GENMASK(17, 10)
#define HAL_TCL_DATA_CMD_INFO0_DATA_LENGTH			GENMASK(31, 18)

#define HAL_TCL_DATA_CMD_INFO1_CACHE_SET_NUM			GENMASK(3, 0)
#define HAL_TCL_DATA_CMD_INFO1_INDEX_LOOKUP_OVERRIDE		BIT(4)
#define HAL_TCL_DATA_CMD_INFO1_HLOS_TID				GENMASK(8, 5)
#define HAL_TCL_DATA_CMD_INFO1_HLOS_TID_OVERWRITE		BIT(9)
#define HAL_TCL_DATA_CMD_INFO1_HEADER_LENGTH_READ_SEL		BIT(10)

#define HAL_TCL_DATA_CMD_INFO2_TO_FW_SW				GENMASK(1, 0)
#define HAL_TCL_DATA_CMD_INFO2_IPV4_CHECKSUM_EN			BIT(2)
#define HAL_TCL_DATA_CMD_INFO2_UDP_OVER_IPV4_CHECKSUM_EN	BIT(3)
#define HAL_TCL_DATA_CMD_INFO2_L4_CHECKSUM_EN			BIT(4)
#define HAL_TCL_DATA_CMD_INFO2_MSDU_COLOR			GENMASK(7, 6)
#define HAL_TCL_DATA_CMD_INFO2_FLOW_OVERRIDE_ENABLE		BIT(8)
#define HAL_TCL_DATA_CMD_INFO2_WHO_CLASSIFY_INFO_SEL		GENMASK(10, 9)
#define HAL_TCL_DATA_CMD_INFO2_RX_TIMESTAMP_FORMAT		BIT(11)
#define HAL_TCL_DATA_CMD_INFO2_RX_TIMESTAMP			GENMASK(30, 12)
#define HAL_TCL_DATA_CMD_INFO2_RX_TIMESTAMP_VALID		BIT(31)

#define HAL_TCL_DATA_CMD_INFO3_TX_NOTIFY_FRAME			GENMASK(2, 0)
#define HAL_TCL_DATA_CMD_INFO3_FLOW_SELECT			BIT(3)
#define HAL_TCL_DATA_CMD_INFO3_METADATA_LENGTH			GENMASK(12, 4)
#define HAL_TCL_DATA_CMD_INFO3_LINK_ID				GENMASK(15, 13)
#define HAL_TCL_DATA_CMD_INFO4_TX_ENQUEUE_TIMESTAMP		GENMASK(18, 0)
#define HAL_TCL_DATA_CMD_INFO4_TX_ENQUEUE_TIMESTAMP_VALID	BIT(19)
#define HAL_TCL_DATA_CMD_INFO4_TELEMETRY_STREAM_ID_VALID	BIT(23)
#define HAL_TCL_DATA_CMD_INFO4_TELEMETRY_STREAM_ID		GENMASK(31, 24)
#define HAL_TCL_DATA_CMD_INFO5_INSERT_VLAN_TCI_OVERRIDE_EN	BIT(0)
#define HAL_TCL_DATA_CMD_INFO5_RING_ID				GENMASK(11, 4)
#define HAL_TCL_DATA_CMD_INFO5_LOOPING_COUNT			GENMASK(15, 12)

struct hal_tcl_data_cmd {
	struct ath12k_buffer_addr buf_addr_info;
	__le32 info0;
	__le16 search_index;
	__le16 info1;
	__le32 info2;
	__le16 info3;
	__le16 tcl_cmd_number;
	__le32 info4;
	__le16 insert_vlan_tci_override_val;
	__le16 info5;
} __packed;

#define HAL_TCL_DESC_LEN sizeof(struct hal_tcl_data_cmd)

#define HAL_TX_RATE_TID_INFO_INFO0_TX_RATE_STATS_INFO_VALID	BIT(0)
#define HAL_TX_RATE_TID_INFO_INFO0_TRANSMIT_BW			GENMASK(3, 1)
#define HAL_TX_RATE_TID_INFO_INFO0_TRANSMIT_PKT_TYPE		GENMASK(7, 4)
#define HAL_TX_RATE_TID_INFO_INFO0_TRANSMIT_SGI			GENMASK(11, 10)
#define HAL_TX_RATE_TID_INFO_INFO0_TRANSMIT_MCS			GENMASK(16, 12)
#define HAL_TX_RATE_TID_INFO_INFO0_OFDMA_TRANSMISSION		BIT(20)
#define HAL_TX_RATE_TID_INFO_INFO0_TONES_IN_RU			GENMASK(24, 21)
#define HAL_TX_RATE_TID_INFO_INFO0_TID				GENMASK(31, 28)

enum hal_tx_rate_stats_bw {
	HAL_TX_RATE_STATS_BW_20,
	HAL_TX_RATE_STATS_BW_40,
	HAL_TX_RATE_STATS_BW_80,
	HAL_TX_RATE_STATS_BW_160,
	HAL_TX_RATE_STATS_BW_320,
	HAL_TX_RATE_STATS_BW_240
};

struct hal_tx_rate_tid_info {
	__le32 info0;
} __packed;

#define HAL_TCL_DESC_LEN sizeof(struct hal_tcl_data_cmd)
struct hal_tx_msdu_details {
	struct ath12k_buffer_addr buf_addr_info;
	__le32 rsvd0[2];
} __packed;


struct hal_tx_msdu_ts_desc_info {
	__le32 rsvd0;
} __packed;


struct hal_tx_msdu_ext_desc_info {
	__le16 rsvd0;
} __packed;


struct hal_tcl_entrance_from_ppe_ring {
	__le32 buffer_addr_lo;
	__le32 rsvd0;
	__le32 opaque_lo;
	__le16 flow_cookie_ext_1;
	__le16 vlan_tci;
	__le16 src_info;
	__le16 dst_info;
	__le32 rsvd1;
	__le16 rsvd2;
	__le16 hash_value;
	__le32 rsvd3;
} __packed;


struct hal_tqm_entrance_ring {
	struct hal_tx_msdu_details tx_msdu_info;
	__le32 flow_queue_addr_39_8;
	struct hal_tx_msdu_ts_desc_info tx_msdu_ts_desc_info;
	struct hal_tx_msdu_ext_desc_info tx_msdu_ext_desc_info;
	__le16 tqm_add_cmd_number;
	__le32 rsvd0;
} __packed;

#define HAL_TX_MSDU_EXT_INFO0_BUF_PTR_HI	GENMASK(7, 0)
#define HAL_TX_MSDU_EXT_INFO0_EXTN_OVERRIDE	BIT(8)
#define HAL_TX_MSDU_EXT_INFO0_ENCAP_TYPE	GENMASK(10, 9)
#define HAL_TX_MSDU_EXT_INFO0_ENCRYPT_TYPE	GENMASK(14, 11)

struct hal_tx_msdu_extension {
	__le32 rsvd0[6];
	__le32 buf0_ptr_31_0;
	__le16 info0;
	__le16 buf0_len;
	__le32 rsvd1[10];
} __packed;

#define HAL_TCL_GSE_CMD_INFO0_CONTROL_BUFFER_ADDR_39_32		GENMASK(7, 0)
#define HAL_TCL_GSE_CMD_INFO0_GSE_CTRL				GENMASK(11, 8)
#define HAL_TCL_GSE_CMD_INFO0_STATUS_DESTINATION_RING_ID	BIT(13)
#define HAL_TCL_GSE_CMD_INFO1_TCL_CMD_TYPE			BIT(0)

struct hal_tcl_gse_cmd {
	__le32 control_buffer_addr_31_0;
	__le32 info0;
	__le16 info1;
	__le16 rsvd0;
	__le32 cmd_meta_data_31_0;
	__le32 cmd_meta_data_63_32;
	__le32 rsvd1[3];
} __packed;

enum hal_ase_cache_op {
	HAL_ASE_CACHE_OP_INVALIDATE_SINGLE_ENTRY        = 4,
	HAL_ASE_CACHE_OP_INVALIDATE_ALL                 = 5,
};

enum hal_tcl_cache_op_res {
	HAL_TCL_CACHE_OP_RES_DONE,
	HAL_TCL_CACHE_OP_RES_NOT_FOUND,
	HAL_TCL_CACHE_OP_RES_TIMEOUT,
};

#define HAL_TCL_STATUS_RING_INFO0_GSE_CTRL		GENMASK(3, 0)

struct hal_tcl_status_ring {
	__le32 info0;
	__le32 msdu_byte_count;
	__le32 msdu_timestamp;
	__le32 cmd_meta_data_31_0;
	__le32 cmd_meta_data_63_32;
	__le32 rsvd0;
	__le16 status_debug_data;
	__le16 rsvd1;
	__le32 rsvd2;
} __packed;

#define HAL_ASE_CMD_RING_INFO0_CMD_TO_CHIP_0	BIT(0)
#define HAL_ASE_CMD_RING_INFO0_CMD_TO_CHIP_1	BIT(1)
#define HAL_ASE_CMD_RING_INFO0_CMD_TO_CHIP_2	BIT(2)
#define HAL_ASE_CMD_RING_INFO0_CMD_TO_CHIP_3	BIT(3)
#define HAL_ASE_CMD_RING_INFO0_CMD_TO_CHIP_4	BIT(4)
#define HAL_ASE_CMD_RING_INFO1_IS_MCAST		BIT(0)
#define HAL_ASE_CMD_RING_INFO1_IS_MEC		BIT(1)
#define HAL_ASE_CMD_RING_INFO1_AD1_MATCH	BIT(2)
#define HAL_ASE_CMD_RING_INFO1_LINK_ID		GENMASK(5, 3)
#define HAL_ASE_CMD_RING_INFO1_GSE_CTRL		GENMASK(9, 6)
#define HAL_ASE_CMD_RING_INFO1_SEARCH_INDEX_EN	BIT(10)
#define HAL_ASE_CMD_RING_INFO1_CACHE_SET_NUM	GENMASK(15, 11)
#define HAL_ASE_CMD_RING_INFO2_SEARCH_INDEX	GENMASK(19, 0)

struct hal_ase_cmd {
	__le16 info0;
	__le16 rsvd0;
	__le32 mac_addr_31_0;
	__le16 mac_addr_47_32;
	__le16 info1;
	__le32 cmd_meta_data_31_0;
	__le32 rsvd1[3];
	__le32 info2;
} __packed;

#define HAL_ASE_STATUS_RING_INFO0_GSE_CTRL		GENMASK(3, 0)
#define HAL_ASE_STATUS_RING_INFO0_ASE_FSE_SEL		BIT(4)
#define HAL_ASE_STATUS_RING_INFO0_CACHE_OP_RES		GENMASK(6, 5)
#define HAL_ASE_STATUS_RING_INFO0_INDEX_SEARCH_EN	BIT(7)
#define HAL_ASE_STATUS_RING_INFO0_MSDU_CNT_N		GENMASK(31, 8)

#define HAL_ASE_STATUS_RING_INFO1_HASH_INDX_VAL		GENMASK(19, 0)
#define HAL_ASE_STATUS_RING_INFO1_CACHE_SET_NUM		GENMASK(24, 20)
#define HAL_ASE_STATUS_RING_INFO2_RING_ID		GENMASK(11, 4)
#define HAL_ASE_STATUS_RING_INFO2_LOOPING_COUNT		GENMASK(15, 12)

struct hal_ase_status {
	__le32 info0;
	__le32 msdu_byte_cnt_n;
	__le32 msdu_timestmp_n;
	__le32 cmd_meta_data_31_0;
	__le32 cmd_meta_data_63_32;
	__le32 info1;
	__le32 rsvd0;
	__le16 rsvd1;
	__le16 info2;
} __packed;

#define HAL_CE_SRC_DESC_INFO0_ADDR_HI		GENMASK(7, 0)
#define HAL_CE_SRC_DESC_INFO0_BYTE_SWAP		BIT(9)
#define HAL_CE_SRC_DESC_INFO0_GATHER		BIT(11)

struct hal_ce_srng_src_desc {
	__le32 buffer_addr_low;
	__le16 info0;
	__le16 length;
	__le16 fw_metadata;
	__le16 rsvd0;
	__le32 rsvd1;
} __packed;

/* hal_ce_srng_src_desc
 *
 * buffer_addr_lo
 *		LSB 32 bits of the 40 Bit Pointer to the source buffer
 *
 * buffer_addr_hi
 *		MSB 8 bits of the 40 Bit Pointer to the source buffer
 *
 * toeplitz_en
 *		Enable generation of 32-bit Toeplitz-LFSR hash for
 *		data transfer. In case of gather field in first source
 *		ring entry of the gather copy cycle in taken into account.
 *
 * src_swap
 *		Treats source memory organization as big-endian. For
 *		each dword read (4 bytes), the byte 0 is swapped with byte 3
 *		and byte 1 is swapped with byte 2.
 *		In case of gather field in first source ring entry of
 *		the gather copy cycle in taken into account.
 *
 * dest_swap
 *		Treats destination memory organization as big-endian.
 *		For each dword write (4 bytes), the byte 0 is swapped with
 *		byte 3 and byte 1 is swapped with byte 2.
 *		In case of gather field in first source ring entry of
 *		the gather copy cycle in taken into account.
 *
 * gather
 *		Enables gather of multiple copy engine source
 *		descriptors to one destination.
 *
 * ce_res_0
 *		Reserved
 *
 *
 * length
 *		Length of the buffer in units of octets of the current
 *		descriptor
 *
 * fw_metadata
 *		Meta data used by FW.
 *		In case of gather field in first source ring entry of
 *		the gather copy cycle in taken into account.
 *
 * ce_res_1
 *		Reserved
 *
 * ce_res_2
 *		Reserved
 *
 * ring_id
 *		The buffer pointer ring ID.
 *		0 refers to the IDLE ring
 *		1 - N refers to other rings
 *		Helps with debugging when dumping ring contents.
 *
 * looping_count
 *		A count value that indicates the number of times the
 *		producer of entries into the Ring has looped around the
 *		ring.
 *
 *		At initialization time, this value is set to 0. On the
 *		first loop, this value is set to 1. After the max value is
 *		reached allowed by the number of bits for this field, the
 *		count value continues with 0 again.
 *
 *		In case SW is the consumer of the ring entries, it can
 *		use this field to figure out up to where the producer of
 *		entries has created new entries. This eliminates the need to
 *		check where the head pointer' of the ring is located once
 *		the SW starts processing an interrupt indicating that new
 *		entries have been put into this ring...
 *
 *		Also note that SW if it wants only needs to look at the
 *		LSB bit of this count value.
 */

#define HAL_CE_DEST_DESC_INFO0_ADDR_HI		GENMASK(7, 0)

struct hal_ce_srng_dest_desc {
	__le32 buffer_addr_low;
	__le16 info0;
	__le16 rsvd0;
} __packed;

/* hal_ce_srng_dest_desc
 *
 * dst_buffer_low
 *		LSB 32 bits of the 40 Bit Pointer to the Destination
 *		buffer
 *
 * dst_buffer_high
 *		MSB 8 bits of the 40 Bit Pointer to the Destination
 *		buffer
 *
 * ce_res_4
 *		Reserved
 *
 * ring_id
 *		The buffer pointer ring ID.
 *		0 refers to the IDLE ring
 *		1 - N refers to other rings
 *		Helps with debugging when dumping ring contents.
 *
 * looping_count
 *		A count value that indicates the number of times the
 *		producer of entries into the Ring has looped around the
 *		ring.
 *
 *		At initialization time, this value is set to 0. On the
 *		first loop, this value is set to 1. After the max value is
 *		reached allowed by the number of bits for this field, the
 *		count value continues with 0 again.
 *
 *		In case SW is the consumer of the ring entries, it can
 *		use this field to figure out up to where the producer of
 *		entries has created new entries. This eliminates the need to
 *		check where the head pointer' of the ring is located once
 *		the SW starts processing an interrupt indicating that new
 *		entries have been put into this ring...
 *
 *		Also note that SW if it wants only needs to look at the
 *		LSB bit of this count value.
 */

struct hal_ce_srng_dst_status_desc {
	__le16 rsvd0;
	__le16 length;
	__le32 toeplitz_hash0;
	__le32 toeplitz_hash1;
	__le16 fw_metadata;
	__le16 rsvd1;
} __packed;

/* hal_ce_srng_dst_status_desc
 *
 * ce_res_5
 *		Reserved
 *
 * toeplitz_en
 *
 * src_swap
 *		Source memory buffer swapped
 *
 * dest_swap
 *		Destination  memory buffer swapped
 *
 * gather
 *		Gather of multiple copy engine source descriptors to one
 *		destination enabled
 *
 * ce_res_6
 *		Reserved
 *
 * length
 *		Sum of all the Lengths of the source descriptor in the
 *		gather chain
 *
 * toeplitz_hash_0
 *		32 LS bits of 64 bit Toeplitz LFSR hash result
 *
 * toeplitz_hash_1
 *		32 MS bits of 64 bit Toeplitz LFSR hash result
 *
 * fw_metadata
 *		Meta data used by FW
 *		In case of gather field in first source ring entry of
 *		the gather copy cycle in taken into account.
 *
 * ce_res_7
 *		Reserved
 *
 * ring_id
 *		The buffer pointer ring ID.
 *		0 refers to the IDLE ring
 *		1 - N refers to other rings
 *		Helps with debugging when dumping ring contents.
 *
 * looping_count
 *		A count value that indicates the number of times the
 *		producer of entries into the Ring has looped around the
 *		ring.
 *
 *		At initialization time, this value is set to 0. On the
 *		first loop, this value is set to 1. After the max value is
 *		reached allowed by the number of bits for this field, the
 *		count value continues with 0 again.
 *
 *		In case SW is the consumer of the ring entries, it can
 *		use this field to figure out up to where the producer of
 *		entries has created new entries. This eliminates the need to
 *		check where the head pointer' of the ring is located once
 *		the SW starts processing an interrupt indicating that new
 *		entries have been put into this ring...
 *
 *		Also note that SW if it wants only needs to look at the
 *			LSB bit of this count value.
 */

struct hal_tx_rate_stats_info {
	__le32 rsvd0[3];
} __packed;

enum hal_tqm_rel_src_module {
	HAL_TQM_REL_SRC_MODULE_TQM = 0,
	HAL_TQM_REL_SRC_MODULE_FW = 3,
	HAL_TQM_REL_SRC_MODULE_SW = 6,
	HAL_TQM_REL_SRC_MODULE_MAX,
};

enum hal_tqm_rel_desc_type {
	HAL_TQM_REL_DESC_TYPE_REL_MSDU,
	HAL_TQM_REL_DESC_TYPE_MSDU_LINK,
	HAL_TQM_REL_DESC_TYPE_MPDU_LINK,
	HAL_TQM_REL_DESC_TYPE_MSDU_EXT,
	HAL_TQM_REL_DESC_TYPE_QUEUE_EXT,
	HAL_TQM_REL_DESC_TYPE_RX_SAWF_DROPPED_MSDU,
};

#define HAL_TQM2SW_COMPLETION_RING_INFO0_RELEASE_SOURCE_MODULE		GENMASK(2, 0)
#define HAL_TQM2SW_COMPLETION_RING_INFO0_TQM_RELEASE_REASON		GENMASK(17, 13)

#define HAL_TQM2SW_COMPLETION_RING_INFO1_TQM_STATUS_NUMBER		GENMASK(23, 0)
#define HAL_TQM2SW_COMPLETION_RING_INFO1_TRANSMIT_COUNT			GENMASK(30, 24)

#define HAL_TQM2SW_COMPLETION_RING_INFO2_ACK_FRAME_RSSI			GENMASK(7, 0)
#define HAL_TQM2SW_COMPLETION_RING_INFO2_FIRST_MSDU			BIT(8)
#define HAL_TQM2SW_COMPLETION_RING_INFO2_LAST_MSDU			BIT(9)
#define HAL_TQM2SW_COMPLETION_RING_INFO2_NW_LATENCY			GENMASK(24, 13)
#define HAL_TQM2SW_COMPLETION_RING_INFO2_NW_LATENCY_VLD		BIT(25)

#define HAL_TQM2SW_COMPLETION_RING_INFO3_COOKIE_CONVERSION_STATUS	BIT(14)
#define HAL_TQM2SW_COMPLETION_RING_INFO3_TELE_STREAM_ID_VLD		BIT(15)

#define HAL_TQM2SW_COMPLETION_RING_INFO4_TELE_STREAM_ID		GENMASK(7, 0)
#define HAL_TQM2SW_COMPLETION_RING_INFO4_WIFI_SCHED_LATENCY		GENMASK(19, 8)

struct hal_tqm2sw_completion_ring {
	struct ath12k_buffer_addr buf_addr_info;
	__le32 info0;
	__le32 info1;
	__le32 info2;
	struct hal_tx_rate_tid_info rate_stats;
	__le16 sw_peer_id;
	__le16 info3;
	__le32 info4;
} __packed;

struct hal_wbm_link_desc {
	struct ath12k_buffer_addr buf_addr_info;
} __packed;

/* hal_wbm_link_desc
 *
 *	Producer: WBM
 *	Consumer: WBM
 *
 * buf_addr_info
 *		Details of the physical address of a buffer or MSDU
 *		link descriptor.
 */

enum hal_wbm_rel_desc_type {
	HAL_WBM_REL_DESC_TYPE_REL_MSDU,
	HAL_WBM_REL_DESC_TYPE_MSDU_LINK,
	HAL_WBM_REL_DESC_TYPE_MPDU_LINK,
	HAL_WBM_REL_DESC_TYPE_MSDU_EXT,
	HAL_WBM_REL_DESC_TYPE_QUEUE_EXT,
};

struct hal_wbm_buffer_ring {
	struct ath12k_buffer_addr buf_addr_info;
} __packed;

struct hal_sw_monitor_ring {
	struct ath12k_buffer_addr msdu_link_desc_addr_info;
	struct hal_rx_reo_mpdu_desc_info reo_level_mpdu_frame_info;
	struct ath12k_buffer_addr status_buff_addr_info;
	__le32 rsvd0;
	__le16 phy_ppdu_id;
	__le16 receive_queue_number;
	__le32 rsvd1[3];
} __packed;

enum hal_desc_owner {
	HAL_DESC_OWNER_WBM,
	HAL_DESC_OWNER_SW,
	HAL_DESC_OWNER_TQM,
	HAL_DESC_OWNER_RXDMA,
	HAL_DESC_OWNER_REO,
	HAL_DESC_OWNER_SWITCH,
};

enum hal_desc_buf_type {
	HAL_DESC_BUF_TYPE_TX_MSDU_LINK,
	HAL_DESC_BUF_TYPE_TX_MPDU_LINK,
	HAL_DESC_BUF_TYPE_TX_MPDU_QUEUE_HEAD,
	HAL_DESC_BUF_TYPE_TX_MPDU_QUEUE_EXT,
	HAL_DESC_BUF_TYPE_TX_FLOW,
	HAL_DESC_BUF_TYPE_TX_BUFFER,
	HAL_DESC_BUF_TYPE_RX_MSDU_LINK,
	HAL_DESC_BUF_TYPE_RX_MPDU_LINK,
	HAL_DESC_BUF_TYPE_RX_REO_QUEUE,
	HAL_DESC_BUF_TYPE_RX_REO_QUEUE_EXT,
	HAL_DESC_BUF_TYPE_RX_BUFFER,
	HAL_DESC_BUF_TYPE_IDLE_LINK,
};

#define HAL_DESC_REO_OWNED		4
#define HAL_DESC_REO_QUEUE_DESC		8
#define HAL_DESC_REO_QUEUE_EXT_DESC	10

#define HAL_DESC_HDR_INFO0_OWNER	GENMASK(3, 0)
#define HAL_DESC_HDR_INFO0_BUF_TYPE	GENMASK(7, 4)
#define HAL_DESC_HDR_INFO0_QUEUE_NUMBER	GENMASK(31, 8)

/* Indicates the MPDU queue ID to which this MPDU descriptor.
 * Field only valid if Buffer_type is any of Transmit_MPDU_*_descriptor
 * or Receive_MPDU_Link_descriptor
 */
#define HAL_DESC_HDR_INFO0_QUEUE_NUMBER	GENMASK(31, 8)
/*TODO: continue using this field as DBG_RESERVED as QUEUE_NUMBER is unused*/
#define HAL_DESC_HDR_INFO0_DBG_RESERVED	GENMASK(31, 8)

struct hal_desc_header {
	__le32 info0;
} __packed;

struct hal_rx_mpdu_link_ptr {
	struct ath12k_buffer_addr addr_info;
} __packed;

struct hal_rx_msdu_details {
	struct ath12k_buffer_addr buf_addr_info;
	struct hal_rx_msdu_desc rx_msdu_info;
	struct hal_rx_msdu_ext_desc_info rx_msdu_ext_info;
} __packed;

struct hal_rx_msdu_link {
	struct hal_desc_header desc_hdr;
	struct ath12k_buffer_addr buf_addr_info;
	struct hal_rx_msdu_details  msdu_0;
	struct hal_rx_msdu_stream_desc_info msdu_str_0;
	struct hal_rx_msdu_stream_desc_info msdu_str_1;
	struct hal_rx_msdu_details msdu_1;
	struct hal_rx_msdu_details msdu_2;
	struct hal_rx_msdu_stream_desc_info msdu_str_2;
	struct hal_rx_msdu_stream_desc_info msdu_str_3;
	struct hal_rx_msdu_details msdu_3;
	struct hal_rx_msdu_details msdu_4;
	struct hal_rx_msdu_stream_desc_info msdu_str_4;
	struct hal_rx_msdu_stream_desc_info msdu_str_5;
	struct hal_rx_msdu_details msdu_5;
	struct hal_rx_msdu_details msdu_6;
	struct hal_rx_msdu_stream_desc_info msdu_str_6;
	struct hal_rx_msdu_stream_desc_info msdu_str_7;
	struct hal_rx_msdu_details msdu_7;
	struct hal_rx_msdu_details msdu_8;
	struct hal_rx_msdu_stream_desc_info msdu_str_8;
	struct hal_rx_msdu_stream_desc_info msdu_str_9;
	struct hal_rx_msdu_details msdu_9;
	struct hal_rx_msdu_details msdu_10;
	struct hal_rx_msdu_stream_desc_info msdu_str_10;
	u16 rsvd0;
} __packed;

struct hal_rx_reo_queue_ext {
	struct hal_desc_header desc_hdr;
	__le32 rsvd;
	struct hal_rx_mpdu_link_ptr mpdu_link[31];
} __packed;

/* hal_rx_reo_queue_ext
 *	Consumer: REO
 *	Producer: REO
 *
 * descriptor_header
 *	Details about which module owns this struct.
 *
 * mpdu_link
 *	Pointer to the next MPDU_link descriptor in the MPDU queue.
 */

enum hal_rx_reo_queue_pn_size {
	HAL_RX_REO_QUEUE_PN_SIZE_24,
	HAL_RX_REO_QUEUE_PN_SIZE_48,
	HAL_RX_REO_QUEUE_PN_SIZE_128,
};

#define HAL_RX_REO_QUEUE_INFO0_VLD					BIT(0)
#define HAL_RX_REO_QUEUE_INFO0_ASSOCIATED_LINK_DESCRIPTOR_COUNTER	GENMASK(2, 1)
#define HAL_RX_REO_QUEUE_INFO0_LINK_LIST_TIMER_IDX			GENMASK(8, 5)
#define HAL_RX_REO_QUEUE_INFO0_RTY					BIT(10)
#define HAL_RX_REO_QUEUE_INFO0_BA_WINDOW_SIZE				GENMASK(22, 13)
#define HAL_RX_REO_QUEUE_INFO0_PN_CHECK_NEEDED				BIT(23)
#define HAL_RX_REO_QUEUE_INFO0_PN_SIZE					GENMASK(28, 27)
#define HAL_RX_REO_QUEUE_INFO0_IGNORE_AMPDU_FLAG			BIT(29)

#define HAL_RX_REO_QUEUE_INFO1_SVLD					BIT(0)
#define HAL_RX_REO_QUEUE_INFO1_SSN					GENMASK(12, 1)

#define HAL_RX_REO_QUEUE_INFO2_STATS_ID				GENMASK(10, 0)

struct hal_rx_reo_queue {
	struct hal_desc_header desc_hdr;
	__le16 receive_queue_number;
	__le16 rsvd0;
	__le32 info0;
	__le16 info1;
	__le16 rsvd1;
	__le32 pn_31_0;
	__le16 pn_47_32;
	__le16 rsvd2;
	__le32 rsvd3[2];
	__le32 last_rx_enqueue_timestamp;
	__le32 last_rx_dequeue_timestamp;
	__le32 ptr_to_next_aging_queue_31_0;
	__le32 rsvd4;
	__le32 ptr_to_previous_aging_queue_31_0;
	__le16 rsvd5;
	__le16 info2;
	__le32 rx_bitmap_31_0;
	__le32 rx_bitmap_63_32;
	__le32 rx_bitmap_95_64;
	__le32 rx_bitmap_127_96;
	__le32 rx_bitmap_159_128;
	__le32 rx_bitmap_191_160;
	__le32 rx_bitmap_223_192;
	__le32 rx_bitmap_255_224;
	__le32 rx_bitmap_287_256;
	__le32 rsvd6;
	__le16 rsvd7;
	__le16 duplicate_count;
	__le32 rsvd8;
	__le32 mpdu_frames_processed_count;
	__le32 msdu_frames_processed_count;
	__le32 total_processed_byte_count;
	__le16 rsvd9;
	__le16 hole_count;
	__le16 aging_drop_mpdu_count;
	__le16 rsvd10;
	__le32 sawf_dropped_msdu_frames_processed_cnt;
	__le32 rx_bitmap_319_288;
	__le32 rx_bitmap_351_320;
	__le32 rx_bitmap_383_352;
	__le32 rx_bitmap_415_384;
	__le32 rx_bitmap_447_416;
	__le32 rx_bitmap_479_448;
	__le32 rx_bitmap_511_480;
	__le32 rx_bitmap_543_512;
	__le32 rx_bitmap_575_544;
	__le32 rx_bitmap_607_576;
	__le32 rx_bitmap_639_608;
	__le32 rx_bitmap_671_640;
	__le32 rx_bitmap_703_672;
	__le32 rx_bitmap_735_704;
	__le32 rx_bitmap_767_736;
	__le32 rx_bitmap_799_768;
	__le32 rx_bitmap_831_800;
	__le32 rx_bitmap_863_832;
	__le32 rx_bitmap_895_864;
	__le32 rx_bitmap_927_896;
	__le32 rx_bitmap_959_928;
	__le32 rx_bitmap_991_960;
	__le32 rx_bitmap_1023_992;
	__le32 rsvd11[9];
	struct hal_rx_reo_queue_ext ext_desc[];
};

#define HAL_REO_UPD_RX_QUEUE_INFO0_QUEUE_ADDR_HI		GENMASK(7, 0)
#define HAL_REO_UPD_RX_QUEUE_INFO0_UPD_RX_QUEUE_NUM		BIT(8)
#define HAL_REO_UPD_RX_QUEUE_INFO0_UPD_VLD			BIT(9)
#define HAL_REO_UPD_RX_QUEUE_INFO0_UPD_ASSOC_LNK_DESC_CNT	BIT(10)
#define HAL_REO_UPD_RX_QUEUE_INFO0_UPD_DIS_DUP_DETECTION	BIT(11)
#define HAL_REO_UPD_RX_QUEUE_INFO0_UPD_SOFT_REORDER_EN		BIT(12)
#define HAL_REO_UPD_RX_QUEUE_INFO0_UPD_AC			BIT(13)
#define HAL_REO_UPD_RX_QUEUE_INFO0_UPD_BAR			BIT(14)
#define HAL_REO_UPD_RX_QUEUE_INFO0_UPD_RETRY			BIT(15)
#define HAL_REO_UPD_RX_QUEUE_INFO0_UPD_CHECK_2K_MODE		BIT(16)
#define HAL_REO_UPD_RX_QUEUE_INFO0_UPD_OOR_MODE			BIT(17)
#define HAL_REO_UPD_RX_QUEUE_INFO0_UPD_BA_WINDOW_SIZE		BIT(18)
#define HAL_REO_UPD_RX_QUEUE_INFO0_UPD_PN_CHECK			BIT(19)
#define HAL_REO_UPD_RX_QUEUE_INFO0_UPD_EVEN_PN			BIT(20)
#define HAL_REO_UPD_RX_QUEUE_INFO0_UPD_UNEVEN_PN		BIT(21)
#define HAL_REO_UPD_RX_QUEUE_INFO0_UPD_PN_HANDLE_ENABLE		BIT(22)
#define HAL_REO_UPD_RX_QUEUE_INFO0_UPD_PN_SIZE			BIT(23)
#define HAL_REO_UPD_RX_QUEUE_INFO0_UPD_IGNORE_AMPDU_FLG		BIT(24)
#define HAL_REO_UPD_RX_QUEUE_INFO0_UPD_SVLD			BIT(25)
#define HAL_REO_UPD_RX_QUEUE_INFO0_UPD_SSN			BIT(26)
#define HAL_REO_UPD_RX_QUEUE_INFO0_UPD_SEQ_2K_ERR		BIT(27)
#define HAL_REO_UPD_RX_QUEUE_INFO0_UPD_PN_ERR			BIT(28)
#define HAL_REO_UPD_RX_QUEUE_INFO0_UPD_PN_VALID			BIT(29)
#define HAL_REO_UPD_RX_QUEUE_INFO0_UPD_PN			BIT(30)
#define HAL_REO_UPD_RX_QUEUE_INFO0_CLR_STATS_COUNTERS		BIT(31)

#define HAL_REO_UPD_RX_QUEUE_INFO1_VLD				BIT(0)
#define HAL_REO_UPD_RX_QUEUE_INFO1_ASSOC_LNK_DESC_COUNTER	GENMASK(2, 1)
#define HAL_REO_UPD_RX_QUEUE_INFO1_DIS_DUP_DETECTION		BIT(3)
#define HAL_REO_UPD_RX_QUEUE_INFO1_SOFT_REORDER_EN		BIT(4)
#define HAL_REO_UPD_RX_QUEUE_INFO1_LINK_LIST_TIMER_ID		GENMASK(8, 5)
#define HAL_REO_UPD_RX_QUEUE_INFO1_BAR				BIT(9)
#define HAL_REO_UPD_RX_QUEUE_INFO1_RETRY			BIT(10)
#define HAL_REO_UPD_RX_QUEUE_INFO1_CHECK_2K_MODE		BIT(11)
#define HAL_REO_UPD_RX_QUEUE_INFO1_OOR_MODE			BIT(12)
#define HAL_REO_UPD_RX_QUEUE_INFO1_PN_CHECK			BIT(13)
#define HAL_REO_UPD_RX_QUEUE_INFO1_EVEN_PN			BIT(14)
#define HAL_REO_UPD_RX_QUEUE_INFO1_UNEVEN_PN			BIT(15)

#define HAL_REO_UPD_RX_QUEUE_INFO2_PN_HANDLE_ENABLE		BIT(0)
#define HAL_REO_UPD_RX_QUEUE_INFO2_IGNORE_AMPDU_FLG		BIT(1)
#define HAL_REO_UPD_RX_QUEUE_INFO2_BA_WINDOW_SIZE		GENMASK(11, 2)
#define HAL_REO_UPD_RX_QUEUE_INFO2_PN_SIZE			GENMASK(13, 12)
#define HAL_REO_UPD_RX_QUEUE_INFO2_SVLD			BIT(14)
#define HAL_REO_UPD_RX_QUEUE_INFO2_SSN				GENMASK(26, 15)
#define HAL_REO_UPD_RX_QUEUE_INFO2_SEQ_2K_ERR			BIT(27)
#define HAL_REO_UPD_RX_QUEUE_INFO2_PN_ERR			BIT(28)
#define HAL_REO_UPD_RX_QUEUE_INFO2_PN_VALID			BIT(29)
#define HAL_REO_UPD_RX_QUEUE_INFO2_FLUSH_FROM_CACHE		BIT(30)

#define HAL_REO_UPD_RX_QUEUE_INFO3_PN_127_48_INFO		GENMASK(1, 0)

struct hal_reo_update_rx_queue {
	struct hal_reo_cmd_hdr cmd;
	__le32 queue_addr_lo;
	__le32 info0;
	__le16 receive_queue_number;
	__le16 info1;
	__le32 info2;
	__le32 pn_31_0;
	__le16 pn_47_32;
	__le16 info3;
	__le32 rsvd0[3];
} __packed;

struct hal_rx_reo_queue_1k {
	struct hal_desc_header desc_hdr;
	__le32 reserved[31];
} __packed;

#define HAL_REO_UNBLOCK_CACHE_INFO0_UNBLK_CACHE		BIT(0)

struct hal_reo_unblock_cache {
	struct hal_reo_cmd_hdr cmd;
	__le16 info0;
	__le16 rsvd0;
	__le32 rsvd1[8];
} __packed;

enum hal_reo_exec_status {
	HAL_REO_EXEC_STATUS_SUCCESS,
	HAL_REO_EXEC_STATUS_BLOCKED,
	HAL_REO_EXEC_STATUS_FAILED,
	HAL_REO_EXEC_STATUS_RESOURCE_BLOCKED,
};

#define HAL_REO_STATUS_HDR_INFO0_EXEC_STATUS	GENMASK(11, 10)

struct hal_reo_status_hdr {
	__le16 reo_status_number;
	__le16 info0;
	__le32 timestamp;
} __packed;

/* hal_reo_status_hdr
 *		Producer: REO
 *		Consumer: SW
 *
 * status_num
 *		The value in this field is equal to value of the reo command
 *		number. This field helps to correlate the statuses with the REO
 *		commands.
 *
 * execution_time (in us)
 *		The amount of time REO took to execute the command. Note that
 *		this time does not include the duration of the command waiting
 *		in the command ring, before the execution started.
 *
 * execution_status
 *		Execution status of the command. Values are defined in
 *		enum %HAL_REO_EXEC_STATUS_.
 */

#define HAL_REO_GET_Q_STATS_STATUS_INFO0_SSN			GENMASK(11, 0)
#define HAL_REO_GET_Q_STATS_STATUS_INFO0_CURRENT_INDEX		GENMASK(21, 12)

#define HAL_REO_GET_Q_STATS_STATUS_INFO1_PN_127_48_INFO		GENMASK(1, 0)

#define HAL_REO_GET_Q_STATS_STATUS_INFO2_CURRENT_MPDU_COUNT	GENMASK(6, 0)
#define HAL_REO_GET_Q_STATS_STATUS_INFO2_CURRENT_MSDU_COUNT	GENMASK(31, 7)
#define HAL_REO_GET_Q_STATS_STATUS_INFO3_WINDOW_JUMP_2K		GENMASK(3, 0)
#define HAL_REO_GET_Q_STATS_STATUS_INFO3_TIMEOUT_COUNT		GENMASK(9, 4)
#define HAL_REO_GET_Q_STATS_STATUS_INFO3_FWD_DUE_TO_BAR_COUNT	GENMASK(15, 10)
#define HAL_REO_GET_Q_STATS_STATUS_INFO4_FRAMES_IN_ORDER_COUNT	GENMASK(23, 0)
#define HAL_REO_GET_Q_STATS_STATUS_INFO4_BAR_RECEIVED_COUNT	GENMASK(31, 24)
#define HAL_REO_GET_Q_STATS_STATUS_INFO5_LATE_RCV_MPDU_COUNT	GENMASK(11, 0)
#define HAL_REO_GET_Q_STATS_STATUS_INFO5_HOLE_COUNT		GENMASK(27, 12)
#define HAL_REO_GET_Q_STATS_STATUS_INFO5_GET_Q_1K_SSTAT_FOLLOW	BIT(28)
#define HAL_REO_GET_Q_STATS_STATUS_INFO6_LOOPING_COUNT		GENMASK(15, 12)

struct hal_reo_get_queue_stats_status {
	struct hal_reo_status_hdr hdr;
	__le32 info0;
	__le32 pn_31_0;
	__le16 pn_47_32;
	__le16 info1;
	__le32 rsvd0[2];
	__le32 last_rx_enqueue_timestamp;
	__le32 last_rx_dequeue_timestamp;
	__le32 rx_bitmap_31_0;
	__le32 rx_bitmap_63_32;
	__le32 rx_bitmap_95_64;
	__le32 rx_bitmap_127_96;
	__le32 rx_bitmap_159_128;
	__le32 rx_bitmap_191_160;
	__le32 rx_bitmap_223_192;
	__le32 rx_bitmap_255_224;
	__le32 rx_bitmap_287_256;
	__le32 info2;
	__le16 info3;
	__le16 duplicate_count;
	__le32 info4;
	__le32 mpdu_frames_processed_count;
	__le32 msdu_frames_processed_count;
	__le32 total_processed_byte_count;
	__le32 info5;
	__le16 aging_drop_mpdu_count;
	__le16 info6;
} __packed;

#define HAL_REO_GET_Q_1K_STATS_STATUS_INFO0_LOOPING_COUNT	GENMASK(15, 12)

/* REO status TLV carrying the extension bitmap bits 288..1023 for the
 * preceding HAL_REO_GET_QUEUE_STATS_STATUS response (when requested).
 */
struct hal_reo_get_queue_1k_stats_status {
	struct hal_reo_status_hdr hdr;
	__le32 rx_bitmap1023_288[23];
	__le16 rsvd0;
	__le16 info0;
} __packed;

#define HAL_REO_FLUSH_QUEUE_INFO0_ERR_DETECTED	BIT(0)

struct hal_reo_flush_queue_status {
	struct hal_reo_status_hdr hdr;
	__le16 info0;
	__le16 rsvd0;
	__le32 rsvd1[23];
} __packed;

/* hal_reo_flush_queue_status
 *		Producer: REO
 *		Consumer: SW
 *
 * status_hdr
 *		Details that can link this status with the original command. It
 *		also contains info on how long REO took to execute this command.
 *
 * error_detected
 *		Status of blocking resource
 *
 *		0 - No error has been detected while executing this command
 *		1 - Error detected. The resource to be used for blocking was
 *		    already in use.
 *
 * looping_count
 *		A count value that indicates the number of times the producer of
 *		entries into this Ring has looped around the ring.
 */

#define HAL_REO_FLUSH_CACHE_STATUS_INFO0_IS_ERR			BIT(0)
#define HAL_REO_FLUSH_CACHE_STATUS_INFO0_BLOCK_ERR_CODE		GENMASK(2, 1)
#define HAL_REO_FLUSH_CACHE_STATUS_INFO0_FLUSH_STATUS_HIT	BIT(8)
#define HAL_REO_FLUSH_CACHE_STATUS_INFO0_FLUSH_DESC_TYPE	GENMASK(11, 9)
#define HAL_REO_FLUSH_CACHE_STATUS_INFO0_FLUSH_CLIENT_ID	GENMASK(15, 12)
#define HAL_REO_FLUSH_CACHE_STATUS_INFO0_FLUSH_ERR		GENMASK(17, 16)
#define HAL_REO_FLUSH_CACHE_STATUS_INFO0_FLUSH_COUNT		GENMASK(25, 18)

struct hal_reo_flush_cache_status {
	struct hal_reo_status_hdr hdr;
	__le32 info0;
	__le32 rsvd0[23];
} __packed;

/* hal_reo_flush_cache_status
 *		Producer: REO
 *		Consumer: SW
 *
 * status_hdr
 *		Details that can link this status with the original command. It
 *		also contains info on how long REO took to execute this command.
 *
 * error_detected
 *		Status for blocking resource handling
 *
 *		0 - No error has been detected while executing this command
 *		1 - An error in the blocking resource management was detected
 *
 * block_error_details
 *		only valid when error_detected is set
 *
 *		0 - No blocking related errors found
 *		1 - Blocking resource is already in use
 *		2 - Resource requested to be unblocked, was not blocked
 *
 * cache_controller_flush_status_hit
 *		The status that the cache controller returned on executing the
 *		flush command.
 *
 *		0 - miss; 1 - hit
 *
 * cache_controller_flush_status_desc_type
 *		Flush descriptor type
 *
 * cache_controller_flush_status_client_id
 *		Module who made the flush request
 *
 *		In REO, this is always 0
 *
 * cache_controller_flush_status_error
 *		Error condition
 *
 *		0 - No error found
 *		1 - HW interface is still busy
 *		2 - Line currently locked. Used for one line flush command
 *		3 - At least one line is still locked.
 *		    Used for cache flush command.
 *
 * cache_controller_flush_count
 *		The number of lines that were actually flushed out
 *
 * looping_count
 *		A count value that indicates the number of times the producer of
 *		entries into this Ring has looped around the ring.
 */

#define HAL_REO_UNBLOCK_CACHE_STATUS_INFO0_IS_ERR	BIT(0)
#define HAL_REO_UNBLOCK_CACHE_STATUS_INFO0_TYPE		BIT(1)

struct hal_reo_unblock_cache_status {
	struct hal_reo_status_hdr hdr;
	__le16 info0;
	__le16 rsvd0;
	__le32 rsvd1[23];
} __packed;

/* hal_reo_unblock_cache_status
 *		Producer: REO
 *		Consumer: SW
 *
 * status_hdr
 *		Details that can link this status with the original command. It
 *		also contains info on how long REO took to execute this command.
 *
 * error_detected
 *		0 - No error has been detected while executing this command
 *		1 - The blocking resource was not in use, and therefore it could
 *		    not be unblocked.
 *
 * unblock_type
 *		Reference to the type of unblock command
 *		0 - Unblock a blocking resource
 *		1 - The entire cache usage is unblock
 *
 * looping_count
 *		A count value that indicates the number of times the producer of
 *		entries into this Ring has looped around the ring.
 */

#define HAL_REO_FLUSH_TIMEOUT_STATUS_INFO0_IS_ERR		BIT(0)
#define HAL_REO_FLUSH_TIMEOUT_STATUS_INFO0_LIST_EMPTY		BIT(1)

struct hal_reo_flush_timeout_list_status {
	struct hal_reo_status_hdr hdr;
	__le16 info0;
	__le16 rsvd0;
	__le16 release_desc_count;
	__le16 forward_buf_count;
	__le32 rsvd1[22];
} __packed;

/* hal_reo_flush_timeout_list_status
 *		Producer: REO
 *		Consumer: SW
 *
 * status_hdr
 *		Details that can link this status with the original command. It
 *		also contains info on how long REO took to execute this command.
 *
 * error_detected
 *		0 - No error has been detected while executing this command
 *		1 - Command not properly executed and returned with error
 *
 * timeout_list_empty
 *		When set, REO has depleted the timeout list and all entries are
 *		gone.
 *
 * release_desc_count
 *		Producer: SW; Consumer: REO
 *		The number of link descriptor released
 *
 * forward_buf_count
 *		Producer: SW; Consumer: REO
 *		The number of buffers forwarded to the REO destination rings
 *
 * looping_count
 *		A count value that indicates the number of times the producer of
 *		entries into this Ring has looped around the ring.
 */

#define HAL_REO_DESC_THRESH_STATUS_INFO0_THRESH_INDEX		GENMASK(1, 0)
#define HAL_REO_DESC_THRESH_STATUS_INFO1_LINK_DESC_COUNTER0	GENMASK(23, 0)
#define HAL_REO_DESC_THRESH_STATUS_INFO2_LINK_DESC_COUNTER1	GENMASK(23, 0)
#define HAL_REO_DESC_THRESH_STATUS_INFO3_LINK_DESC_COUNTER2	GENMASK(23, 0)
#define HAL_REO_DESC_THRESH_STATUS_INFO4_LINK_DESC_COUNTER_SUM	GENMASK(25, 0)

struct hal_reo_desc_thresh_reached_status {
	struct hal_reo_status_hdr hdr;
	__le16 info0;
	__le16 rsvd0;
	__le32 info1;
	__le32 info2;
	__le32 info3;
	__le32 info4;
	__le32 rsvd1[19];
} __packed;

/* hal_reo_desc_thresh_reached_status
 *		Producer: REO
 *		Consumer: SW
 *
 * status_hdr
 *		Details that can link this status with the original command. It
 *		also contains info on how long REO took to execute this command.
 *
 * threshold_index
 *		The index of the threshold register whose value got reached
 *
 * link_descriptor_counter0
 * link_descriptor_counter1
 * link_descriptor_counter2
 * link_descriptor_counter_sum
 *		Value of the respective counters at generation of this message
 *
 * looping_count
 *		A count value that indicates the number of times the producer of
 *		entries into this Ring has looped around the ring.
 */

struct hal_mon_buf_ring {
	__le32 paddr_lo;
	__le32 paddr_hi;
	__le64 cookie;
} __packed;

/* hal_mon_buf_ring
 *	Producer : SW
 *	Consumer : Monitor
 *
 * paddr_lo
 *	Lower 32-bit physical address of the buffer pointer from the source ring.
 * paddr_hi
 *	bit range 7-0 : upper 8 bit of the physical address.
 *	bit range 31-8 : reserved.
 * cookie
 *	Consumer: RxMon/TxMon 64 bit cookie of the buffers.
 */

#define HAL_MON_DEST_INFO0_END_OFFSET			GENMASK(11, 0)
#define HAL_MON_DEST_INFO0_END_REASON			GENMASK(17, 16)
#define HAL_MON_DEST_INFO0_EMPTY_DESC			BIT(19)
#define HAL_MON_DEST_INFO0_PKT_BUFFER_CNT		GENMASK(27, 20)

struct hal_mon_dest_desc {
	__le64 stat_buf_va;
	__le32 ppdu_id;
	__le32 info0;
} __packed;

/* hal_mon_dest_ring
 *	Producer : TxMon/RxMon
 *	Consumer : SW
 * cookie
 *	bit 0 -17 buf_id to track the skb's vaddr.
 * ppdu_id
 *	Phy ppdu_id
 * end_offset
 *	The offset into status buffer where DMA ended, ie., offset to the last
 *	TLV + last TLV size.
 * flush_detected
 *	Indicates whether 'tx_flush' or 'rx_flush' occurred.
 * end_of_ppdu
 *	Indicates end of ppdu.
 * pmac_id
 *	Indicates PMAC that received from frame.
 * empty_descriptor
 *	This descriptor is written on flush or end of ppdu or end of status
 *	buffer.
 * ring_id
 *	updated by SRNG.
 * looping_count
 *	updated by SRNG.
 */

#define HAL_TX_MSDU_METADATA_INFO0_ENCRYPT_FLAG		BIT(8)
#define HAL_TX_MSDU_METADATA_INFO0_VALID_KEY_FLAGS	BIT(9)
#define HAL_TX_MSDU_METADATA_INFO0_ENCRYPT_TYPE		GENMASK(16, 15)
#define HAL_TX_MSDU_METADATA_INFO0_HOST_TX_DESC_POOL	BIT(31)
#define HAL_TX_MSDU_METADATA_INFO2_KEY_FLAGS		GENMASK(15, 8)

struct hal_tx_msdu_metadata {
	__le32 info0;
	__le32 info1;
	__le32 info2;
	__le32 rsvd0[4];
} __packed;

/* hal_tx_msdu_metadata
 * valid_encrypt_type
 *		if set, encrypt type is valid
 * encrypt_type
 *		0 = NO_ENCRYPT,
 *		1 = ENCRYPT,
 *		2 ~ 3 - Reserved
 * host_tx_desc_pool
 *		If set, Firmware allocates tx_descriptors
 *		in WAL_BUFFERID_TX_HOST_DATA_EXP,instead
 *		of WAL_BUFFERID_TX_TCL_DATA_EXP.
 *		Use cases:
 *		Any time firmware uses TQM-BYPASS for Data
 *		TID, firmware expect host to set this bit.
 */

#define HAL_TCL_EXIT_BASE_INFO0_BUF_OR_EXT_DESC_TYPE		BIT(0)
#define HAL_TCL_EXIT_BASE_INFO0_ENCAP_TYPE			GENMASK(2, 1)
#define HAL_TCL_EXIT_BASE_INFO0_TID				GENMASK(14, 11)
#define HAL_TCL_EXIT_BASE_INFO0_MSDU_DROP			BIT(15)
#define HAL_TCL_EXIT_BASE_INFO1_NON_QOS				BIT(7)
#define HAL_TCL_EXIT_BASE_INFO1_ILLEGAL_FRAME			BIT(30)
#define HAL_TCL_EXIT_BASE_INFO1_ILLEGAL_ETH_HEADER		BIT(31)
#define HAL_TCL_EXIT_BASE_INFO2_TCP_PROTO			BIT(1)
#define HAL_TCL_EXIT_BASE_INFO2_UDP_PROTO			BIT(2)
#define HAL_TCL_EXIT_BASE_INFO3_DA_IS_BCAST_MCAST		BIT(8)
#define HAL_TCL_EXIT_BASE_INFO3_FLOW_OVERRIDE_ENABLE		BIT(10)
#define HAL_TCL_EXIT_BASE_INFO3_FLOW_SELECT			BIT(11)
#define HAL_TCL_EXIT_BASE_INFO3_INDEX_LOOKUP_OVERRIDE		BIT(12)
#define HAL_TCL_EXIT_BASE_INFO3_CMD_LINK_ID			GENMASK(15, 13)
#define HAL_TCL_EXIT_BASE_INFO4_ADDRX_IDX_TIMEOUT		BIT(0)
#define HAL_TCL_EXIT_BASE_INFO4_ADDRX_IDX_INVALID		BIT(1)
#define HAL_TCL_EXIT_BASE_INFO5_PEER_POINTER_NULL_EXCEPTION	BIT(30)
#define HAL_TCL_EXIT_BASE_INFO5_BANK_NOT_CONFIGURED		BIT(31)
#define HAL_TCL_EXIT_BASE_INFO6_DATA_LENGTH			GENMASK(24, 9)
#define HAL_TCL_EXIT_BASE_INFO6_IPV4_CHECKSUM_EN		BIT(25)
#define HAL_TCL_EXIT_BASE_INFO6_UDP_OVER_IPV4_CHECKSUM_EN	BIT(26)
#define HAL_TCL_EXIT_BASE_INFO6_L4_CHECKSUM_EN			BIT(27)
#define HAL_TCL_EXIT_BASE_INFO6_HLOS_TID_OVERWRITE		BIT(30)
#define HAL_TCL_EXIT_BASE_INFO6_MSDU_LENGTH_ERROR		BIT(31)
#define HAL_TCL_EXIT_BASE_INFO7_CACHE_SET_NUM			GENMASK(3, 0)
#define HAL_TCL_EXIT_BASE_INFO7_TO_FW_SW			GENMASK(5, 4)
#define HAL_TCL_EXIT_BASE_INFO7_PARSER_OP_TLV_SEQUENCE_ERR	BIT(6)
#define HAL_TCL_EXIT_BASE_INFO7_WHO_CLASSIFY_INFOSEL		GENMASK(9, 8)
#define HAL_TCL_EXIT_BASE_INFO7_VDEV_ID				GENMASK(17, 10)
#define HAL_TCL_EXIT_BASE_INFO7_VDEV_ID_CHECK_EN		BIT(19)
#define HAL_TCL_EXIT_BASE_INFO7_VDEV_ID_CHECK_FAILURE		BIT(20)
#define HAL_TCL_EXIT_BASE_INFO7_FLOW_POINTER_NULL		BIT(25)
#define HAL_TCL_EXIT_BASE_INFO7_WHO_CLASSIFY_INFO_SEL_EXCEEDED	BIT(28)
#define HAL_TCL_EXIT_BASE_INFO7_BANK_ID_EXCEEDED		BIT(29)
#define HAL_TCL_EXIT_BASE_INFO7_BUFFER_LENGTH_ERROR		BIT(30)
#define HAL_TCL_EXIT_BASE_INFO8_BANK_ID				GENMASK(25, 18)
#define HAL_TCL_EXIT_BASE_INFO8_FW_LINK_ID			GENMASK(31, 29)

struct hal_tcl_exit_base {
	struct ath12k_buffer_addr buf_addr_info;
	__le16 tcl_status_number;
	__le16 info0;
	__le32 info1;
	__le16 l3_type;
	__le16 rsvd0;
	__le16 info2;
	__le16 l4_offset;
	__le32 ipv6_options_crc;
	__le16 l4_sport;
	__le16 l4_dport;
	__le16 sw_peer_id;
	__le16 rsvd1;
	__le32 rsvd2;
	__le16 window_size;
	__le16 rsvd3;
	__le32 toeplitz_hash_2;
	__le32 toeplitz_hash_4;
	__le32 flowid_toeplitz_hash_5;
	__le16 info3;
	__le16 addrx_ast_hash_idx;
	u8 addr_x[ETH_ALEN];
	u8 addr_y[ETH_ALEN];
	__le16 addry_ast_hash_idx;
	__le16 info4;
	__le32 flow_pntr_39_8;
	__le32 info5;
	__le32 rsvd4;
	__le16 meta_data_ase;
	__le16 meta_data_cce;
	__le32 cce_rule_ind_31_0;
	__le32 cce_rule_ind_63_32;
	__le32 info6;
	__le32 rsvd5;
	__le32 info7;
	__le32 info8;
	__le32 rsvd6;
	__le32 host_meta_info_0;
	__le32 host_meta_info_1;
	__le32 host_meta_info_2;
	__le32 host_meta_info_3;
	__le32 host_meta_info_4;
	__le32 host_meta_info_5;
	__le32 host_meta_info_6;
	__le32 host_meta_info_7;
	__le32 host_meta_info_8;
	__le32 host_meta_info_9;
	__le32 host_meta_info_10;
	__le32 host_meta_info_11;
	__le32 host_meta_info_12;
	__le32 host_meta_info_13;
	__le32 host_meta_info_14;
	__le32 host_meta_info_15;
};

struct hal_tcl_regular_exit_ring {
	struct hal_tcl_exit_base tcl_exit_base_info;
	__le32 header_data[32];
	__le32 rsvd0[2];
} __packed;

#define HAL_WBM_RELEASE_INFO0_REL_SRC_MODULE		GENMASK(2, 0)
#define HAL_WBM_RELEASE_INFO0_DESC_TYPE			GENMASK(8, 6)

struct hal_wbm_release_ring {
	struct ath12k_buffer_addr buf_addr_info;
	__le16 info0;
	__le16 rsvd0;
	__le32 rsvd1[5];
} __packed;

enum hal_tlv_tag_be {
	HAL_TQM_REMOVE_MSDU_BO = 183,
	HAL_TQM_REMOVE_MSDU_STATUS_BO = 184,
	HAL_TQM_REMOVE_MPDU_BO = 181,
	HAL_TQM_REMOVE_MPDU_STATUS_BO = 182,
	HAL_TQM_SYNC_CMD_BO = 332,
	HAL_TQM_SYNC_CMD_STATUS_BO = 334,
	HAL_TQM_GET_MPDUQ_STATS_BO		= 211,
	HAL_TQM_GET_MPDUQ_STATS_STATUS_BO	= 238,
	HAL_TQM_UPDATE_MSDUQ_BO			= 367,
	HAL_TQM_UPDATE_MPDUQ_BO			= 368,
	HAL_TQM_UPDATE_MSDUQ_STATUS_BO		= 369,
	HAL_TQM_UPDATE_MPDUQ_STATUS_BO		= 370,
	HAL_SAM_MPDU_QUEUE_CLEAR_PROGRAMMING_BO = 608,
	HAL_SAM_MSDU_QUEUE_CLEAR_PROGRAMMING_BO = 609,

	/*
	 * WIFITQM_FW_COMPLETION_E = 637
	 * Used for SW2TQM FW-owned buffer release commands.
	 * The buffer_addr_info.return_buffer_manager field must be set
	 * to the FW RBM value so TQM/WBM returns the buffer to the
	 * correct FW buffer pool.
	 */
	HAL_TQM_FW_COMPLETION_BO = 637,
	HAL_SAM_PEER_CLEAR_PROGRAMMING_BO = 647,
};

enum hal_tqm_remove_msdu_cmd_type {
	HAL_WIFIREMOVE_HEAD_MSDUS,
	HAL_WIFIREMOVE_AGED_MSDUS,
	HAL_WIFIREMOVE_MSDUS_AND_DISABLE_FLOW,
};

enum hal_tqm_remove_mpdu_cmd_type {
	HAL_WIFIREMOVE_MPDUS,
	HAL_WIFIREMOVE_TRANSMITTED_MPDUS,
	HAL_WIFIREMOVE_UNTRANSMITTED_MPDUS,
	HAL_WIFIREMOVE_AGED_MPDUS,
	HAL_WIFIREMOVE_MPDUS_AND_DISABLE_QUEUE,
};

enum hal_tqm_host_status_ring {
	HAL_TQM_HOST_STATUS_RING_0,
	HAL_TQM_HOST_STATUS_RING_1,
};

enum hal_tqm_cmd_execution_status {
	HAL_TQM_SUCCESSFUL_EXECUTION,
	HAL_TQM_FAILED_WITH_INVALID_DESCRIPTOR,
	HAL_TQM_RESOURCE_BLOCKED,
	HAL_TQM_SCH_FLUSH,
};

enum hal_tqm_service_category {
	HAL_TQM_SERVICE_CATEGORY_SC0,
	HAL_TQM_SERVICE_CATEGORY_SC1,
	HAL_TQM_SERVICE_CATEGORY_SC2,
	HAL_TQM_SERVICE_CATEGORY_SC3,

	/* keep last */
	HAL_TQM_SERVICE_CATEGORY_MAX,
};

#define HAL_TQM_INFO0_REQUIRED_FOR_CHIP0		BIT(0)
#define HAL_TQM_INFO0_REQUIRED_FOR_CHIP1		BIT(1)
#define HAL_TQM_INFO0_REQUIRED_FOR_CHIP2		BIT(2)
#define HAL_TQM_INFO0_REQUIRED_FOR_CHIP3		BIT(3)
#define HAL_TQM_INFO0_REQUIRED_FOR_CHIP4		BIT(4)
#define HAL_TQM_INFO0_SESSION_ID			GENMASK(11, 5)
#define HAL_TQM_INFO0_CMD_STATUS_RING			GENMASK(13, 12)
#define HAL_TQM_INFO0_PASS_ONTO_TQM			BIT(14)
#define HAL_TQM_INFO0_SCH_SIFS_BURST_CMD_DROP		BIT(15)
#define HAL_TQM_INFO0_SCH_BACKOFF_CMD_DROP		BIT(16)
#define HAL_TQM_INFO0_SCH_TRIG_CMD_DROP			BIT(17)
#define HAL_TQM_INFO0_STATUS_REQUIRED_FOR_HOST		BIT(18)
#define HAL_TQM_INFO0_HOST_STATUS_RING			BIT(19)
#define HAL_TQM_INFO0_SW_PEER_ID_FOR_COMPARISON		GENMASK(31, 20)

struct hal_uniform_tqm_cmd_hdr {
	__le32 tqm_cmd_number;
	__le32 info0;
} __packed;

#define HAL_TQM_MSDU_INFO0_FLOW_DESC_ADDR_39_32		GENMASK(7, 0)
#define HAL_TQM_MSDU_INFO0_SMD_ROAMING_REMOVAL_CMD	BIT(8)
#define HAL_TQM_MSDU_INFO0_SMD_ROAMING_REMOVE_MSDU_COUNT	GENMASK(24, 9)
#define HAL_TQM_MSDU_INFO0_SMD_ROAMING_FORWARDING_RING	BIT(25)

#define HAL_TQM_MSDU_INFO1_REMOVE_MSDU_CMD_TYPE	GENMASK(3, 0)
#define HAL_TQM_MSDU_INFO1_REMOVE_COUNT		GENMASK(19, 4)
#define HAL_TQM_MSDU_INFO1_BLOCK_TX_NOTIFY_FRAME_REMOVAL	BIT(20)
#define HAL_TQM_MSDU_INFO1_RELEASE_REASON_OVERWRITE	GENMASK(22, 21)
#define HAL_TQM_MSDU_INFO1_ALLOW_EARLY_TERMINATION	BIT(23)
#define HAL_TQM_MSDU_INFO1_MIN_ALLOWED_EXECUTION_TIME	GENMASK(29, 24)

#define HAL_TQM_MSDU_INFO2_AGED_REFERENCE_TIMESTAMP	GENMASK(18, 0)
#define HAL_TQM_MSDU_INFO2_RBM_OVERRIDE_VALID		BIT(19)
#define HAL_TQM_MSDU_INFO2_RBM_OVERRIDE		GENMASK(23, 20)

#define HAL_TQM_MSDU_INFO3_SMD_ROAMING_REO2PPE_INT_PRI	GENMASK(3, 0)
#define HAL_TQM_MSDU_INFO3_SMD_ROAMING_REO2PPE_POOL_ID	GENMASK(9, 4)
#define HAL_TQM_MSDU_INFO3_SMD_ROAMING_REO2PPE_PRI_VALID	BIT(10)
#define HAL_TQM_MSDU_INFO3_SMD_ROAMING_REO2PPE_SERVICE_CODE	GENMASK(19, 11)

struct hal_tqm_remove_msdu {
	struct hal_uniform_tqm_cmd_hdr cmd_hdr;
	__le32 msdu_flow_desc_addr_31_0;
	__le32 info0;
	__le32 info1;
	__le32 info2;
	__le32 info3;
	__le16 smd_roaming_reo2ppe_src_info;
	__le16 smd_roaming_reo2ppe_dst_info;
} __packed;

#define HAL_TQM_MPDU_INFO0_QUEUE_DESC_ADDR_39_32	GENMASK(7, 0)
#define HAL_TQM_MPDU_INFO0_BLOCK_TX_NOTIFY_FRAME_REMOVAL	BIT(8)
#define HAL_TQM_MPDU_INFO0_AGED_REFERENCE_TIMESTAMP	GENMASK(27, 9)

#define HAL_TQM_MPDU_INFO1_REMOVE_MPDU_CMD_TYPE	GENMASK(2, 0)
#define HAL_TQM_MPDU_INFO1_REMOVE_COUNT		GENMASK(19, 4)
#define HAL_TQM_MPDU_INFO1_MAX_TRANSMIT_COUNT		GENMASK(26, 20)
#define HAL_TQM_MPDU_INFO1_RELEASE_REASON_OVERWRITE	GENMASK(28, 27)

#define HAL_TQM_MPDU_INFO2_REMOVE_MPDU_SIZE		GENMASK(13, 0)
#define HAL_TQM_MPDU_INFO2_RBM_OVERRIDE_VALID		BIT(14)
#define HAL_TQM_MPDU_INFO2_RBM_OVERRIDE		GENMASK(18, 15)
#define HAL_TQM_MPDU_INFO2_SMD_ROAMING_REMOVAL_CMD	BIT(19)
#define HAL_TQM_MPDU_INFO2_DONOT_FWD_TXD_MDPUS_DURING_SMD	BIT(20)
#define HAL_TQM_MPDU_INFO2_SMD_ROAMING_FORWARDING_RING	BIT(21)
#define HAL_TQM_MPDU_INFO2_SMD_ROAMING_REO2PPE_SERVICE_CODE	GENMASK(30, 22)
#define HAL_TQM_MPDU_INFO2_SMD_ROAMING_REO2PPE_PRI_VALID	BIT(31)

#define HAL_TQM_MPDU_INFO3_SMD_ROAMING_REO2PPE_POOL_ID	GENMASK(5, 0)
#define HAL_TQM_MPDU_INFO3_SMD_ROAMING_REO2PPE_INT_PRI	GENMASK(9, 6)

struct hal_tqm_remove_mpdu {
	struct hal_uniform_tqm_cmd_hdr cmd_hdr;
	__le32 mpdu_queue_desc_addr_31_0;
	__le32 info0;
	__le32 info1;
	__le32 info2;
	__le16 smd_roaming_remove_mpdu_count;
	__le16 smd_roaming_reo2ppe_dst_info;
	__le16 smd_roaming_reo2ppe_src_info;
	__le16 info3;
} __packed;

#define HAL_TQM_SYNC_INFO0_SW_METADATA_667_640		GENMASK(27, 0)
#define HAL_TQM_SYNC_INFO0_TLV64_PADDING		GENMASK(31, 0)

struct hal_tqm_sync_cmd {
	struct hal_uniform_tqm_cmd_hdr cmd_hdr;
	__le32 sync_cmd_sw_metadata_31_0;
	__le32 sync_cmd_sw_metadata_63_32;
	__le32 sync_cmd_sw_metadata_95_64;
	__le32 sync_cmd_sw_metadata_127_96;
	__le32 sync_cmd_sw_metadata_159_128;
	__le32 sync_cmd_sw_metadata_191_160;
	__le32 sync_cmd_sw_metadata_223_192;
	__le32 sync_cmd_sw_metadata_255_224;
	__le32 sync_cmd_sw_metadata_287_256;
	__le32 sync_cmd_sw_metadata_319_288;
	__le32 sync_cmd_sw_metadata_351_320;
	__le32 sync_cmd_sw_metadata_383_352;
	__le32 sync_cmd_sw_metadata_415_384;
	__le32 sync_cmd_sw_metadata_447_416;
	__le32 sync_cmd_sw_metadata_479_448;
	__le32 sync_cmd_sw_metadata_511_480;
	__le32 sync_cmd_sw_metadata_543_512;
	__le32 sync_cmd_sw_metadata_575_544;
	__le32 sync_cmd_sw_metadata_607_576;
	__le32 sync_cmd_sw_metadata_639_608;
	__le32 info0;
	__le32 rsvd0;
} __packed;

#define HAL_TQM_STATUS_INFO0_CMD_EXECUTION_TIME		GENMASK(6, 0)
#define HAL_TQM_STATUS_INFO0_CMD_EXECUTION_TIME_UNIT	BIT(7)
#define HAL_TQM_STATUS_INFO0_CMD_EXECUTION_STATUS	GENMASK(9, 8)
#define HAL_TQM_STATUS_INFO0_CMD_EXECUTION_WAS_PAUSED	BIT(10)
#define HAL_TQM_STATUS_INFO0_CMD_EXECUTION_CODE		GENMASK(18, 11)
#define HAL_TQM_STATUS_INFO0_SESSION_ID			GENMASK(26, 19)
#define HAL_TQM_STATUS_INFO0_RING			GENMASK(28, 27)

struct hal_uniform_tqm_status_hdr {
	__le32 tqm_status_number;
	__le32 timestamp;
	__le32 info0;
} __packed;

#define HAL_TQM_MSDU_RMSTAT_INFO0_REMOVE_MSDU_CMD_TYPE		GENMASK(3, 0)
#define HAL_TQM_MSDU_RMSTAT_INFO0_REMOVED_MSDU_COUNT		GENMASK(19, 4)
#define HAL_TQM_MSDU_RMSTAT_INFO0_HEAD_TX_NOTIFY_FRAME_TYPE	GENMASK(22, 20)
#define HAL_TQM_MSDU_RMSTAT_INFO1_TX_FLOW_NUMBER		GENMASK(23, 0)
#define HAL_TQM_MSDU_RMSTAT_INFO1_FLOW_STATUS			GENMASK(30, 29)
#define HAL_TQM_MSDU_RMSTAT_INFO1_FLOW_EMPTY_STATUS		BIT(31)

struct hal_tqm_remove_msdu_status {
	struct hal_uniform_tqm_status_hdr status_hdr;
	__le32 info0;
	__le32 removed_msdu_byte_count;
	struct ath12k_buffer_addr tx_notify_frame;
	__le32 remaining_flow_byte_count;
	__le16 remaining_msdu_count;
	__le16 rsvd0;
	__le32 info1;
	__le32 msdu_active_count;
	__le32 rsvd1[13];
} __packed;

#define HAL_TQM_MPDU_RMSTAT_INFO0_REMOVE_MPDU_CMD_TYPE		GENMASK(2, 0)
#define HAL_TQM_MPDU_RMSTAT_INFO0_REMOVED_MPDU_COUNT		GENMASK(19, 4)
#define HAL_TQM_MPDU_RMSTAT_INFO0_HEAD_NOTIFY_FRAME_TYPE	GENMASK(31, 29)
#define HAL_TQM_MPDU_RMSTAT_INFO1_REMOVE_MPDU_SIZE		GENMASK(15, 2)
#define HAL_TQM_MPDU_RMSTAT_INFO2_PREV_MPDU_START_SEQ_NUM	GENMASK(11, 0)
#define HAL_TQM_MPDU_RMSTAT_INFO3_HEAD_NOTIFY_FRAME_METADATA	GENMASK(14, 0)
#define HAL_TQM_MPDU_RMSTAT_INFO3_NEW_MPDU_START_SEQ_NUM	GENMASK(31, 20)
#define HAL_TQM_MPDU_RMSTAT_INFO4_HEAD_LENGTH			GENMASK(13, 0)
#define HAL_TQM_MPDU_RMSTAT_INFO4_QUEUE_EMPTY_STATUS		BIT(14)
#define HAL_TQM_MPDU_RMSTAT_INFO4_L4S_DROPPING_CREDIT		GENMASK(30, 15)
#define HAL_TQM_MPDU_RMSTAT_INFO5_TX_MPDU_QUEUE_NUMBER		GENMASK(23, 0)

struct hal_tqm_remove_mpdu_status {
	struct hal_uniform_tqm_status_hdr status_hdr;
	__le32 info0;
	__le16 info1;
	__le16 removed_msdu_count;
	__le32 removed_mpdu_byte_count;
	struct ath12k_buffer_addr tx_notify_frame;
	__le32 remaining_mpdu_queue_byte_count;
	__le16 remaining_mpdu_count;
	__le16 info2;
	__le32 info3;
	__le32 info4;
	__le32 msdu_active_count;
	__le32 info5;
	__le32 rsvd0[10];
} __packed;

#define HAL_TQM_SYNC_STATUS_INFO0_SW_METADATA_667_640		GENMASK(27, 0)
#define HAL_TQM_SYNC_STATUS_INFO0_LOOPING_COUNT			GENMASK(31, 28)

struct hal_tqm_sync_cmd_status {
	struct hal_uniform_tqm_status_hdr status_hdr;
	__le32 sync_cmd_sw_metadata_31_0;
	__le32 sync_cmd_sw_metadata_63_32;
	__le32 sync_cmd_sw_metadata_95_64;
	__le32 sync_cmd_sw_metadata_127_96;
	__le32 sync_cmd_sw_metadata_159_128;
	__le32 sync_cmd_sw_metadata_191_160;
	__le32 sync_cmd_sw_metadata_223_192;
	__le32 sync_cmd_sw_metadata_255_224;
	__le32 sync_cmd_sw_metadata_287_256;
	__le32 sync_cmd_sw_metadata_319_288;
	__le32 sync_cmd_sw_metadata_351_320;
	__le32 sync_cmd_sw_metadata_383_352;
	__le32 sync_cmd_sw_metadata_415_384;
	__le32 sync_cmd_sw_metadata_447_416;
	__le32 sync_cmd_sw_metadata_479_448;
	__le32 sync_cmd_sw_metadata_511_480;
	__le32 sync_cmd_sw_metadata_543_512;
	__le32 sync_cmd_sw_metadata_575_544;
	__le32 sync_cmd_sw_metadata_607_576;
	__le32 sync_cmd_sw_metadata_639_608;
	__le32 info0;
} __packed;

#define HAL_TQM_GET_MPDUQ_STATS_INFO0_MPDUQ_ADDR_HI	GENMASK(7, 0)
#define HAL_TQM_GET_MPDUQ_STATS_INFO0_CLEAR_STATS	BIT(8)

struct hal_tqm_get_mpduq_stats {
	struct hal_uniform_tqm_cmd_hdr cmd_hdr;
	__le32 mpdu_queue_desc_addr_31_0;
	__le16 info0;
	__le16 rsvd0;
} __packed;

#define HAL_TQM_GET_MPDUQ_STATS_STATUS_INFO0_START_SEQ_NUM	GENMASK(11, 0)
#define HAL_TQM_GET_MPDUQ_STATS_STATUS_INFO0_TID		GENMASK(26, 23)
#define HAL_TQM_GET_MPDUQ_STATS_STATUS_INFO1_LAST_SEQ_NUM	GENMASK(11, 0)
#define HAL_TQM_GET_MPDUQ_STATS_STATUS_INFO1_MAX_LSN		GENMASK(23, 12)
#define HAL_TQM_GET_MPDUQ_STATS_STATUS_INFO1_MAX_LSN_VALID	BIT(24)

struct hal_tqm_get_mpduq_stats_cmd_status {
	struct hal_uniform_tqm_status_hdr status_hdr;
	__le16 rsvd0;
	__le16 mpdu_count;
	__le32 rsvd1;
	__le32 info0;
	__le32 pn_31_0;
	__le16 pn_47_32;
	__le16 sw_peer_id;
	__le32 rsvd2[10];
	__le32 info1;
	__le32 rsvd3[5];
} __packed;

/* Chip ID bitmap masks — shared by hal_queue.c and hal_tx.c */
#define ATH12K_CHIP0_BITMAP_MASK                        BIT(0)
#define ATH12K_CHIP1_BITMAP_MASK                        BIT(1)
#define ATH12K_CHIP2_BITMAP_MASK                        BIT(2)
#define ATH12K_CHIP3_BITMAP_MASK                        BIT(3)
#define ATH12K_CHIP4_BITMAP_MASK                        BIT(4)

#define HAL_TQM_UPDATE_MPDUQ_INFO0_MPDUQ_ADDR_HI		GENMASK(7, 0)
#define HAL_TQM_UPDATE_MPDUQ_INFO0_UPDATE_TX_QUEUE_NUMBER	BIT(8)
#define HAL_TQM_UPDATE_MPDUQ_INFO0_SEQ_NUM_UPDATE_VALID		BIT(9)
#define HAL_TQM_UPDATE_MPDUQ_INFO0_UPDATE_QUEUE_VALID		BIT(10)
#define HAL_TQM_UPDATE_MPDUQ_INFO0_UPDATE_SW_PEER_ID		BIT(15)
#define HAL_TQM_UPDATE_MPDUQ_INFO0_UPDATE_TID			BIT(16)
#define HAL_TQM_UPDATE_MPDUQ_INFO0_UPDATE_MPDU_TYPE		BIT(18)

#define HAL_TQM_UPDATE_MPDUQ_INFO1_START_SEQ_NUM		GENMASK(11, 0)
#define HAL_TQM_UPDATE_MPDUQ_INFO1_LAST_SEQ_NUM			GENMASK(27, 16)

#define HAL_TQM_UPDATE_MPDUQ_INFO2_TID_VALUE			GENMASK(12, 9)

#define HAL_TQM_UPDATE_MPDUQ_INFO3_MAX_LSN			GENMASK(13, 2)
#define HAL_TQM_UPDATE_MPDUQ_INFO3_MAX_LSN_VALID		BIT(14)

#define HAL_TQM_UPDATE_MPDUQ_INFO4_QUEUE_NUMBER_VALUE		GENMASK(23, 0)
#define HAL_TQM_UPDATE_MPDUQ_INFO4_UPDATE_MAX_LSN		BIT(24)
#define HAL_TQM_UPDATE_MPDUQ_INFO4_UPDATE_MAX_LSN_VALID		BIT(25)

struct hal_tqm_update_mpduq {
	struct hal_uniform_tqm_cmd_hdr cmd_hdr;
	__le32 mpdu_queue_desc_addr_31_0;
	__le32 info0;
	__le32 info1;
	__le32 rsvd0;
	__le16 rsvd1;
	__le16 info2;
	__le16 rsvd2;
	__le16 sw_peer_id;
	__le32 rsvd3[8];
	__le16 rsvd4;
	__le16 info3;
	__le32 rsvd5;
	__le32 info4;
	__le32 rsvd6[3];
} __packed;

#define HAL_TQM_UPDATE_MPDUQ_CMD_STATUS_INFO0_UPDATE_REQ_MET	BIT(0)
#define HAL_TQM_UPDATE_MPDUQ_CMD_STATUS_INFO0_TX_MPDU_QUEUE_NUM	GENMASK(31, 8)
/* info1 layout matches HAL_TQM_FLOW_UPDATE_STATUS_* (msduq) at the same bit positions */
#define HAL_TQM_UPDATE_MPDUQ_CMD_STATUS_INFO1_SW_PEER_ID		GENMASK(15, 0)
#define HAL_TQM_UPDATE_MPDUQ_CMD_STATUS_INFO1_TID			GENMASK(19, 16)
#define HAL_TQM_UPDATE_MPDUQ_CMD_STATUS_INFO1_ALLOC_SEQ_NUM		GENMASK(31, 20)
/* info2 carries lower 32 bits of allocated PN; info3[15:0] carries upper 16 bits */

struct hal_tqm_update_mpduq_cmd_status {
	struct hal_uniform_tqm_status_hdr status_hdr;
	__le32 info0;
	__le32 info1;
	__le32 allocated_pn_31_0;
	__le16 allocated_pn_47_32;
	__le16 rsvd0;
	__le32 rsvd1[17];
} __packed;

struct hal_tqm_update_tx_mpdu_queue_head_status {
	struct hal_uniform_tqm_status_hdr status_hdr;
	__le32 info0;
	__le32 info1;
	__le32 info2;
	__le32 info3;
	__le32 info4;
	__le32 info5;
	__le32 info6;
	__le32 info7;
	__le32 info8;
	__le32 info9;
	__le32 info10;
	__le32 info11;
	__le32 info12;
	__le32 info13;
	__le32 info14;
	__le32 info15;
	__le32 info16;
	__le32 info17;
	__le32 info18;
	__le32 info19;
	__le32 info20;
} __packed;

struct hal_tqm_cmd_params_std {
	u16 peer_id;
};

struct hal_tqm_remove_msdu_params {
	u8 type;
	u8 block_tx_notify_frame_removal;
	u16 count;
	u8 qtype;
	dma_addr_t msdu_q_paddr;
};

struct hal_tqm_remove_mpdu_params {
	u8 type;
	u8 block_tx_notify_frame_removal;
	u16 count;
	dma_addr_t mpdu_q_paddr;
};

struct hal_tqm_sync_cmd_params {
	bool data_only;
	void *cb_func;
	void *cb_ctxt;
	u64 cb_data;
};

/**
 * struct hal_tqm_fw_buf_release_params - Parameters for FW-owned buffer release
 *
 * Used with HAL_TQM_FW_COMPLETION_BO (TLV tag 637). The paddr, rbm, and
 * cookie are encoded into buffer_addr_info fields so TQM/WBM returns the
 * buffer to the correct FW buffer pool (return_buffer_manager = rbm).
 *
 * @paddr:  Physical address of the FW-owned buffer
 * @rbm:    Return Buffer Manager ID (FW pool identifier)
 * @cookie: SW buffer cookie (original TX descriptor ID)
 */
struct hal_tqm_fw_buf_release_params {
	dma_addr_t paddr;
	u8 rbm;
	u32 cookie;
};

struct hal_tqm_get_mpdu_queue_stats {
	dma_addr_t mpdu_q_paddr;
	bool clear_stats;
};

struct hal_tqm_update_tx_msdu_flow_params {
	dma_addr_t msdu_q_paddr;
	u32 tx_flow_number;
	u16 new_peer_id;
	u8 tid;
	u8 bitmap;
	enum hal_tqm_service_category svc;
	u16 hard_drop_threshold;
	bool update_hard_drop_threshold;
	bool update_peer_id;
	bool update_tid;
	bool flow_valid;
	bool update_flow_valid;
	bool update_flow_number;
	bool update_smd_roaming;
};

struct hal_tqm_update_mpdu_queue_params {
	dma_addr_t mpdu_q_paddr;
	u32 new_queue_number;
	u32 max_lsn_valid:1,
	    sn_num_valid:1,
	    max_lsn:12,
	    sn_num:12,
	    reserved:6;
	u16 new_peer_id;
	u8 new_tid;
	u8 __pad;
	bool update_peer_id;
	bool update_tid;
	bool update_queue_valid;
	bool update_queue_number;
	bool update_seq_number;
	bool update_mpdu_type;
};

struct ath12k_hal_tqm_cmd {
	struct hal_tqm_cmd_params_std std;
	union {
		struct hal_tqm_remove_msdu_params remove_msdu_params;
		struct hal_tqm_remove_mpdu_params remove_mpdu_params;
		struct hal_tqm_sync_cmd_params tqm_sync_params;
		struct hal_tqm_fw_buf_release_params fw_buf_release_params;
		struct hal_tqm_get_mpdu_queue_stats get_mpduq_stats;
		struct hal_tqm_update_tx_msdu_flow_params update_tx_msdu_params;
		struct hal_tqm_update_mpdu_queue_params update_mpduq;
	};
};

struct hal_tqm_status_hdr {
	u32 status_num;
	u32 timestamp;
	u8 cmd_execution_status;
	u8 tqm_status_ring;
};

struct hal_tqm_status_remove_msdu {
	u8 remove_msdu_cmd_type;
	u8 flow_status;
	u16 removed_msdu_count;
	u32 tx_flow_number;
	dma_addr_t buffer_addr;
};

struct hal_tqm_status_remove_mpdu {
	u8 remove_mpdu_cmd_type;
	u16 removed_mpdu_count;
	u16 removed_msdu_count;
	u32 tx_mpdu_queue_number;
	dma_addr_t buffer_addr;
};

struct hal_tqm_status_sync_cmd {
	u32 metadata_0;
	u32 metadata_1;
	u32 metadata_2;
};

struct hal_tqm_get_mpduq_stats_status {
	u16 mpdu_cnt;
	u16 start_seq_num;
	u16 last_seq_num;
	u16 sw_peer_id;
	u32 pn_31_0;
	u32 pn_47_32;
	u32 tid:8,
	    max_lsn:12,
	    max_lsn_valid:1,
	    reserved:11;
};

struct hal_tqm_status_update_msdu_flow {
	u32 tx_flow_number;
	u16 sw_peer_id;
	u8 tid;
	bool update_requirements_not_met;
};

struct hal_tqm_status_update_mpdu_queue {
	u16 sw_peer_id;
	u8 tid;
	u32 tx_mpdu_queue_number;
	u16 allocated_sequence_number;
	u64 allocated_pn;
	bool update_requirements_not_met;
};

struct hal_tqm_status {
	struct hal_tqm_status_hdr status_hdr;
	union {
		struct hal_tqm_status_remove_msdu remove_msdu;
		struct hal_tqm_status_remove_mpdu remove_mpdu;
		struct hal_tqm_status_sync_cmd sync_status;
		struct hal_tqm_get_mpduq_stats_status mpduq_stats;
		struct hal_tqm_status_update_msdu_flow update_msdu_flow;
		struct hal_tqm_status_update_mpdu_queue update_mpdu_queue;
	};
};

#define HAL_TQM_FLOW_INFO0_QUEUE_ADDR_39_32			GENMASK(7, 0)
#define HAL_TQM_FLOW_INFO0_UPDATE_TX_FLOW_NUMBER		BIT(8)
#define HAL_TQM_FLOW_INFO0_UPDATE_FLOW_VALID			BIT(9)
#define HAL_TQM_FLOW_INFO0_UPDATE_EMPTY_TO_NOT_EMPTY_NOTIFICATION	BIT(10)
#define HAL_TQM_FLOW_INFO0_UPDATE_NOT_EMPTY_TO_EMPTY_NOTIFICATION	BIT(11)
#define HAL_TQM_FLOW_INFO0_UPDATE_THRESHOLD_BASED_NOTIFICATION	BIT(12)
#define HAL_TQM_FLOW_INFO0_UPDATE_THRESHOLD_BASED_NOTIFICATION_2	BIT(13)
#define HAL_TQM_FLOW_INFO0_UPDATE_SW_NOTIFICATION_THRESHOLD		BIT(14)
#define HAL_TQM_FLOW_INFO0_UPDATE_SW_NOTIFICATION_THRESHOLD_2	BIT(15)
#define HAL_TQM_FLOW_INFO0_UPDATE_SW_PEER_ID			BIT(16)
#define HAL_TQM_FLOW_INFO0_UPDATE_DROP_RULE			BIT(17)
#define HAL_TQM_FLOW_INFO0_UPDATE_GEN_SLOW_DROP_NOTIFICATION	BIT(18)
#define HAL_TQM_FLOW_INFO0_UPDATE_SLOW_DROP_NOTIFICATION_DONE	BIT(19)
#define HAL_TQM_FLOW_INFO0_UPDATE_GEN_MEDIUM_DROP_NOTIFICATION	BIT(20)
#define HAL_TQM_FLOW_INFO0_UPDATE_MEDIUM_DROP_NOTIFICATION_DONE	BIT(21)
#define HAL_TQM_FLOW_INFO0_UPDATE_GEN_HARD_DROP_NOTIFICATION	BIT(22)
#define HAL_TQM_FLOW_INFO0_UPDATE_HARD_DROP_NOTIFICATION_DONE	BIT(23)
#define HAL_TQM_FLOW_INFO0_UPDATE_SLOW_DROP_THRESHOLD		BIT(24)
#define HAL_TQM_FLOW_INFO0_UPDATE_MEDIUM_DROP_THRESHOLD		BIT(25)
#define HAL_TQM_FLOW_INFO0_UPDATE_HARD_DROP_THRESHOLD		BIT(26)
#define HAL_TQM_FLOW_INFO0_UPDATE_TID				BIT(27)
#define HAL_TQM_FLOW_INFO0_UPDATE_LINK_DESC_COUNTER		BIT(28)
#define HAL_TQM_FLOW_INFO0_UPDATE_CLEAR_STAT_COUNTER		BIT(29)
#define HAL_TQM_FLOW_INFO0_UPDATE_FLUSH_FROM_CACHE		BIT(30)
#define HAL_TQM_FLOW_INFO0_UPDATE_INVALIDATED_BY_TQM_CMD	BIT(31)

#define HAL_TQM_FLOW_INFO1_UPDATE_MLO_VALID			BIT(0)
#define HAL_TQM_FLOW_INFO1_UPDATE_GREEN_MSDU_DROP_THRESHOLD	BIT(1)
#define HAL_TQM_FLOW_INFO1_UPDATE_YELLOW_MSDU_DROP_THRESHOLD	BIT(2)
#define HAL_TQM_FLOW_INFO1_UPDATE_RED_MSDU_DROP_THRESHOLD	BIT(3)
#define HAL_TQM_FLOW_INFO1_UPDATE_GEN_GREEN_MSDU_DROP_NOTIFICATION	BIT(4)
#define HAL_TQM_FLOW_INFO1_UPDATE_GEN_YELLOW_MSDU_DROP_NOTIFICATION	BIT(5)
#define HAL_TQM_FLOW_INFO1_UPDATE_GEN_RED_MSDU_DROP_NOTIFICATION	BIT(6)
#define HAL_TQM_FLOW_INFO1_UPDATE_GREEN_MSDU_DROP_NOTIFICATION_DONE	BIT(7)
#define HAL_TQM_FLOW_INFO1_UPDATE_YELLOW_MSDU_DROP_NOTIFICATION_DONE	BIT(8)
#define HAL_TQM_FLOW_INFO1_UPDATE_RED_MSDU_DROP_NOTIFICATION_DONE	BIT(9)
#define HAL_TQM_FLOW_INFO1_UPDATE_STATUS_REQUIRED_FOR_CHIP0	BIT(10)
#define HAL_TQM_FLOW_INFO1_UPDATE_STATUS_REQUIRED_FOR_CHIP1	BIT(11)
#define HAL_TQM_FLOW_INFO1_UPDATE_STATUS_REQUIRED_FOR_CHIP2	BIT(12)
#define HAL_TQM_FLOW_INFO1_UPDATE_STATUS_REQUIRED_FOR_CHIP3	BIT(13)
#define HAL_TQM_FLOW_INFO1_UPDATE_STATUS_REQUIRED_FOR_CHIP4	BIT(14)
#define HAL_TQM_FLOW_INFO1_UPDATE_SMD_ROAMING_IN_PROGRESS	BIT(15)
#define HAL_TQM_FLOW_INFO1_VALID				BIT(16)
#define HAL_TQM_FLOW_INFO1_EMPTY_TO_NOT_EMPTY_NOTIFICATION	BIT(17)
#define HAL_TQM_FLOW_INFO1_NOT_EMPTY_TO_EMPTY_NOTIFICATION	BIT(18)
#define HAL_TQM_FLOW_INFO1_THRESHOLD_BASED_NOTIFICATION	BIT(19)
#define HAL_TQM_FLOW_INFO1_THRESHOLD_BASED_NOTIFICATION_2	BIT(20)
#define HAL_TQM_FLOW_INFO1_DROP_RULE				BIT(21)
#define HAL_TQM_FLOW_INFO1_GEN_SLOW_DROP_NOTIFICATION		BIT(22)
#define HAL_TQM_FLOW_INFO1_SLOW_DROP_NOTIFICATION_DONE		BIT(23)
#define HAL_TQM_FLOW_INFO1_GEN_MEDIUM_DROP_NOTIFICATION	BIT(24)
#define HAL_TQM_FLOW_INFO1_MEDIUM_DROP_NOTIFICATION_DONE	BIT(25)
#define HAL_TQM_FLOW_INFO1_GEN_HARD_DROP_NOTIFICATION		BIT(26)
#define HAL_TQM_FLOW_INFO1_HARD_DROP_NOTIFICATION_DONE		BIT(27)
#define HAL_TQM_FLOW_INFO1_TID					GENMASK(31, 28)

#define HAL_TQM_FLOW_INFO2_LINK_DESC_COUNTER			GENMASK(1, 0)
#define HAL_TQM_FLOW_INFO2_INVALIDATED_BY_TQM_CMD		BIT(2)
#define HAL_TQM_FLOW_INFO2_MLO_VALID				BIT(3)
#define HAL_TQM_FLOW_INFO2_SERVICE_CATEGORY_VALID		BIT(4)
#define HAL_TQM_FLOW_INFO2_QUEUE_SAM_ID			GENMASK(17, 5)
#define HAL_TQM_FLOW_INFO2_PEER_MLO_STATS_ID			GENMASK(28, 18)

#define HAL_TQM_FLOW_INFO3_UPDATE_TX_MSDU_FLOW_NUMBER		GENMASK(23, 0)
#define HAL_TQM_FLOW_INFO3_UPDATE_MSDU_QUEUE_SAM_ID		BIT(24)
#define HAL_TQM_FLOW_INFO3_UPDATE_SERVICE_CATEGORY_VALID	BIT(25)
#define HAL_TQM_FLOW_INFO3_UPDATE_PEER_MLO_STATS_ID		BIT(26)
#define HAL_TQM_FLOW_INFO3_SMD_ROAMING_IN_PROGRESS		BIT(27)

#define HAL_TQM_FLOW_INFO4_GEN_GREEN_MSDU_DROP_NOTIFICATION	BIT(0)
#define HAL_TQM_FLOW_INFO4_GEN_YELLOW_MSDU_DROP_NOTIFICATION	BIT(1)
#define HAL_TQM_FLOW_INFO4_GEN_RED_MSDU_DROP_NOTIFICATION	BIT(2)
#define HAL_TQM_FLOW_INFO4_GREEN_MSDU_DROP_NOTIFICATION_DONE	BIT(3)
#define HAL_TQM_FLOW_INFO4_YELLOW_MSDU_DROP_NOTIFICATION_DONE	BIT(4)
#define HAL_TQM_FLOW_INFO4_RED_MSDU_DROP_NOTIFICATION_DONE	BIT(5)
#define HAL_TQM_FLOW_INFO4_STATUS_REQUIRED_FOR_CHIP0		BIT(6)
#define HAL_TQM_FLOW_INFO4_STATUS_REQUIRED_FOR_CHIP1		BIT(7)
#define HAL_TQM_FLOW_INFO4_STATUS_REQUIRED_FOR_CHIP2		BIT(8)
#define HAL_TQM_FLOW_INFO4_STATUS_REQUIRED_FOR_CHIP3		BIT(9)
#define HAL_TQM_FLOW_INFO4_STATUS_REQUIRED_FOR_CHIP4		BIT(10)

#define HAL_TQM_FLOW_INFO5_UPDATE_SERVICE_CATEGORY		BIT(0)
#define HAL_TQM_FLOW_INFO5_UPDATE_QDEPTH_INDEX_VALID		BIT(1)
#define HAL_TQM_FLOW_INFO5_UPDATE_QDEPTH_INDEX			BIT(2)
#define HAL_TQM_FLOW_INFO5_SERVICE_CATEGORY			GENMASK(4, 3)
#define HAL_TQM_FLOW_INFO5_QDEPTH_INDEX_VALID			BIT(5)
#define HAL_TQM_FLOW_INFO5_QDEPTH_INDEX			GENMASK(17, 6)
#define HAL_TQM_FLOW_INFO5_UPDATE_PER_PEER_FLOW_NUMBER		BIT(18)
#define HAL_TQM_FLOW_INFO5_PER_PEER_FLOW_NUMBER		GENMASK(25, 19)
#define HAL_TQM_FLOW_INFO5_UPDATE_L4_MARKING_PROB_NUMERATOR	BIT(26)
#define HAL_TQM_FLOW_INFO5_UPDATE_L4_DROPPING_CREDIT		BIT(27)
#define HAL_TQM_FLOW_INFO5_UPDATE_L4_DROPPING_PROB_NUMERATOR	BIT(28)
#define HAL_TQM_FLOW_INFO5_UPDATE_L4_DROPPING_PROB_DENOMINATOR	BIT(29)

#define HAL_TQM_FLOW_INFO6_L4_MARKING_PROB_NUMERATOR		GENMASK(9, 0)

struct hal_tqm_update_tx_msdu_flow {
	struct hal_uniform_tqm_cmd_hdr cmd_hdr;
	__le32 flow_queue_addr_31_0;
	__le32 info0;
	__le32 info1;
	__le16 slow_drop_threshold;
	__le16 medium_drop_threshold;
	__le16 hard_drop_threshold;
	__le16 sw_notification_threshold;
	__le16 sw_notification_threshold_2;
	__le16 sw_peer_id;
	__le32 info2;
	__le32 info3;
	__le16 green_msdu_drop_threshold;
	__le16 yellow_msdu_drop_threshold;
	__le16 red_msdu_drop_threshold;
	__le16 info4;
	__le32 info5;
	__le16 info6;
	__le16 l4s_dropping_credit;
	__le16 l4s_dropping_probability_numerator;
	__le16 l4s_dropping_probability_denominator;
	__le32 rsvd0[5];
} __packed;

/*
 * QCN9625 HW1.0 (E3R65) layout of hal_tqm_update_tx_msdu_flow. Identical to
 * the HW2.0 (E3R86) struct above except the trailing reserved field: HW1.0 has
 * a scalar rsvd0 (1 word) where HW2.0 grew it to rsvd0[5] (+16 bytes). All
 * functional fields are at identical offsets, so a single packer fills either
 * layout; only sizeof() — the TLV length written to the descriptor and the TQM
 * command-ring entry size — differs. Keep this in sync with the struct above
 * for every field except rsvd0.
 */
struct hal_tqm_update_tx_msdu_flow_hw10 {
	struct hal_uniform_tqm_cmd_hdr cmd_hdr;
	__le32 flow_queue_addr_31_0;
	__le32 info0;
	__le32 info1;
	__le16 slow_drop_threshold;
	__le16 medium_drop_threshold;
	__le16 hard_drop_threshold;
	__le16 sw_notification_threshold;
	__le16 sw_notification_threshold_2;
	__le16 sw_peer_id;
	__le32 info2;
	__le32 info3;
	__le16 green_msdu_drop_threshold;
	__le16 yellow_msdu_drop_threshold;
	__le16 red_msdu_drop_threshold;
	__le16 info4;
	__le32 info5;
	__le16 info6;
	__le16 l4s_dropping_credit;
	__le16 l4s_dropping_probability_numerator;
	__le16 l4s_dropping_probability_denominator;
	__le32 rsvd0;
} __packed;
#define HAL_TQM_FLOW_UPDSTAT_INFO0_REQUIREMENTS_NOT_MET	BIT(0)
#define HAL_TQM_FLOW_UPDSTAT_INFO0_TX_FLOW_NUMBER	GENMASK(31, 8)

#define HAL_TQM_FLOW_UPDSTAT_INFO1_TID			GENMASK(3, 0)

/* info2 (W23) */
#define HAL_TQM_FLOW_UPDSTAT_INFO2_W23_LOOPING_COUNT	GENMASK(31, 28)

struct hal_tqm_update_tx_msdu_flow_status {
	struct hal_uniform_tqm_status_hdr status_hdr;
	__le32 info0;
	__le16 sw_peer_id;
	__le16 info1;
	__le32 rsvd0[18];
	__le32 info2;
} __packed;

#define HAL_FSE_CMD_HDR_INFO0_SEND_TO_CHIP_N \
	{BIT(0), BIT(1), BIT(2), BIT(3), BIT(4)}

#define HAL_FSE_CMD_INFO0_L4_PROTOCOL		GENMASK(7, 0)
#define HAL_FSE_CMD_INFO0_GSE_CTRL		GENMASK(11, 8)
#define HAL_FSE_CMD_INFO0_INDEX_SEARCH_EN	BIT(12)
#define HAL_FSE_CMD_INFO0_CACHE_SET_NUM		GENMASK(17, 13)

#define HAL_FSE_CMD_INFO1_SEARCH_INDEX		GENMASK(19, 0)
#define HAL_FSE_CMD_INFO1_RING_ID		GENMASK(27, 20)
#define HAL_FSE_CMD_INFO1_LOOPING_COUNT		GENMASK(31, 28)

/* GSE Control operations */
enum hal_fse_gse_ctrl {
	HAL_FSE_GSE_CTRL_RD_STAT = 0,
	HAL_FSE_GSE_CTRL_SRCH_DIS = 1,
	HAL_FSE_GSE_CTRL_WR_BK_SINGLE = 2,
	HAL_FSE_GSE_CTRL_WR_BK_ALL = 3,
	HAL_FSE_GSE_CTRL_INVAL_SINGLE = 4,
	HAL_FSE_GSE_CTRL_INVAL_ALL = 5,
	HAL_FSE_GSE_CTRL_WR_BK_INVAL_SINGLE = 6,
	HAL_FSE_GSE_CTRL_WR_BK_INVAL_ALL = 7,
	HAL_FSE_GSE_CTRL_CLR_STAT_SINGLE = 8,
};

/* FSE Command structures */
struct hal_fse_cmd_hdr {
	__le16 info0;
	__le16 rsvd0;
} __packed;


struct hal_fse_cmd {
	struct hal_fse_cmd_hdr cmd;
	__le32 src_ip[4];
	__le32 dest_ip[4];
	__le16 src_port;
	__le16 dest_port;
	__le32 info0;
	__le32 meta_data[2];
	__le32 rsvd[2];
	__le32 info1;
} __packed;

#define HAL_SAM_CMD_HDR_INFO0_STATUS_REQUIRED_TO_SW	BIT(0)
#define HAL_SAM_CMD_HDR_INFO0_STATUS_REQUIRED_TO_LINK_FW	BIT(1)
#define HAL_SAM_CMD_HDR_INFO0_TO_SAM_COMMUNICATION	BIT(2)

struct hal_uniform_sam_cmd_hdr {
	__le16 sam_cmd_number;
	__le16 info0;
} __packed;

#define HAL_SAM_MPDU_INFO0_START_MPDU_QUEUE_SAM_ID	GENMASK(10, 0)
#define HAL_SAM_MPDU_INFO0_END_MPDU_QUEUE_SAM_ID	GENMASK(23, 13)
#define HAL_SAM_MPDU_INFO0_LINK_MASK			GENMASK(28, 24)

struct hal_sam_mpdu_queue_clear_programming {
	struct hal_uniform_sam_cmd_hdr cmd_hdr;
	__le32 info0;
} __packed;

#define HAL_SAM_MSDU_INFO0_START_MSDU_QUEUE_SAM_ID	GENMASK(12, 0)
#define HAL_SAM_MSDU_INFO0_END_MSDU_QUEUE_SAM_ID	GENMASK(25, 13)
#define HAL_SAM_MSDU_INFO0_LINK_MASK			GENMASK(30, 26)

struct hal_sam_msdu_queue_clear_programming {
	struct hal_uniform_sam_cmd_hdr cmd_hdr;
	__le32 info0;
} __packed;

#define HAL_SAM_PEER_INFO0_START_PEER_ID		GENMASK(12, 0)
#define HAL_SAM_PEER_INFO0_END_PEER_ID			GENMASK(25, 13)
#define HAL_SAM_PEER_INFO0_LINK_MASK			GENMASK(30, 26)

struct hal_sam_peer_clear_programming {
	struct hal_uniform_sam_cmd_hdr cmd_hdr;
	__le32 info0;
} __packed;

#define HAL_SAM_STATUS_HDR_INFO0_STATUS_REQUIRED_TO_SW		BIT(0)
#define HAL_SAM_STATUS_HDR_INFO0_STATUS_REQUIRED_TO_LINK_FW	BIT(1)
#define HAL_SAM_STATUS_HDR_INFO0_TO_SAM_COMMUNICATION		BIT(2)

struct hal_uniform_sam_status_hdr {
	__le16 sam_cmd_number;
	__le16 info0;
} __packed;

struct hal_sam_cmd_status {
	struct hal_uniform_sam_status_hdr status_hdr;
	__le32 rsvd0;
} __packed;

#define MAX_TX_PEER_BAND	5
#define MAX_RX_PEER_BAND	5

#define TX_PEER_BAND_TELEMETRY_STATS_INFO0_LOWER_SUCCESS_BYTES	GENMASK(31, 0)
#define TX_PEER_BAND_TELEMETRY_STATS_INFO1_UPPER_SUCCESS_BYTES	GENMASK(1, 0)
#define TX_PEER_BAND_TELEMETRY_STATS_INFO1_NUM_SUCCESS_PACKETS	GENMASK(31, 7)
#define TX_PEER_BAND_TELEMETRY_STATS_INFO2_NUM_RETRANSMISSIONS	GENMASK(24, 0)
#define TX_PEER_BAND_TELEMETRY_STATS_INFO4_LOWER_SUM_ACK_RSSI	GENMASK(31, 0)
#define TX_PEER_BAND_TELEMETRY_STATS_INFO5_UPPER_SUM_ACK_RSSI	BIT(0)
#define TX_PEER_BAND_TELEMETRY_STATS_INFO5_PPDU_COUNT		GENMASK(22, 1)
#define TX_PEER_BAND_TELEMETRY_STATS_INFO5_UPPER_SUM_PHY_RATES	GENMASK(31, 25)
#define TX_PEER_BAND_TELEMETRY_STATS_INFO6_LOWER_SUM_PHY_RATES	GENMASK(31, 0)

/* tx_peer_band_stats
 *
 * tx_peer_band_stats: indicate peer_id of the packet
 * success_bytes: tx successful packets bytes per window per peer per band
 * num_success_packets: no of successful packets per window per peer per band
 * num_retransmissions: no of retransmissions per window per peer per band
 * sum_of_ack_rssi: signed sum of ack rssi per window per peer per band
 * ppdu_count: no of ppdu for ack rrsi average and transmit phy rate
 * sum_of_phy_rates: sum of tx phy rates per window per peer per band
 */
struct tx_peer_band_telemetry {
	__le32 info0;
	__le32 info1;
	__le32 info2;
	__le32 info3;
	__le32 info4;
	__le32 info5;
	__le32 info6;
} __packed;

#define TX_PEER_TELEMETRY_DESC_INFO0_STATS_ID		GENMASK(10, 0)
#define TX_PEER_TELEMETRY_DESC_INFO0_NUM_WINDOWS		GENMASK(13, 11)
#define TX_PEER_TELEMETRY_DESC_INFO0_PEER_STATS_SIGNATURE	GENMASK(31, 16)
#define TX_PEER_TELEMETRY_DESC_INFO1_UPPER_FAIL_BYTES		GENMASK(1, 0)
#define TX_PEER_TELEMETRY_DESC_INFO1_UPPER_DROP_BYTES		GENMASK(31, 30)
#define TX_PEER_TELEMETRY_DESC_INFO2_NUM_FAIL_PACKETS		GENMASK(24, 0)
#define TX_PEER_TELEMETRY_DESC_INFO3_NUM_DROPPED1_PACKETS	GENMASK(24, 0)
#define TX_PEER_TELEMETRY_DESC_INFO4_NUM_DROPPED2_PACKETS	GENMASK(24, 0)
#define TX_PEER_TELEMETRY_DESC_INFO5_NUM_RETRIED_PACKETS	GENMASK(24, 0)
#define TX_PEER_TELEMETRY_DESC_INFO6_RING_ID			GENMASK(27, 20)
#define TX_PEER_TELEMETRY_DESC_INFO6_LOOPING_COUNT		GENMASK(31, 28)

/* tx_peer_telemetry_desc
 * stats_id: indicate TASC stats_id of the packet.
 * num_windows: The number of windows the stats are reported for -1, in case
 *	of back-pressure and in case of no backpressure this will be zero.
 * peer_stats_signature: legal 0x3333
 * tx_peer_band_stats: per-peer per-band stats information for all bands
 * fail_bytes: tx failed packet bytes per window per peer
 * drop_bytes: tx dropped packets bytes per window per peer
 * num_fail_packets: no of tx failed packets per window per peer
 * num_dropped1_packets: no of tx dropped packets with reason set 1, per
 *	window per peer
 * num_dropped2_packets: no of tx drooped packets with reason set 2, per
 *	window per peer
 * num_tried_packets: no of retransmitted packets per window per peer
 * ring_id: buffer pointer ring ID
 * looping_count: a count values that indicates the no of times the producer
 *	of entries into the ring has looped around the ring.
 */
struct tx_peer_telemetry_desc {
	__le32 info0;
	struct tx_peer_band_telemetry peer_band[MAX_TX_PEER_BAND];
	__le32 lower_fail_bytes;
	__le32 info1;
	__le32 lower_drop_bytes;
	__le32 info2;
	__le32 info3;
	__le32 info4;
	__le32 info5;
	__le32 info6;
} __packed;

#define RX_PEER_BAND_TELEMETRY_STATS_INFO0_LOWER_SUCCESS_BYTES		GENMASK(31, 0)
#define RX_PEER_BAND_TELEMETRY_STATS_INFO1_UPPER_SUCCESS_BYTES		GENMASK(2, 0)
#define RX_PEER_BAND_TELEMETRY_STATS_INFO1_UPPER_SUCCESS_GCAST_BYTES	GENMASK(31, 29)
#define RX_PEER_BAND_TELEMETRY_STATS_INFO2_LOWER_SUCCESS_GCAST_BYTES	GENMASK(31, 0)
#define RX_PEER_BAND_TELEMETRY_STATS_INFO3_LOWER_FAIL_MPDU_BYTES	GENMASK(31, 0)
#define RX_PEER_BAND_TELEMETRY_STATS_INFO4_UPPER_FAIL_MPDU_BYTES	GENMASK(2, 0)
#define RX_PEER_BAND_TELEMETRY_STATS_INFO4_NUM_SUCES_FST_TRY_UCAST_PKT	GENMASK(31, 6)
#define RX_PEER_BAND_TELEMETRY_STATS_INFO5_NUM_SUCES_RETRIED_UCAST_PKT	GENMASK(25, 0)
#define RX_PEER_BAND_TELEMETRY_STATS_INFO6_NUM_SUCES_GCAST_PKT		GENMASK(25, 0)
#define RX_PEER_BAND_TELEMETRY_STATS_INFO7_NUM_FAILED_MPDUS		GENMASK(25, 0)
#define RX_PEER_BAND_TELEMETRY_STATS_INFO8_NUM_UCAST_DROPPED1_PKTS	GENMASK(25, 0)
#define RX_PEER_BAND_TELEMETRY_STATS_INFO9_LOWER_SUM_RSSIS		GENMASK(31, 0)
#define RX_PEER_BAND_TELEMETRY_STATS_INFO10_UPPER_SUM_RSSIS		GENMASK(1, 0)
#define RX_PEER_BAND_TELEMETRY_STATS_INFO10_PPDU_COUNT			GENMASK(23, 2)
#define RX_PEER_BAND_TELEMETRY_STATS_INFO10_UPPER_SUM_PHY_RATES		GENMASK(31, 24)
#define RX_PEER_BAND_TELEMETRY_STATS_INFO11_LOWER_SUM_PHY_RATES		GENMASK(31, 0)

/* rx_peer_band_stats
 * success_ucast_bytes: rx successful unicast packet bytes per window per
 *	peer per band
 * success_gcast_bytes: rx successful multicast/broadcast packet bytes per
 *	window per peer per band
 * fail_mpdu_bytes: rx failed mpdu bytes per window per peer per band
 * num_success_first_try_ucast_packets: no of rx successful unicast packets
 *	with retry=0 per window per peer per band
 * num_success_retried_ucast_packets: no of rx successful unicast packets
 *	with retry=1 per window per peer per band
 * num_success_gcast_packets: no of rx successful multicast/broadcast
 *	packets per window per peer per band
 * num_failed_mpdus: no of rx failed mpdus per window per peer per band
 * num_ucast_dropped1_packets: no of rx dropped unicast packets per window
 *	per peer per band
 * sum_rssi: signed sum of rx rssi per window per peer per band
 * ppdu_count: no of ppdu for rrsi average and rx phy rate average
 * sum_of_phy_rates: sum of rx phy rates per window per peer per band
 */

struct rx_peer_band_telemetry {
	__le32 info0;
	__le32 info1;
	__le32 info2;
	__le32 info3;
	__le32 info4;
	__le32 info5;
	__le32 info6;
	__le32 info7;
	__le32 info8;
	__le32 info9;
	__le32 info10;
	__le32 info11;
} __packed;

#define RX_PEER_TELEMETRY_DESC_INFO0_STATS_ID		GENMASK(10, 0)
#define RX_PEER_TELEMETRY_DESC_INFO0_NUM_WINDOWS		GENMASK(13, 11)
#define RX_PEER_TELEMETRY_DESC_INFO0_PEER_STATS_SIGNATURE	GENMASK(31, 16)
#define RX_PEER_TELEMETRY_DESC_INFO1_UPPER_DROP_UCAST_BYTES	GENMASK(2, 0)
#define RX_PEER_TELEMETRY_DESC_INFO1_UPPER_DRP_GCAST_BYTES	GENMASK(31, 29)
#define RX_PEER_TELEMETRY_DESC_INFO2_NUM_UCAST_DROPPED2_PACKETS	GENMASK(25, 0)
#define RX_PEER_TELEMETRY_DESC_INFO3_NUM_GCAST_DROPPED_PACKETS	GENMASK(25, 0)
#define RX_PEER_TELEMETRY_DESC_INFO4_RING_ID			GENMASK(27, 20)
#define RX_PEER_TELEMETRY_DESC_INFO4_LOOPING_COUNT		GENMASK(31, 28)

/* rx_peer_telemetry_desc
 * stats_id: indicate TASC stats_id of the packet.
 * num_windows: The number of windows the stats are reported for -1, in case
 *	of back-pressure and in case of no backpressure this will be zero.
 * peer_stats_signature: legal 0x3333
 * rx_peer_band_stats: per-peer per-band stats information for all bands
 * drop_ucast_bytes: rx dropped unicast packet bytes per window per peer
 * drop_gcast_bytes: rx_dropped multicast/broadcast packet bytes per window
 *	per peer
 * num_ucast_dropped2_packets: no of rx dropped unicast packets per window
 *	per peer
 * num_gcast_dropped_packets: no of rx dropped multicast/broadcast packets
+ * per window per peer
 * ring_id: buffer pointer ring ID
 * looping_count: a count values that indicates the no of times the producer
 *	of entries into the ring has looped around the ring.
 */

struct rx_peer_telemetry_desc {
	__le32 info0;
	struct rx_peer_band_telemetry peer_band[MAX_RX_PEER_BAND];
	__le32 lower_drop_ucast_bytes;
	__le32 info1;
	__le32 lower_drop_gcast_bytes;
	__le32 info2;
	__le32 info3;
	__le32 rsvd0;
	__le32 info4;
} __packed;

#endif /* ATH12K_HAL_DESC_H */
