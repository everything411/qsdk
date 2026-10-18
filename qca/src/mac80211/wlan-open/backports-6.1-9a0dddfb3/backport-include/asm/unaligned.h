#ifndef __BACKPORT_ASM_GENERIC_UNALIGNED_H
#define __BACKPORT_ASM_GENERIC_UNALIGNED_H
/* On 6.18 headers, asm/unaligned.h may not exist; fallback to linux/unaligned.h */
#if LINUX_VERSION_IS_LESS(6,8,0)
#if __has_include(<asm/unaligned.h>)
#include_next <asm/unaligned.h>
#endif
#else
#include <linux/unaligned.h>
#endif

#if LINUX_VERSION_IS_LESS(5,7,0)
static inline u32 __get_unaligned_be24(const u8 *p)
{
	return p[0] << 16 | p[1] << 8 | p[2];
}

static inline u32 get_unaligned_be24(const void *p)
{
	return __get_unaligned_be24(p);
}
#endif /* < 5.7 */

#endif /* __BACKPORT_ASM_GENERIC_UNALIGNED_H */
