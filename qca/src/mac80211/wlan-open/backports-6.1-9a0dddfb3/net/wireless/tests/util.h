/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Utilities for cfg80211 unit testing
 *
 * Copyright (C) 2023 Intel Corporation
 */
#ifndef __CFG80211_UTILS_H
#define __CFG80211_UTILS_H

#include <linux/if_ether.h>

void cfg80211_extract_smd_info(struct cfg80211_bss *bss,
			       const u8 *ie, size_t ielen);
#define CHAN2G(_freq)  { \
	.band = NL80211_BAND_2GHZ, \
	.center_freq = (_freq), \
	.hw_value = (_freq), \
}

static const struct ieee80211_channel channels_2ghz[] = {
	CHAN2G(2412), /* Channel 1 */
	CHAN2G(2417), /* Channel 2 */
	CHAN2G(2422), /* Channel 3 */
	CHAN2G(2427), /* Channel 4 */
	CHAN2G(2432), /* Channel 5 */
	CHAN2G(2437), /* Channel 6 */
	CHAN2G(2442), /* Channel 7 */
	CHAN2G(2447), /* Channel 8 */
	CHAN2G(2452), /* Channel 9 */
	CHAN2G(2457), /* Channel 10 */
	CHAN2G(2462), /* Channel 11 */
	CHAN2G(2467), /* Channel 12 */
	CHAN2G(2472), /* Channel 13 */
	CHAN2G(2484), /* Channel 14 */
};

struct t_wiphy_priv {
	struct kunit *test;
	struct cfg80211_ops *ops;

	void *ctx;

	struct ieee80211_supported_band band_2ghz;
	struct ieee80211_channel channels_2ghz[ARRAY_SIZE(channels_2ghz)];
};

#define T_WIPHY(test, ctx) ({						\
		struct wiphy *__wiphy =					\
			kunit_alloc_resource(test, t_wiphy_init,	\
					     t_wiphy_exit,		\
					     GFP_KERNEL, &(ctx));	\
									\
		KUNIT_ASSERT_NOT_NULL(test, __wiphy);			\
		__wiphy;						\
	})
#define t_wiphy_ctx(wiphy) (((struct t_wiphy_priv *)wiphy_priv(wiphy))->ctx)

int t_wiphy_init(struct kunit_resource *resource, void *data);
void t_wiphy_exit(struct kunit_resource *resource);

#define t_skb_remove_member(skb, type, member)	do {				\
		memmove((skb)->data + (skb)->len - sizeof(type) +		\
			offsetof(type, member),					\
			(skb)->data + (skb)->len - sizeof(type) +		\
			offsetofend(type, member),				\
			offsetofend(type, member));				\
		skb_trim(skb, (skb)->len - sizeof_field(type, member));		\
	} while (0)

/* SMD Test Utils */
/* Common SMD domain IDs for testing */
#define T_SMD_DOMAIN_ID_1	{ 0x11, 0x22, 0x33, 0x44, 0x55, 0x66 }
#define T_SMD_DOMAIN_ID_2	{ 0xaa, 0xbb, 0xcc, 0xdd, 0xee, 0xff }
#define T_SMD_DOMAIN_ID_ZERO	{ 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 }

#define T_SMD_CAP_BASIC		0x01
#define T_SMD_CAP_DL_FORWARD	IEEE80211_SMD_CAP_DL_DATA_FORWARDING
#define T_SMD_CAP_COMBINED	0x03

#define T_SMD_TIMEOUT_1000	1000
#define T_SMD_TIMEOUT_2000	2000
#define T_SMD_TIMEOUT_4096	4096

static inline void t_expect_smd_fields(struct kunit *test,
				       struct cfg80211_bss *bss,
				       bool has_smd,
				       const u8 *expected_identifier,
				       u8 expected_cap,
				       u16 expected_timeout)
{
	KUNIT_EXPECT_EQ(test, bss->has_smd, has_smd);
	if (has_smd && expected_identifier) {
		KUNIT_EXPECT_MEMEQ(test, bss->smd_identifier,
				   expected_identifier, ETH_ALEN);
		KUNIT_EXPECT_EQ(test, bss->smd_capabilities, expected_cap);
		KUNIT_EXPECT_EQ(test, bss->smd_timeout, expected_timeout);
	} else {
		u8 zero_id[ETH_ALEN] = { 0 };

		KUNIT_EXPECT_MEMEQ(test, bss->smd_identifier, zero_id, ETH_ALEN);
		KUNIT_EXPECT_EQ(test, bss->smd_capabilities, 0);
		KUNIT_EXPECT_EQ(test, bss->smd_timeout, 0);
	}
}

static inline struct cfg80211_inform_bss t_smd_inform_bss(void *ctx, int signal)
{
	return (struct cfg80211_inform_bss) {
		.drv_data = ctx,
		.signal = signal,
	};
}

#endif /* __CFG80211_UTILS_H */
