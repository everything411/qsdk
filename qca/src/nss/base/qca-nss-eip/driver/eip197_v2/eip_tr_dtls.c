/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: ISC
 */

/*
 * eip_tr_dtls.c
 *	EIP197_v2 stub for DTLS transform operations.
 *
 *	eip197_v2 does not support DTLS offload. These stubs satisfy the
 *	linker requirement from modules that reference the public eip.h API.
 */

#include <linux/module.h>
#include <linux/skbuff.h>
#include <linux/errno.h>

struct eip_tr;
struct eip_svc_entry;

/*
 * eip_tr_dtls_enc()
 *	Not supported on eip197_v2.
 */
int eip_tr_dtls_enc(struct eip_tr *tr, struct sk_buff *skb)
{
	return -ENOTSUPP;
}
EXPORT_SYMBOL(eip_tr_dtls_enc);

/*
 * eip_tr_dtls_dec()
 *	Not supported on eip197_v2.
 */
int eip_tr_dtls_dec(struct eip_tr *tr, struct sk_buff *skb)
{
	return -ENOTSUPP;
}
EXPORT_SYMBOL(eip_tr_dtls_dec);

/*
 * eip_tr_dtls_get_svc()
 *	Not supported on eip197_v2 — return NULL service database.
 */
const struct eip_svc_entry *eip_tr_dtls_get_svc(void)
{
	return NULL;
}
EXPORT_SYMBOL(eip_tr_dtls_get_svc);

/*
 * eip_tr_dtls_get_svc_len()
 *	Not supported on eip197_v2 — return zero.
 */
size_t eip_tr_dtls_get_svc_len(void)
{
	return 0;
}
EXPORT_SYMBOL(eip_tr_dtls_get_svc_len);
