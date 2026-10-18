/*
 * Copyright (c) 2022, 2025 Qualcomm Innovation Center, Inc. All rights reserved.
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

#ifndef _PPE_DRV_TUN_ENCAP_XLATE_RULE_
#define _PPE_DRV_TUN_ENCAP_XLATE_RULE_

#define PPE_DRV_TUN_ENCAP_XLATE_RULE_MAX_RULES 16
#define PPE_DRV_TUN_ENCAP_XLATE_SRC_ENTRY_MAX 2

/*
 * ppe_drv_tun_encap_xlate_src1_sel
 * 	SRC1_SEL mode
 */
enum ppe_drv_tun_encap_xlate_src1_sel {
	PPE_DRV_TUN_ENCAP_XLATE_SRC1_ZERO_DATA,	/* Copy 16 bytes zero data */
	PPE_DRV_TUN_ENCAP_XLATE_SRC1_HDR_DATA,	/* Copy 16 bytes pkt header data */
};

/*
 * ppe_drv_tun_encap_xlate_src2_sel
 *	SRC2_SEL mode
 */
enum ppe_drv_tun_encap_xlate_src2_sel {
	PPE_DRV_TUN_ENCAP_XLATE_SRC2_ZERO_DATA,		/* zero data */
	PPE_DRV_TUN_ENCAP_XLATE_SRC2_PKT_DATA0,		/* pkt data */
	PPE_DRV_TUN_ENCAP_XLATE_SRC2_NAPT_ADDR,		/* NAPT ADDR select */
	PPE_DRV_TUN_ENCAP_XLATE_SRC2_TUNNEL_VNI,	/* Tunnel VNI */
	PPE_DRV_TUN_ENCAP_XLATE_SRC2_PROTO_MAP0,	/* PROTO_MAP 0 data */
	PPE_DRV_TUN_ENCAP_XLATE_SRC2_PROTO_MAP1,	/* PROTO_MAP 1 data */

};

/*
 * ppe_drv_tun_encap_xlate_src3_sel
 *	SRC3_SEL mode
 */
enum ppe_drv_tun_encap_xlate_src3_sel {
	PPE_DRV_TUN_ENCAP_XLATE_SRC3_ZERO_DATA,		/* zero data */
	PPE_DRV_TUN_ENCAP_XLATE_SRC3_PKT_DATA1,		/* pkt data */
	PPE_DRV_TUN_ENCAP_XLATE_SRC3_NAPT_PORT,		/* NAPT Port select */
	PPE_DRV_TUN_ENCAP_XLATE_SRC3_POLICY_ID,		/* Policy ID */
	PPE_DRV_TUN_ENCAP_XLATE_SRC3_HASH_VALUE,	/* Hash value */
	PPE_DRV_TUN_ENCAP_XLATE_SRC3_PROTO_MAP0,	/* PROTO_MAP 0 data */
	PPE_DRV_TUN_ENCAP_XLATE_SRC3_PROTO_MAP1,	/* PROTO_MAP 1 data */
};

/*
 * ppe_drv_tun_encap_edit_rule_entry
 *	edit rule entry
 */
struct ppe_drv_tun_encap_edit_rule_entry {
	bool enable;		/* entry enable */
	uint8_t src_start;	/* offset to the start of header to be copied */
	uint8_t src_width;	/* src width to be updated */
	uint8_t	dest_pos;	/* position where the data needs to be updated (offset from LSB */
};

/*
 * ppe_drv_tun_encap_xlate_data
 *	Xlate Rule data configured
 */
struct ppe_drv_tun_encap_xlate_data {
	enum ppe_drv_tun_encap_xlate_src1_sel src1_sel;		/* src1 data Select */
	uint8_t src1_start;					/* src1 start position */
	enum ppe_drv_tun_encap_xlate_src2_sel src2_sel;		/* src2 data select */
	struct ppe_drv_tun_encap_edit_rule_entry src2_entry[2];	/* src2 entry */
	enum ppe_drv_tun_encap_xlate_src3_sel src3_sel;		/* src3 select */
	struct ppe_drv_tun_encap_edit_rule_entry src3_entry[2];	/* src3 entry */
};

/*
 * ppe_drv_tun_encap_xlate_rule
 *	EG edit rule table information
 */
struct ppe_drv_tun_encap_xlate_rule {
	uint8_t rule_index;				/* encap rule index */
	struct kref ref;				/* Reference counter */
	enum ppe_drv_tun_cmn_ctx_type tun_type;		/* tunnel type */
	struct ppe_drv_tun_encap_xlate_data data;	/* xlate data configured */
};

/*
 * ppe_drv_tun_encap_xlate_cmp_src_sel
 *	Compare edit rule entries
 */
static inline bool ppe_drv_tun_encap_xlate_cmp_src_sel(struct ppe_drv_tun_encap_edit_rule_entry *s1, struct ppe_drv_tun_encap_edit_rule_entry *s2)
{
    return s1->enable == s2->enable &&
           s1->src_start == s2->src_start &&
           s1->src_width == s2->src_width &&
           s1->dest_pos == s2->dest_pos;
}

bool ppe_drv_tun_encap_xlate_rule_deref(struct ppe_drv_tun_encap_xlate_rule *ptecxr);
void ppe_drv_tun_encap_xlate_rule_entries_free(struct ppe_drv_tun_encap_xlate_rule *encap_xlate_rules);
uint8_t ppe_drv_tun_encap_xlate_rule_get_index(struct ppe_drv_tun_encap_xlate_rule *ptecxr);
bool ppe_drv_tun_encap_xlate_rule_configure(struct ppe_drv_tun_encap_xlate_rule *ptecxr,
		struct ppe_drv_tun_cmn_ctx_xlate_rule *rule, int8_t tun_len, uint32_t l2_flags, bool dmr, bool is_gretap_mapt);
struct ppe_drv_tun_encap_xlate_rule *ppe_drv_tun_encap_xlate_rule_entries_alloc(struct ppe_drv *p);
struct ppe_drv_tun_encap_xlate_rule *ppe_drv_tun_encap_xlate_rule_alloc(struct ppe_drv *p, enum ppe_drv_tun_cmn_ctx_type type);
struct ppe_drv_tun_encap_xlate_rule *ppe_drv_tun_encap_xlate_rule_ref(struct ppe_drv_tun_encap_xlate_rule *ptecxr);
struct ppe_drv_tun_encap_xlate_rule *ppe_drv_tun_encap_xlate_rule_exists(enum ppe_drv_tun_cmn_ctx_type type, struct ppe_drv_tun_encap_xlate_data *data);
#endif /* _PPE_DRV_TUN_ENCAP_XLATE_RULE_ */
