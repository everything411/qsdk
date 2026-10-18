/*
 * Copyright (c) 2025, Qualcomm Innovation Center, Inc. All rights reserved.
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

/*
 * nss_ppenl_exception.h
 *	NSS Netlink EXCEPTION definitions
 */
#ifndef __NSS_PPENL_EXCEPTION_H
#define __NSS_PPENL_EXCEPTION_H

bool nss_ppenl_exception_init(void);
bool nss_ppenl_exception_exit(void);

#if defined(CONFIG_NSS_PPENL_EXCEPTION)
#define NSS_PPENL_EXCEPTION_INIT nss_ppenl_exception_init
#define NSS_PPENL_EXCEPTION_EXIT nss_ppenl_exception_exit
#else
#define NSS_PPENL_EXCEPTION_INIT 0
#define NSS_PPENL_EXCEPTION_EXIT 0
#endif /* !CONFIG_NSS_PPENL_EXCEPTION */

#endif /* __NSS_PPENL_EXCEPTION_H */
