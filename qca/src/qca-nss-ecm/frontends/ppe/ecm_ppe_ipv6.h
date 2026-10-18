/*
 **************************************************************************
 * Copyright (c) 2022, 2024 Qualcomm Innovation Center, Inc. All rights reserved.
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
 **************************************************************************
 */

#include <ppe_drv.h>
#include <ppe_drv_v4.h>
#include <ppe_drv_v6.h>
#include <ppe_vp_public.h>

#ifdef CONFIG_NF_CONNTRACK_NPTV6_EXT
#include <net/netfilter/nf_conntrack_nptv6_ext.h>

#define ECM_NPTV6_MAX_ADDR_LEN 128
#endif

extern int ecm_ppe_ipv6_no_action_limit_default;		/* Default no-action limit. */
extern int ecm_ppe_ipv6_driver_fail_limit_default;		/* Default driver fail limit. */
extern int ecm_ppe_ipv6_nack_limit_default;			/* Default nack limit. */
extern int ecm_ppe_ipv6_accelerated_count;			/* Total offloads */
extern int ecm_ppe_ipv6_pending_accel_count;			/* Total pending offloads issued to the PPE / awaiting completion */
extern int ecm_ppe_ipv6_pending_decel_count;			/* Total pending deceleration requests issued to the PPE / awaiting completion */

/*
 * Limiting the acceleration of connections.
 *
 * By default there is no acceleration limiting.
 * This means that when ECM has more connections (that can be accelerated) than the acceleration
 * engine will allow the ECM will continue to try to accelerate.
 * In this scenario the acceleration engine will begin removal of existing rules to make way for new ones.
 * When the accel_limit_mode is set to FIXED ECM will not permit more rules to be issued than the engine will allow.
 */
extern uint32_t ecm_ppe_ipv6_accel_limit_mode;

/*
 * Flag to enable/disable bridge VLAN passthrough feature.
 * By default this feature is disabled.
 */
extern int ecm_ppe_ipv6_vlan_passthrough_enable;

/*
 * Locking of the classifier - concurrency control for file global parameters.
 * NOTE: It is safe to take this lock WHILE HOLDING a feci->lock. The reverse is NOT SAFE.
 */
extern spinlock_t ecm_ppe_ipv6_lock;			/* Protect against SMP access between netfilter, events and private threaded function. */

#ifdef CONFIG_NF_CONNTRACK_NPTV6_EXT
/*
 * ecm_ppe_nptv6_info
 *	Contains the extracted information from the NPTv6 Extension
 */
struct ecm_ppe_nptv6_info {
	uint32_t src_pfx[4];	/* Source Prefix used by the NPTv6 translator for the packet */
	uint32_t dst_pfx[4];	/* Destination Prefix used by the NPTv6 translator for the packet */
	uint16_t nptv6_flags;	/* NPTv6 specfic flags set for the packet */
	uint8_t src_pfx_len;	/* Source Prefix length set by the NPTv6 translator for the packet */
	uint8_t dst_pfx_len;	/* Destination Prefix length set by the NPTv6 translator for the packet */
};

/*
 * ecm_v6_clear_low_bits()
 *	Clears the Lower bits of IPv6 address
 */
static inline void ecm_v6_clear_low_bits(uint16_t len, uint32_t *prefix)
{
	uint8_t i = 0;
	uint8_t ints_to_clear = len / 32;
	uint8_t bits_to_clear = len % 32;

	for (i = 0; i < ints_to_clear; i++) {
		prefix[3 - i] = 0;
	}

	if (bits_to_clear > 0) {
		prefix[3 - ints_to_clear] &= ~((1U << bits_to_clear) - 1);
	}
}

/*
 * ecm_nptv6_info_fill()
 *	Fills the NPTv6 Extension info.
 */
