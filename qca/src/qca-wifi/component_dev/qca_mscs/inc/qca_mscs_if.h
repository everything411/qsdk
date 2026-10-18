
/*
 * Copyright (c) 2020-2021 The Linux Foundation. All rights reserved.
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

#ifndef __qca_mscs_if_H_
#define __qca_mscs_if_H_
/*
 * qca_mscs_get_priority_param
 *
 * @dst_dev - Destination netdev
 * @src_dev - Source netdev
 * @dest_mac - Source mac address
 * @dest_mac - destination mac address
 * @skb - Socket buffer

*/

struct qca_mscs_get_priority_param {
	struct net_device *dst_dev;
	struct net_device *src_dev;
	uint8_t *src_mac;
	uint8_t *dst_mac;
	struct sk_buff *skb;
};

int qca_mscs_peer_lookup_n_get_priority(uint8_t *src_mac, uint8_t *dst_mac, struct sk_buff *skb);
int qca_mscs_peer_lookup_n_get_priority_v2(
				   struct qca_mscs_get_priority_param *params);
#endif /* qca_mscs_if_H*/
