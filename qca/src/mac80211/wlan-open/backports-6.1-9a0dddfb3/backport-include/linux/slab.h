#ifndef __BACKPORT_SLAB_H
#define __BACKPORT_SLAB_H
#include_next <linux/slab.h>
#include <linux/version.h>

#ifdef CPTCFG_MAC80211_ATHMEMDEBUG
#include <linux/athdebug_slab.h>
#endif

#ifdef CPTCFG_ATHDEBUG
#if !defined(CONFIG_DEBUG_MEM_USAGE)
#if !defined(CPTCFG_MAC80211_ATHMEMDEBUG) && defined(CONFIG_QCA_MINIDUMP)
#include "linux/ath_alloc_if.h"
#endif
#endif
#endif

#if LINUX_VERSION_IS_LESS(5,9,0)
#define kfree_sensitive(x)	kzfree(x)
#endif

#endif /* __BACKPORT_SLAB_H */
