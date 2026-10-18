/*
 * Copyright (c) 2023, Qualcomm Innovation Center, Inc. All rights reserved.
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

/**
 * DOC: qal_bond
 * This file provides OS dependent bond related APIs
 */

#include <qdf_module.h>
#include "qal_bond.h"

#ifdef CONFIG_BOND_MOD_SUPPORT
struct net_device *
qal_bond_create(struct net *net, char *name,
		void *mlo_info)
{
	return bond_create_mlo(net, name, (struct mlo_bond_info *)mlo_info);
}

qdf_export_symbol(qal_bond_create);

int
qal_bond_create_slave(struct net_device *bond_dev,
		      struct net_device *slave_dev)
{
	return bond_enslave(bond_dev, slave_dev, NULL);
}

qdf_export_symbol(qal_bond_create_slave);

int
qal_bond_slave_release(struct net_device *bond_dev,
		       struct net_device *slave_dev)
{
	return bond_release(bond_dev, slave_dev);
}

qdf_export_symbol(qal_bond_slave_release);

int
qal_bond_destroy(struct net_device *bond_dev)
{
	return bond_destroy_mlo(bond_dev);
}

qdf_export_symbol(qal_bond_destroy);

void *
qal_bond_get_mlo_ctx(struct net_device *bond_dev)
{
	return bond_get_mlo_ctx(bond_dev);
}

qdf_export_symbol(qal_bond_get_mlo_ctx);
#else
struct net_device *
qal_bond_create(struct net *net, char *name,
		void *mlo_info)
{
	return NULL;
}

qdf_export_symbol(qal_bond_create);

int
qal_bond_create_slave(struct net_device *bond_dev,
		      struct net_device *slave_dev)
{
	return 0;
}

qdf_export_symbol(qal_bond_create_slave);

int
qal_bond_slave_release(struct net_device *bond_dev,
		       struct net_device *slave_dev)
{
	return 0;
}

qdf_export_symbol(qal_bond_slave_release);

int
qal_bond_destroy(struct net_device *bond_dev)
{
	return 0;
}

qdf_export_symbol(qal_bond_destroy);

void *
qal_bond_get_mlo_ctx(struct net_device *bond_dev)
{
	return NULL;
}

qdf_export_symbol(qal_bond_get_mlo_ctx);
#endif /* CONFIG_BOND_MOD_SUPPORT */
