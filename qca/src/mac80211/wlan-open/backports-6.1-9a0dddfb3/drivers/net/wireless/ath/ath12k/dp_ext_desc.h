/* SPDX-License-Identifier: BSD-3-Clause-Clear */
/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 */

#ifndef ATH12K_DP_EXT_DESC_H
#define ATH12K_DP_EXT_DESC_H

#define ATH12K_DP_EXT_DESC_CTL_WORDS	6	/* Words for control information */
#define ATH12K_DP_EXT_DESC_BUF_WORDS	12	/* Words for buffer information */
#define ATH12K_DP_EXT_DESC_RES_WORDS	14	/* Words reserved for HW */
#define ATH12K_DP_EXT_DESC_TOT_WORDS	32	/* Total Words */

#define ATH12K_DP_EXT_DESC_CTL_BYTES	(ATH12K_DP_EXT_DESC_CTL_WORDS * 4)
#define ATH12K_DP_EXT_DESC_BUF_BYTES	(ATH12K_DP_EXT_DESC_BUF_WORDS * 4)
#define ATH12K_DP_EXT_DESC_RES_BYTES	(ATH12K_DP_EXT_DESC_RES_WORDS * 4)
#define ATH12K_DP_EXT_DESC_TOT_BYTES	(ATH12K_DP_EXT_DESC_TOT_WORDS * 4)

#define ATH12K_TX_MSDU_EXT_SZ	(ATH12K_DP_EXT_DESC_CTL_BYTES + \
				 ATH12K_DP_EXT_DESC_BUF_BYTES)

#define ATH12K_DP_EXT_DESC_SZ			sizeof(struct ath12k_dp_ext_desc)
#define ATH12K_DP_EXT_DESC_HW_SZ		sizeof(struct ath12k_dp_ext_desc_hw)
#define ATH12K_DP_EXT_DESC_SPARE_SZ		64	/* Spare bytes to store context */

#define ATH12K_DP_EXT_DESC_BUF_INFO_SZ	2	/* No of words - one buf ptr needs 2 */
#define ATH12K_DP_EXT_DESC_BUF_INFO_CNT	6	/* 6 buffer pointers, each 2 words sz */
#define ATH12K_DP_EXT_DESC_BUF_INFO_PTR_LO	GENMASK(31, 0)
#define ATH12K_DP_EXT_DESC_BUF_INFO_PTR_HI	GENMASK(7, 0)
#define ATH12K_DP_EXT_DESC_BUF_INFO_LEN		GENMASK(31, 16)

#define ATH12K_DP_EXT_DESC_BUF_EXTN_OVERRIDE    BIT(8)
#define ATH12K_DP_EXT_DESCBUF_ENCAP_TYPE        GENMASK(10, 9)
#define ATH12K_DP_EXT_DESCBUF_ENCRYPT_TYPE      GENMASK(14, 11)

#include "core.h"
#include "dp_tx.h"

/*
 * Extension descriptor cache management structure
 */
struct ath12k_dp_ext_desc_cache;

/*
 * HW Extension descriptor
 */
struct ath12k_dp_ext_desc_hw {
	__le32 ctl_info[ATH12K_DP_EXT_DESC_CTL_WORDS];	/* Operation specific ctrl info */
	__le32 buf_info[ATH12K_DP_EXT_DESC_BUF_WORDS];	/* Buffer specific information */
	__le32 reserved[ATH12K_DP_EXT_DESC_RES_WORDS];	/* Reserved for HW use */
};

/*
 * Top half contains HW specific words understood by SW and HW.
 * Lower bytes are SW specific used for storing context per descriptor
 */
struct ath12k_dp_ext_desc {
	struct ath12k_dp_ext_desc_hw desc;		/* HW  Extension descriptor */
	u8 spare_bytes[ATH12K_DP_EXT_DESC_SPARE_SZ];	/* Spare mem of ext descriptor */
} __packed;

/*
 * Required to pass any additional information for ext operations.
 */
struct ath12k_dp_ext_info {
	u16 tcl_metadata;
	u8 reserved[2];
};

static inline void
__ext_desc_set_ctl(struct ath12k_dp_ext_desc_hw *desc, u32 val, u8 widx)
{
	desc->ctl_info[widx] = le32_encode_bits(val, GENMASK(31, 0));
}

