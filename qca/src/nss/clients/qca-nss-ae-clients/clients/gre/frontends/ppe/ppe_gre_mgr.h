/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: ISC
 */

#ifndef __PPE_GRE_MGR_PRIV_H_
#define __PPE_GRE_MGR_PRIV_H_

#include "gre_mgr.h"
#ifdef CONFIG_QCA_MINIDUMP
#include <soc/qcom/ctx-save.h>
#endif

extern int ppe_gre_mgr_init(struct gre_mgr_cmn_ctx *gre_ctx);
extern void ppe_gre_mgr_exit(struct gre_mgr_cmn_ctx *gre_ctx);

/*
 * ppe_gre_mgr_stats
 */
struct ppe_gre_mgr_stats {
	atomic64_t iflag_seq_err;               /* Sequence number enabled in iflag */
	atomic64_t oflag_seq_err;               /* Sequence number enabled in oflag */
	atomic64_t enc_lim_err;                 /* Encap limit not set for v6 tunnel */
	atomic64_t gretun_src_excep_drop_count; /* GRETUN source exception drop counter */
	atomic64_t gretap_src_excep_drop_count; /* GRETAP source exception drop counter */
	atomic64_t gretun_key_flag_failure;     /* GRETUN key flag set failure */
	atomic64_t gretun_csum_flag_failure;    /* GRETUN csum flag set failure */
};

/*
 * ppe_gre_mgr_stats
 * 	Global gre client context.
 */
struct ppe_gre_mgr_ctx {
	struct ppe_gre_mgr_stats stats;		/* GRE client statistics */
	struct dentry *dentry;			/* Root dentry for GRE client */
};

/*
 * ppe_gre_mgr__minidump_log()
 * 	To log data structures into minidump
 */
static inline void ppe_gre_mgr_minidump_log(void *start_addr, uint64_t size, const char *name)
{
#ifdef CONFIG_QCA_MINIDUMP
	if (minidump_add_segments((uint64_t)(uintptr_t)(start_addr), size, QCA_WDT_LOG_DUMP_TYPE_MOD, name, MINIDUMP_CRASH_TYPE_NSS, "qca_nss_ppe_gre") != 0)
		pr_warn("minidump_log failed for structure type %s at address %p\n", name, start_addr);
#endif
}

/*
 * ppe_gre_mgr_minidump_free()
 * 	To unregister data structures from minidump tlv
 */
static inline void ppe_gre_mgr_minidump_free(void *start_addr, const char *name) {
#ifdef CONFIG_QCA_MINIDUMP
	if (minidump_remove_segments((uint64_t)(uintptr_t)(start_addr)) != 0)
		pr_warn("minidump_free failed for structure %s at address %p\n", name, start_addr);
#endif
}

#endif /* __PPE_GRE_MGR_PRIV_H_ */