static inline bool ecm_nptv6_info_fill(uint32_t *match_ip, uint32_t *xlate_ip, uint32_t *ret_match,
				uint32_t *ret_match_xlate, struct nf_ct_nptv6_ext *nptcte,
				bool is_outbound, struct ecm_ppe_nptv6_info *npt)
{
	uint16_t src_clear_len = ECM_NPTV6_MAX_ADDR_LEN - nptcte->src_pfx_len;
	uint16_t dst_clear_len = ECM_NPTV6_MAX_ADDR_LEN - nptcte->dst_pfx_len;
	uint32_t src_pfx[4], dst_pfx[4];

	/*
	 * Verify whether the src/dst Prefix programed by NFTABLES
	 * is matching the IPv6 addresses obtained from ECM's database
	 */
	ECM_IP_ADDR_COPY(src_pfx, match_ip);
	ECM_IP_ADDR_COPY(dst_pfx, xlate_ip);
	ecm_v6_clear_low_bits(src_clear_len, src_pfx);
	ecm_v6_clear_low_bits(dst_clear_len, dst_pfx);
	if ((ECM_IP_ADDR_MATCH(nptcte->src_pfx, src_pfx))
		&& (ECM_IP_ADDR_MATCH(nptcte->dst_pfx, dst_pfx))) {
		npt->src_pfx_len = nptcte->src_pfx_len;
		npt->dst_pfx_len = nptcte->dst_pfx_len;
		npt->nptv6_flags = nptcte->nptv6_flags;
		ECM_IP_ADDR_COPY(npt->src_pfx, nptcte->src_pfx);
		ECM_IP_ADDR_COPY(npt->dst_pfx, nptcte->dst_pfx);
		return true;
	}

	/*
	 * for bi-di flows, ECM pushes rule in one direction,
	 * while conntrack extension points to other direction.
	 */
	ECM_IP_ADDR_COPY(src_pfx, ret_match_xlate);
	ECM_IP_ADDR_COPY(dst_pfx, ret_match);
	ecm_v6_clear_low_bits(src_clear_len, src_pfx);
	ecm_v6_clear_low_bits(dst_clear_len, dst_pfx);
	if ((ECM_IP_ADDR_MATCH(nptcte->src_pfx, src_pfx))
		&& (ECM_IP_ADDR_MATCH(nptcte->dst_pfx, dst_pfx))) {
		npt->src_pfx_len = nptcte->dst_pfx_len;
		npt->dst_pfx_len = nptcte->src_pfx_len;
		ECM_IP_ADDR_COPY(npt->dst_pfx, nptcte->src_pfx);
		ECM_IP_ADDR_COPY(npt->src_pfx, nptcte->dst_pfx);
		npt->nptv6_flags = is_outbound ? NF_CT_NPTV6_EXT_DNPT: NF_CT_NPTV6_EXT_SNPT;
		return true;
	}

	return false;
}

bool ecm_ppe_nptv6_validate_pkt(struct sk_buff *skb, uint32_t *flow_ip, uint32_t *flow_ip_xlate,
				uint32_t *return_ip, uint32_t *return_ip_xlate, struct ecm_ppe_nptv6_info *npt);
#endif

/*
 * ecm_ppe_ipv6_accel_pending_set()
 *	Set pending acceleration for the connection object.
 *
 * Return false if the acceleration is not permitted or is already in progress.
 */
