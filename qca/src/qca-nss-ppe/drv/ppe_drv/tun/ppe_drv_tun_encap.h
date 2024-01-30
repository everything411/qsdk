/*
 * Copyright (c) 2022 Qualcomm Innovation Center, Inc. All rights reserved.
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
#ifndef _PPE_DRV_TUN_ENCAP_H_
#define _PPE_DRV_TUN_ENCAP_H_

#define PPE_DRV_TUN_ENCAP_ENTRIES	FAL_TUNNEL_DECAP_ENTRY_MAX
#define PPE_DRV_TUN_ENCAP_HDR_DATA_SIZE	128

/*
 * ppe_drv_tun_encap
 *	EG tunnel control table information
 */
struct ppe_drv_tun_encap {
	struct  ppe_drv_port *port;	/* Pointer to associated port structure */
	uint32_t hdr[PPE_DRV_TUN_ENCAP_HDR_DATA_SIZE/sizeof(uint32_t)];
					/* Index into the TL table */
	struct kref ref;		/* Reference counter */
	uint8_t tun_idx;		/* Tunnel index */
	uint8_t tun_len;		/* Tunnel header length */
	uint8_t l3_offset;		/* Tunnel L3 offset */
	uint8_t l4_offset;		/* Tunnel L4 offset */
	uint8_t rule_id;		/* EG edit rule index for MAP-T */
	uint8_t l4_offset_valid;	/* is L4 offset valid in header */
};

uint16_t ppe_drv_tun_encap_get_len(struct ppe_drv_tun_encap *ptec);
void ppe_drv_tun_encap_set_l3_offset(struct ppe_drv_tun_encap *ptec, uint8_t l3_offset);
void ppe_drv_tun_encap_set_l4_offset(struct ppe_drv_tun_encap *ptec, uint8_t l4_offset);
void ppe_drv_tun_encap_set_rule_id(struct ppe_drv_tun_encap *ptec, uint8_t rule_id);
bool ppe_drv_tun_encap_tun_idx_configure(struct ppe_drv_tun_encap *ptec, uint32_t port_num,
		bool tunnel_id_valid);
uint8_t ppe_drv_tun_encap_get_tun_idx(struct ppe_drv_tun_encap *ptec);
bool ppe_drv_tun_encap_configure(struct ppe_drv_tun_encap *ptec, struct ppe_drv_tun_cmn_ctx *th,
		struct ppe_drv_tun_cmn_ctx_l2 *l2_hdr);
bool ppe_drv_tun_encap_deref(struct ppe_drv_tun_encap *ptec);
struct ppe_drv_tun_encap *ppe_drv_tun_encap_ref(struct ppe_drv_tun_encap *ptec);
struct ppe_drv_tun_encap *ppe_drv_tun_encap_alloc(struct ppe_drv *p);
void ppe_drv_tun_encap_entries_free(struct ppe_drv_tun_encap *ptun_ec);
struct ppe_drv_tun_encap *ppe_drv_tun_encap_entries_alloc(struct ppe_drv *p);
#endif /* _PPE_DRV_TUN_ENCAP_H_ */