static inline u32
__ext_desc_get_ctl(struct ath12k_dp_ext_desc_hw *desc, u8 widx)
{
	return le32_get_bits(desc->ctl_info[widx], GENMASK(31, 0));
}

static inline void
__ext_desc_upd_ctl(struct ath12k_dp_ext_desc_hw *desc, u32 val, u8 widx)
{
	desc->ctl_info[widx] |= le32_encode_bits(val, GENMASK(31, 0));
}

static inline void
__set_ext_desc_buf(struct ath12k_dp_ext_desc_hw *desc, dma_addr_t paddr, u16 len, u8 idx)
{
	__le32 *buf = (__le32 *)&desc->buf_info[idx *  ATH12K_DP_EXT_DESC_BUF_INFO_SZ];
	u32 paddr_lo = lower_32_bits(paddr);
	u32 paddr_hi = upper_32_bits(paddr);

	buf[0] = le32_encode_bits(paddr_lo, ATH12K_DP_EXT_DESC_BUF_INFO_PTR_LO);
	buf[1] = le32_encode_bits(paddr_hi, ATH12K_DP_EXT_DESC_BUF_INFO_PTR_HI);
	buf[1] |= le32_encode_bits(len, ATH12K_DP_EXT_DESC_BUF_INFO_LEN);
}

static inline void
__get_ext_desc_buf(struct ath12k_dp_ext_desc_hw *desc, dma_addr_t *paddr, u16 *len,
		   u8 idx)
{
	__le32 *buf = (__le32 *)&desc->buf_info[idx * ATH12K_DP_EXT_DESC_BUF_INFO_SZ];
	u32 paddr_lo = le32_get_bits(buf[0], ATH12K_DP_EXT_DESC_BUF_INFO_PTR_LO);
	u32 paddr_hi = le32_get_bits(buf[1], ATH12K_DP_EXT_DESC_BUF_INFO_PTR_HI);

	*paddr = ((u64)paddr_hi << 32) | paddr_lo;
	*len = le32_get_bits(buf[1], ATH12K_DP_EXT_DESC_BUF_INFO_LEN);
}

static inline void
ath12k_dp_ext_desc_override_set(struct ath12k_dp_ext_desc_hw *desc,
				struct ath12k_dp_ext_desc_msdu_info *ext_msdu_info)
{
	u8 idx = 0;
	__le32 *buf = (__le32 *)&desc->buf_info[idx * ATH12K_DP_EXT_DESC_BUF_INFO_SZ];

	buf[1] |= le32_encode_bits(0x1, ATH12K_DP_EXT_DESC_BUF_EXTN_OVERRIDE);
	buf[1] |= le32_encode_bits(ext_msdu_info->encap_type,
				   ATH12K_DP_EXT_DESCBUF_ENCAP_TYPE);
	buf[1] |= le32_encode_bits(ext_msdu_info->encrypt_type,
				   ATH12K_DP_EXT_DESCBUF_ENCRYPT_TYPE);
}

static inline __le32 *
__ext_desc_get_rsvd(struct ath12k_dp_ext_desc_hw *desc, u8 widx)
{
	if (widx >= ATH12K_DP_EXT_DESC_RES_WORDS)
		return NULL;

	return &desc->reserved[widx];
}

/*
 * Convenience macros to set, get and update control word 0 of tx extension descriptor.
 */
#define ath12k_dp_ext_desc_set_ctl0(_d, _value) \
	__ext_desc_set_ctl(&(_d)->desc, (_value), 0)
#define ath12k_dp_ext_desc_get_ctl0(_d) \
	__ext_desc_get_ctl(&(_d)->desc, 0)
#define ath12k_dp_ext_desc_upd_ctl0(_d, _value) \
	__ext_desc_upd_ctl(&(_d)->desc, (_value), 0)

/*
 * Convenience macros to set, get and update control word 1 of tx extension descriptor.
 */
#define ath12k_dp_ext_desc_set_ctl1(_d, _value) \
	__ext_desc_set_ctl(&(_d)->desc, (_value), 1)
#define ath12k_dp_ext_desc_get_ctl1(_d) \
	_ext_desc_get_ctl(&(_d)->desc, 1)
