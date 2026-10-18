/*
 * Copyright (c) 2023-2024, Qualcomm Innovation Center, Inc. All rights reserved.
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

#ifndef _DP_FSE_H__
#define _DP_FSE_H__

/**
 * struct qca_fse_flow_info - FSE flow information
 * @src_ip: Source IP address
 * @src_port: Source port
 * @dest_ip:  Dest IP address
 * @dest_port: Dest port
 * @protocl: Protocol
 * @ring_id: ring_id to receive the rx on
 * @drop_flow: drop the flow
 * @src_dev: Source Netdev
 * @dest_dev: Dest Netdev
 * @src_mac: Source MAC
 * @dest_mac: Destination MAC
 * @fw_svc_id: Service Class ID in forward direction
 * @rv_svc_id: Service Class ID in reverse direction
 * @flow_dscp: DSCP value in forward direction
 * @return_dscp: DSCP value in return direction
 */
struct qca_fse_flow_info {
	uint32_t src_ip[4];
	uint32_t src_port;
	uint32_t dest_ip[4];
	uint32_t dest_port;
	uint8_t protocol;
	uint8_t version;
	uint8_t ring_id;
	uint8_t drop_flow;
	struct net_device *src_dev;
	struct net_device *dest_dev;
	uint8_t *src_mac;
	uint8_t *dest_mac;
	uint32_t fw_svc_id;
	uint32_t rv_svc_id;
	uint8_t flow_dscp;
	uint8_t return_dscp;
};

/*
 * qca_fse_add_rule() - Config FSE rule from ECM - SFE.
 *
 * @fse_info: Information related to FSE rule.
 *
 * Return: status for the API - success/failure.
 */
bool qca_fse_add_rule(struct qca_fse_flow_info *fse_info);

/*
 * qca_fse_delete_rule() - Delete FSE rule from ECM - SFE.
 *
 * @fse_info: Information related to FSE rule.
 *
 * Return: status for the API - success/failure.
 */
bool qca_fse_delete_rule(struct qca_fse_flow_info *fse_info);

#endif
