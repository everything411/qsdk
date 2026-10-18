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

#ifndef __qca_scs_if_H_
#define __qca_scs_if_H_

/*
 * qca_scs_peer_lookup_n_rule_match
 *
 * @dst_dev: Destination netdev
 * @src_dev: Source netdev
 * @dest_mac_addr - destination mac address
 * @src_mac_addr - source mac address
 * @rule_id -  rule ID

*/
struct qca_scs_peer_lookup_n_rule_match{
	struct net_device *dst_dev;
	struct net_device *src_dev;
	uint8_t *dst_mac_addr;
	uint8_t *src_mac_addr;
	uint32_t rule_id;
};

bool qca_scs_peer_lookup_n_rule_match(uint32_t rule_id, uint8_t *dst_mac_addr);
bool qca_scs_peer_lookup_n_rule_match_v2(
			struct qca_scs_peer_lookup_n_rule_match *params);

#endif /* qca_scs_if_H*/
