/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: ISC
 */

#ifndef __NSS_PPENL_PORT_MIRROR_API_H__
#define __NSS_PPENL_PORT_MIRROR_API_H__

#include <nss_ppenl_port_mirror_if.h>

/**
 * nss_ppenl_port_mirror_cfg()
 *	Enable or disable L2 VP mirroring on a named port.
 *
 * @param[in] rule  Pointer to the port-mirror rule message (port_name and
 *                  mirror_en must be filled in by the caller).
 *
 * @return
 * 0 on success or negative errno on failure.
 */
int nss_ppenl_port_mirror_cfg(struct nss_ppenl_port_mirror_rule *rule);

#endif /* __NSS_PPENL_PORT_MIRROR_API_H__ */
