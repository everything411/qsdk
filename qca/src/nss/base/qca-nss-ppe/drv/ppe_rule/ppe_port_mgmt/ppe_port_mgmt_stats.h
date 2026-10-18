/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: ISC
 */

#ifndef __PPE_PORT_MGMT_STATS_H
#define __PPE_PORT_MGMT_STATS_H

#include <linux/atomic.h>
#include <linux/debugfs.h>

/*
 * ppe_port_mgmt_stats
 *	Statistics counters for PPE port management operations.
 *	Counters are grouped by feature and follow the req/success/fail
 *	convention: req is incremented on every API call, success and fail
 *	are incremented based on the outcome.  req == success + fail always.
 */
struct ppe_port_mgmt_stats {
	/*
	 * OMCI-managed port operations.
	 */
	atomic64_t omci_port_add_req;		/**< OMCI port add requests */
	atomic64_t omci_port_add_success;	/**< OMCI port add successes */
	atomic64_t omci_port_add_fail;		/**< OMCI port add failures */
	atomic64_t omci_port_del_req;		/**< OMCI port delete requests */
	atomic64_t omci_port_del_success;	/**< OMCI port delete successes */
	atomic64_t omci_port_del_fail;		/**< OMCI port delete failures */
	atomic64_t omci_port_flush_req;		/**< OMCI port flush requests */
	atomic64_t omci_port_flush_success;	/**< OMCI port flush successes */

	/*
	 * MAC learn-limit operations.
	 * _req/_success/_fail are aggregate totals across all calls.
	 * _en_* counts calls where port_learn_limit_en == true.
	 * _dis_* counts calls where port_learn_limit_en == false.
	 */
	atomic64_t mac_lrn_limit_req;		/**< MAC learn limit set requests (aggregate) */
	atomic64_t mac_lrn_limit_success;	/**< MAC learn limit set successes (aggregate) */
	atomic64_t mac_lrn_limit_fail;		/**< MAC learn limit set failures (aggregate) */
	atomic64_t mac_lrn_limit_en_req;	/**< MAC learn limit enable requests */
	atomic64_t mac_lrn_limit_en_success;	/**< MAC learn limit enable successes */
	atomic64_t mac_lrn_limit_en_fail;	/**< MAC learn limit enable failures */
	atomic64_t mac_lrn_limit_dis_req;	/**< MAC learn limit disable requests */
	atomic64_t mac_lrn_limit_dis_success;	/**< MAC learn limit disable successes */
	atomic64_t mac_lrn_limit_dis_fail;	/**< MAC learn limit disable failures */

	/*
	 * MAC filter (FDB DROP entry) operations.
	 */
	atomic64_t mac_filter_set_req;		/**< MAC filter set requests */
	atomic64_t mac_filter_set_success;	/**< MAC filter set successes */
	atomic64_t mac_filter_set_fail;		/**< MAC filter set failures */
	atomic64_t mac_filter_clr_req;		/**< MAC filter clear requests */
	atomic64_t mac_filter_clr_success;	/**< MAC filter clear successes */
	atomic64_t mac_filter_clr_fail;		/**< MAC filter clear failures */

	/*
	 * Port isolation operations.
	 */
	atomic64_t port_isol_req;		/**< Port isolation set requests */
	atomic64_t port_isol_success;		/**< Port isolation set successes */
	atomic64_t port_isol_fail;		/**< Port isolation set failures */
};

/*
 * ppe_port_mgmt_stats_inc()
 *	Atomically increment a single stats counter.
 */
static inline void ppe_port_mgmt_stats_inc(atomic64_t *stat)
{
	atomic64_inc(stat);
}

/*
 * Debugfs stats APIs.
 */
void ppe_port_mgmt_stats_debugfs_init(struct dentry *d_rule);
void ppe_port_mgmt_stats_debugfs_exit(void);

#endif /* __PPE_PORT_MGMT_STATS_H */