static inline bool ecm_ppe_ipv6_accel_pending_set(struct ecm_front_end_connection_instance *feci)
{
	DEBUG_INFO("%px: Accel conn: %px\n", feci, feci->ci);

	/*
	 * If re-generation is required then we cannot permit acceleration
	 */
	if (ecm_db_connection_regeneration_required_peek(feci->ci)) {
		DEBUG_TRACE("%px: accel %px failed - regen required\n", feci, feci->ci);
		return false;
	}

	/*
	 * Is connection acceleration permanently failed?
	 */
	spin_lock_bh(&feci->lock);
	if (ECM_FRONT_END_ACCELERATION_FAILED(feci->accel_mode)) {
		spin_unlock_bh(&feci->lock);
		DEBUG_TRACE("%px: accel %px failed\n", feci, feci->ci);
		return false;
	}

	/*
	 * If acceleration mode is anything other than "not accelerated" then ignore.
	 */
	if (feci->accel_mode != ECM_FRONT_END_ACCELERATION_MODE_DECEL) {
		spin_unlock_bh(&feci->lock);
		DEBUG_TRACE("%px: Ignoring wrong mode accel for conn: %px\n", feci, feci->ci);
		return false;
	}

	/*
	 * Do we have a fixed upper limit for acceleration?
	 */
	spin_lock_bh(&ecm_ppe_ipv6_lock);
	if (ecm_ppe_ipv6_accel_limit_mode & ECM_FRONT_END_ACCEL_LIMIT_MODE_FIXED) {
		if ((ecm_ppe_ipv6_pending_accel_count + ecm_ppe_ipv6_accelerated_count) >= PPE_DRV_V6_MAX_CONN_COUNT) {
			spin_unlock_bh(&ecm_ppe_ipv6_lock);
			spin_unlock_bh(&feci->lock);
			DEBUG_INFO("%px: Accel limit reached, accel denied: %px\n", feci, feci->ci);
			return false;
		}
	}

	/*
	 * Okay to accelerate
	 */
	ecm_ppe_ipv6_pending_accel_count++;
	spin_unlock_bh(&ecm_ppe_ipv6_lock);

	/*
	 * Okay connection can be set to pending acceleration
	 */
	feci->accel_mode = ECM_FRONT_END_ACCELERATION_MODE_ACCEL_PENDING;
	spin_unlock_bh(&feci->lock);
	return true;
}

/*
 * _ecm_ppe_ipv6_accel_pending_clear()
 *	Clear pending acceleration for the connection object, setting it to the desired state.
 *
 * Returns true if "decelerate was pending".
 *
 * The feci->lock AND ecm_ppe_ipv6_lock must be held on entry.
 */
static inline bool _ecm_ppe_ipv6_accel_pending_clear(struct ecm_front_end_connection_instance *feci, ecm_front_end_acceleration_mode_t mode)
{
	bool decel_pending;

	/*
	 * Set the mode away from its accel pending state.
	 */
	DEBUG_ASSERT(feci->accel_mode == ECM_FRONT_END_ACCELERATION_MODE_ACCEL_PENDING, "%px: Accel mode unexpected: %d\n", feci, feci->accel_mode);
	feci->accel_mode = mode;

	/*
	 * Clear decelerate pending flag.
	 * This flag is only set when we are ACCEL_PENDING -
	 * and we are moving from that to the given mode anyway.
	 */
	decel_pending = feci->stats.decelerate_pending;
	feci->stats.decelerate_pending = false;

	/*
	 * Decrement pending counter
	 */
	ecm_ppe_ipv6_pending_accel_count--;
	DEBUG_ASSERT(ecm_ppe_ipv6_pending_accel_count >= 0, "Accel pending underflow\n");
	return decel_pending;
}

/*
 * ecm_ppe_ipv6_accel_pending_clear()
 *	Clear pending acceleration for the connection object, setting it to the desired state.
 */
static inline bool ecm_ppe_ipv6_accel_pending_clear(struct ecm_front_end_connection_instance *feci, ecm_front_end_acceleration_mode_t mode)
{
	bool decel_pending;
	spin_lock_bh(&feci->lock);
	spin_lock_bh(&ecm_ppe_ipv6_lock);
	decel_pending = _ecm_ppe_ipv6_accel_pending_clear(feci, mode);
	spin_unlock_bh(&ecm_ppe_ipv6_lock);
	spin_unlock_bh(&feci->lock);
	return decel_pending;
}

extern void ecm_ppe_ipv6_accel_done_time_update(struct ecm_front_end_connection_instance *feci);
extern void ecm_ppe_ipv6_decel_done_time_update(struct ecm_front_end_connection_instance *feci);
extern int ecm_ppe_ipv6_init(struct dentry *dentry);
extern void ecm_ppe_ipv6_exit(void);