#define ath12k_dp_ext_desc_upd_ctl1(_d, _value) \
	_ext_desc_upd_ctl(&(_d)->desc, (_value), 1)

/*
 * Convenience macros to set, get and update control word 2 of tx extension descriptor.
 */
#define ath12k_dp_ext_desc_set_ctl2(_d, _value) \
	__ext_desc_set_ctl(&(_d)->desc, (_value), 2)
#define ath12k_dp_ext_desc_get_ctl2(_d) \
	__ext_desc_get_ctl(&(_d)->desc, 2)
#define ath12k_dp_ext_desc_upd_ctl2(_d, _value) \
	__ext_desc_upd_ctl(&(_d)->desc, (_value), 2)

/*
 * Convenience macros to set, get and update control word 3 of tx extension descriptor.
 */
#define ath12k_dp_ext_desc_set_ctl3(_d, _value) \
	__ext_desc_set_ctl(&(_d)->desc, (_value), 3)
#define ath12k_dp_ext_desc_get_ctl3(_d) \
	__ext_desc_get_ctl(&(_d)->desc, 3)
#define ath12k_dp_ext_desc_upd_ctl3(_d, _value) \
	__ext_desc_upd_ctl(&(_d)->desc, (_value), 3)

/*
 * Convenience macros to set, get and update control word 4 of tx extension descriptor.
 */
#define ath12k_dp_ext_desc_set_ctl4(_d, _value) \
	__ext_desc_set_ctl(&(_d)->desc, (_value), 4)
#define ath12k_dp_ext_desc_get_ctl4(_d) \
	__ext_desc_get_ctl(&(_d)->desc, 4)
#define ath12k_dp_ext_desc_upd_ctl4(_d, _value) \
	__ext_desc_upd_ctl(&(_d)->desc, (_value), 4)

/*
 * Convenience macros to set, get and update control word 5 of tx extension descriptor.
 */
#define ath12k_dp_ext_desc_set_ctl5(_d, _value) \
	__ext_desc_set_ctl(&(_d)->desc, (_value), 5)
#define ath12k_dp_ext_desc_get_ctl5(_d) \
	__ext_desc_get_ctl(&(_d)->desc, 5)
#define ath12k_dp_ext_desc_upd_ctl5(_d, _value) \
	__ext_desc_upd_ctl(&(_d)->desc, (_value), 5)

/*
 * Convenience macros to program address of buffers in
 * buffer pointer fields of tx extension descriptor.
 */
#define ath12k_dp_ext_desc_set_buf0(_d, _addr, _len) \
	__set_ext_desc_buf(&(_d)->desc, (_addr), (_len), 0)
#define ath12k_dp_ext_desc_set_buf1(_d, _addr, _len) \
	__set_ext_desc_buf(&(_d)->desc, (_addr), (_len), 1)
#define ath12k_dp_ext_desc_set_buf2(_d, _addr, _len) \
	__set_ext_desc_buf(&(_d)->desc, (_addr), (_len), 2)
#define ath12k_dp_ext_desc_set_buf3(_d, _addr, _len) \
	__set_ext_desc_buf(&(_d)->desc, (_addr), (_len), 3)
#define ath12k_dp_ext_desc_set_buf4(_d, _addr, _len) \
	__set_ext_desc_buf(&(_d)->desc, (_addr), (_len), 4)
#define ath12k_dp_ext_desc_set_buf5(_d, _addr, _len) \
	__set_ext_desc_buf(&(_d)->desc, (_addr), (_len), 5)

/*
 * Convenience macros to extract address of buffers in
 * buffer pointer fields of tx extension descriptor.
 */
#define ath12k_dp_ext_desc_get_buf0(_d, _addr, _len) \
	__get_ext_desc_buf(&(_d)->desc, (_addr), (_len), 0)
#define ath12k_dp_ext_desc_get_buf1(_d, _addr, _len) \
	__get_ext_desc_buf(&(_d)->desc, (_addr), (_len), 1)
#define ath12k_dp_ext_desc_get_buf2(_d, _addr, _len) \
	__get_ext_desc_buf(&(_d)->desc, (_addr), (_len), 2)
#define ath12k_dp_ext_desc_get_buf3(_d, _addr, _len) \
	__get_ext_desc_buf(&(_d)->desc, (_addr), (_len), 3)
