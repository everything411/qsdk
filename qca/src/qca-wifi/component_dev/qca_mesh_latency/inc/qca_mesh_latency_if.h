
/*
 * Copyright (c) 2021 The Linux Foundation. All rights reserved.
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

#ifndef __qca_mesh_latency_if_H_
#define __qca_mesh_latency_if_H_

/*
 * qca_mesh_latency_update_peer_param
 *
 * @dst_dev: Destination netdev
 * @src_dev: Source netdev
 * @dest_mac - destination mac address
 * @service_interval_dl - Service interval associated with the tid in DL
 * @burst_size_dl - Burst size corresponding to the tid in DL
 * @service_interval_ul- Service interval associated with the tid in UL
 * @burst_size_u - Burst size corresponding to the tid in UL
 * @priority - priority/tid associated with the peer
 * @add_or_sub -  ADD/SUB command to WMI

*/
struct qca_mesh_latency_update_peer_param{
	struct net_device *dst_dev;
	struct net_device *src_dev;
	uint8_t *dest_mac;
	uint32_t service_interval_dl;
	uint32_t burst_size_dl;
	uint32_t service_interval_ul;
	uint32_t burst_size_ul;
	uint16_t priority;
	uint8_t add_or_sub;
};

int qca_mesh_latency_update_peer_parameter(uint8_t *dest_mac,
		uint32_t service_interval_dl, uint32_t burst_size_dl,
		uint32_t service_interval_ul, uint32_t burst_size_ul,
		uint16_t priority, uint8_t add_or_sub);

int qca_mesh_latency_update_peer_parameter_v2(
		struct qca_mesh_latency_update_peer_param *params);
#endif /* qca_mesh_latency_if_H*/
