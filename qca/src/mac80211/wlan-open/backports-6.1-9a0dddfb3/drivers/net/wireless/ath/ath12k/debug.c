// SPDX-License-Identifier: BSD-3-Clause-Clear
/*
 * Copyright (c) 2018-2021 The Linux Foundation. All rights reserved.
 * Copyright (c) 2021-2025 Qualcomm Innovation Center, Inc. All rights reserved.
 */

#include <linux/vmalloc.h>
#include "core.h"
#include "debug.h"
#ifdef CPTCFG_ATHDEBUG_UIO_LOGGING
#include "ath_debug/athdbg_uio.h"
#endif

void ath12k_info(struct ath12k_base *ab, const char *fmt, ...)
{
	struct va_format vaf = {
		.fmt = fmt,
	};
	va_list args;

	va_start(args, fmt);
	vaf.va = &args;

	if (likely(ab))
		dev_info(ab->dev, "%pV", &vaf);
	else
		pr_info("ath12k: %pV", &vaf);

	va_end(args);
}
EXPORT_SYMBOL(ath12k_info);

void ath12k_err(struct ath12k_base *ab, const char *fmt, ...)
{
	struct va_format vaf = {
		.fmt = fmt,
	};
	va_list args;

	va_start(args, fmt);
	vaf.va = &args;

	if (likely(ab))
		dev_err(ab->dev, "%pV", &vaf);
	else
		pr_err("ath12k: %pV", &vaf);

	va_end(args);
}
EXPORT_SYMBOL(ath12k_err);

void __ath12k_warn(struct device *dev, const char *fmt, ...)
{
	struct va_format vaf = {
		.fmt = fmt,
	};
	va_list args;

	va_start(args, fmt);
	vaf.va = &args;

	dev_warn_ratelimited(dev, "%pV", &vaf);

	va_end(args);
}
EXPORT_SYMBOL(__ath12k_warn);

#ifdef CPTCFG_ATH12K_DEBUG

void __ath12k_dbg(struct ath12k_base *ab, u64 mask,
		  const char *fmt, ...)
{
	struct va_format vaf;
	va_list args;

	va_start(args, fmt);
	vaf.fmt = fmt;
	vaf.va = &args;

	if (mask & ath12k_debug_mask) {
		if (ab)
#if defined(CPTCFG_ATH12K_MEM_PROFILE_256M)
			/* dev_dbg() is a no-op in 256M profile since CONFIG_DYNAMIC_DEBUG
			 * is disabled to save memory. Use dev_info() instead
			 * to ensure debug messages are always emitted.
			 */
			dev_printk(KERN_DEBUG, ab->dev, "%pV", &vaf);
#else
			dev_dbg(ab->dev, "%pV", &vaf);
#endif
		else
			pr_info("ath12k: %pV", &vaf);
	}
	va_end(args);
}
EXPORT_SYMBOL(__ath12k_dbg);

void ath12k_dbg_dump(struct ath12k_base *ab,
		     u64 mask,
		     const char *msg, const char *prefix,
		     const void *buf, size_t len)
{
	char linebuf[256];
	size_t linebuflen;
	const void *ptr;

	if (ath12k_debug_mask & mask) {
		if (msg)
			__ath12k_dbg(ab, mask, "%s\n", msg);

		for (ptr = buf; (ptr - buf) < len; ptr += 16) {
			linebuflen = 0;
			linebuflen += scnprintf(linebuf + linebuflen,
						sizeof(linebuf) - linebuflen,
						"%s%08x: ",
						(prefix ? prefix : ""),
						(unsigned int)(ptr - buf));
			hex_dump_to_buffer(ptr, len - (ptr - buf), 16, 1,
					   linebuf + linebuflen,
					   sizeof(linebuf) - linebuflen, true);
#if defined(CPTCFG_ATH12K_MEM_PROFILE_256M)
			dev_printk(KERN_DEBUG, ab->dev, "%s\n", linebuf);
#else
			dev_dbg(ab->dev, "%s\n", linebuf);
#endif
		}
	}
}
EXPORT_SYMBOL(ath12k_dbg_dump);

#endif /* CPTCFG_ATH12K_DEBUG */