#define ath12k_dp_ext_desc_get_buf4(_d, _addr, _len) \
	__get_ext_desc_buf(&(_d)->desc, (_addr), (_len), 4)
#define ath12k_dp_ext_desc_get_buf5(_d, _addr, _len) \
	__get_ext_desc_buf(&(_d)->desc, (_addr), (_len), 5)

/*
 * Convenience macros to get and update reserved word.
 */
#define ath12k_dp_ext_desc_get_rsvd0(_d) \
	__ext_desc_get_rsvd(&(_d)->desc, 0)

/*
 * ath12k_dp_ext_desc_set_ctl() - Set a control word in tx extension descriptor
 */
static inline void ath12k_dp_ext_desc_set_ctl(struct ath12k_dp_ext_desc *ext,
					      u32 value, u8 widx)
{
	WARN_ON_ONCE(widx >= ATH12K_DP_EXT_DESC_CTL_WORDS);
	__ext_desc_set_ctl(&ext->desc, value, widx % ATH12K_DP_EXT_DESC_CTL_WORDS);
}

/*
 * ath12k_dp_ext_desc_get_ctl() - Get a control word from tx extension descriptor.
 */
static inline u32 ath12k_dp_ext_desc_get_ctl(struct ath12k_dp_ext_desc *ext, u8 widx)
{
	WARN_ON_ONCE(widx >= ATH12K_DP_EXT_DESC_CTL_WORDS);
	return __ext_desc_get_ctl(&ext->desc, widx % ATH12K_DP_EXT_DESC_CTL_WORDS);
}

/*
 * ath12k_dp_ext_desc_upd_ctl() - Update specific bits in a control word
 * of tx extension descriptor.
 */
static inline void ath12k_dp_ext_desc_upd_ctl(struct ath12k_dp_ext_desc *ext,
					      u32 value, u8 widx)
{
	WARN_ON_ONCE(widx >= ATH12K_DP_EXT_DESC_CTL_WORDS);
	/*Update ctl word without overriding existing bits*/
	__ext_desc_upd_ctl(&ext->desc, value, widx % ATH12K_DP_EXT_DESC_CTL_WORDS);
}

/*
 * ath12k_dp_ext_desc_set_buf() - Program the address of the buffer in the
 * buffer pointer fields of tx extension descriptors a buffer info ptr
 */
static inline void ath12k_dp_ext_desc_set_buf(struct ath12k_dp_ext_desc *ext,
					      dma_addr_t paddr, u16 len, u8 idx)
{
	WARN_ON_ONCE(idx >= ATH12K_DP_EXT_DESC_BUF_INFO_CNT);
	__set_ext_desc_buf(&ext->desc, paddr, len, idx);
}

/*
 * ath12k_dp_ext_desc_set_buf() - Extract the address of the buffer in the
 * buffer pointer fields of tx extension descriptors a buffer info ptr
 */
static inline void ath12k_dp_ext_desc_get_buf(struct ath12k_dp_ext_desc *ext,
					      dma_addr_t *paddr, u16 *len, u8 idx)
{
	WARN_ON_ONCE(idx >= ATH12K_DP_EXT_DESC_BUF_INFO_CNT);
	__get_ext_desc_buf(&ext->desc, paddr, len, idx);
}

/*
 * ath12k_dp_ext_desc_get_spare() - Get pointer to spare memory.
 */
static inline void *ath12k_dp_ext_desc_get_spare(struct ath12k_dp_ext_desc *ext,
						 const u16 len)
{
	BUILD_BUG_ON(len >= ATH12K_DP_EXT_DESC_SPARE_SZ);
	return ext->spare_bytes;
}

/* Function declarations for extension descriptor management. */
int ath12k_dp_ext_desc_cache_init(struct ath12k_dp *dp);
void ath12k_dp_ext_desc_cache_deinit(struct ath12k_dp *dp);
struct kmem_cache *ath12k_dp_ext_desc_get_cache(struct ath12k_dp *dp);
dma_addr_t ath12k_dp_ext_desc_map(struct ath12k_dp *dp, struct ath12k_dp_ext_desc *ext);
void ath12k_dp_ext_desc_unmap(struct ath12k_dp *dp, dma_addr_t paddr);

#endif /* ATH12K_DP_EXT_DESC_H */
