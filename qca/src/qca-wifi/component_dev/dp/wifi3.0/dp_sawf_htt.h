/*
 * Copyright (c) 2022 Qualcomm Innovation Center, Inc. All rights reserved.
 *
 * Permission to use, copy, modify, and/or distribute this software for
 * any purpose with or without fee is hereby granted, provided that the
 * above copyright notice and this permission notice appear in all
 * copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL
 * WARRANTIES WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE
 * AUTHOR BE LIABLE FOR ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL
 * DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR
 * PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER
 * TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
 * PERFORMANCE OF THIS SOFTWARE.
 */

#include <dp_types.h>

QDF_STATUS
dp_htt_h2t_sawf_def_queues_map_req(struct htt_soc *soc,
				   uint8_t svc_class_id,
				   uint16_t peer_id);

QDF_STATUS
dp_htt_h2t_sawf_def_queues_unmap_req(struct htt_soc *soc,
				     uint8_t svc_id, uint16_t peer_id);

QDF_STATUS
dp_htt_h2t_sawf_def_queues_map_report_req(struct htt_soc *soc,
					  uint16_t peer_id, uint8_t tid_mask);

QDF_STATUS
dp_htt_sawf_def_queues_map_report_conf(struct htt_soc *soc,
				       uint32_t *msg_word,
				       qdf_nbuf_t htt_t2h_msg);

QDF_STATUS
dp_htt_sawf_msduq_map(struct htt_soc *soc, uint32_t *msg_word,
		      qdf_nbuf_t htt_t2h_msg);

/*
 * dp_htt_sawf_dynamic_ast_update() - Handle dynamic AST update for SAWF peers
 * @soc: HTT SOC handle
 * @msg_word: Pointer to htt msg word.
 * @htt_t2h_msg: HTT buffer
 *
 * @Return: QDF_STATUS_SUCCESS on success
 */
QDF_STATUS
dp_htt_sawf_dynamic_ast_update(struct htt_soc *soc, uint32_t *msg_word,
			       qdf_nbuf_t htt_t2h_msg);

/*
 * dp_sawf_htt_h2t_mpdu_stats_req() - Send MPDU stats request to target
 * @soc: HTT SOC handle
 * @stats_type: MPDU stats type
 * @enable: 1: Enable 0: Disable
 * @config_param0: Opaque configuration
 * @config_param1: Opaque configuration
 * @config_param2: Opaque configuration
 * @config_param3: Opaque configuration
 *
 * @Return: QDF_STATUS_SUCCESS on success
 */
QDF_STATUS
dp_sawf_htt_h2t_mpdu_stats_req(struct htt_soc *soc,
			       uint8_t stats_type, uint8_t enable,
			       uint32_t config_param0,
			       uint32_t config_param1,
			       uint32_t config_param2,
			       uint32_t config_param3);

/*
 * dp_sawf_htt_mpdu_stats_handler() - Handle MPDU stats sent by target
 * @soc: HTT SOC handle
 * @htt_t2h_msg: HTT buffer
 *
 * @Return: QDF_STATUS_SUCCESS on success
 */
QDF_STATUS
dp_sawf_htt_mpdu_stats_handler(struct htt_soc *soc,
			       qdf_nbuf_t htt_t2h_msg);
