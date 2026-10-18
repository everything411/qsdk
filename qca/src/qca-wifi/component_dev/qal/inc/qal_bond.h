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
 * QTI driver framework for bond related APIs prototype
 */

#ifndef __QAL_BOND_H
#define __QAL_BOND_H

#include <qdf_types.h>
#include <net/cfg80211.h>
#include <net/bonding.h>

/**
 *
 * qal_bond_create() - Create bond netdevice
 *
 * @net: net structure
 * @name: Bond netdevice name
 * @mlo_info: mlo_bond_info private structure
 *
 * Return: Created bond netdevice
 */
struct net_device *qal_bond_create(struct net *net,
				   char *name, void *mlo_info);

/**
 *
 * qal_bond_create_slave() - Add slave netdevice to bond netdevice
 *
 * @bond_dev: Bond netdevice i.e. master
 * @slave_dev: Slave netdevice which will be enslaved to master
 *
 * Return: 0 for success, -1 for failure
 */
int qal_bond_create_slave(struct net_device *bond_dev,
			  struct net_device *slave_dev);

/**
 *
 * qal_bond_slave_release() - Release slave netdevice from bond netdevice
 *
 * @bond_dev: Bond netdevice i.e. master
 * @slave_dev: Slave netdevice which will be released from master
 *
 * Return: 0 for success, -1 for failure
 */
int qal_bond_slave_release(struct net_device *bond_dev,
			   struct net_device *slave_dev);

/**
 *
 * qal_bond_destroy() - Destroy bond netdevice
 *
 * @bond_dev: Bond netdevice to be destroyed
 *
 * Return: 0 for success, -1 for failure
 */
int qal_bond_destroy(struct net_device *bond_dev);

/**
 *
 * qal_bond_get_mlo_ctx() - Get bond mlo context
 *
 * @bond_dev: Bond netdevice
 *
 * Return: bond mlo context
 */
void *qal_bond_get_mlo_ctx(struct net_device *bond_dev);
#endif /* __QAL_BOND_H */
