/*
 * netfn_capwap.h
 *	Network function's CAPWAP offload public APIs.
 *
 * Copyright (c) 2023-2024 Qualcomm Innovation Center, Inc. All rights reserved.
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
#ifndef __NETFN_CAPWAP_H
#define __NETFN_CAPWAP_H

#include "netfn_capwap_types.h"

/*
 * netfn_capwap_tun_alloc()
 *	Adds a new tunnel.
 * @cfg: Tunnel configuration.
 * @return: Net device associated with tunnel.
 */
struct net_device *netfn_capwap_tun_alloc(struct netfn_capwap_tun_cfg *cfg, struct netfn_tuple *tuple, ssize_t pvt_sz);

/*
 * netfn_capwap_tun_free()
 *	Deletes the tunnel.
 * @dev: Tunnel netdevice.
 * @return: bool
 */
bool netfn_capwap_tun_free(struct net_device *dev);

/*
 * netfn_capwap_tun_bind()
 *	Binds the 2 netdevices.
 * @dev: dev to binded.
 * @next: dev to bind it to.
 * @return: bool
 */
bool netfn_capwap_tun_bind(struct net_device *dev, struct net_device *next);

/*
 * netfn_capwap_tun_unbind()
 *	Unbinds the tunnel.
 * @dev: dev to unbinded.
 * @return: bool
 */
bool netfn_capwap_tun_unbind(struct net_device *dev);

/*
 * netfn_capwap_tun_stats_get()
 *	Gets the tunnel stats.
 * @dev: Tunnel netdevice.
 * @stats: Tunnel stats object.
 * @return: int
 */
int netfn_capwap_tun_stats_get(struct net_device *dev, struct netfn_capwap_tun_stats *stats);

/*
 * netfn_capwap_tun_pvt_get()
 *	Returns pointer to pvt area
 * @dev: Netdevice.
 * @return: Pointer to private area.
 */
void *netfn_capwap_tun_pvt_get(struct net_device *dev);

/*
 * netfn_capwap_tunid_alloc()
 *	Allocates a new dummy dev.
 * @void:
 * @return: Net device.
 */
struct net_device *netfn_capwap_tunid_alloc(ssize_t pvt_sz);

/*
 * netfn_capwap_tunid_free()
 *	Frees the dummy dev.
 * @dev: Dev to be freed.
 * @return: bool.
 */
bool netfn_capwap_tunid_free(struct net_device *dev);

/*
 * netfn_capwap_tunid_add()
 *	Adds a new tunnel with a specific id.
 * @dev: Dummy netdev incorporating multiple IDs.
 * @id: ID for tunnel configuration.
 * @cfg: Tunnel configuration.
 * @return: bool.
 */
struct net_device *netfn_capwap_tunid_add(struct net_device *dev, uint8_t id, struct netfn_capwap_tun_cfg *cfg, struct netfn_tuple *tuple, ssize_t pvt_sz);

/*
 * netfn_capwap_tunid_del()
 *	Deletes the tunnel.
 * @dev: Dummy netdev incorporating multiple tun IDs.
 * @id: ID for tunnel configuration.
 * @return: bool
 */
bool netfn_capwap_tunid_del(struct net_device *dev, uint8_t id);

/*
 * netfn_capwap_tunid_bind()
 *	Binds the 2 netdevices.
 * @dev: Dummy netdev hosting multiple netdevs.
 * @id: ID for tunnel configuration.
 * @next: Next dev to be set for tunnel dev.
 * @return: bool
 */
bool netfn_capwap_tunid_bind(struct net_device *dev, uint8_t id, struct net_device *next);

/*
 * netfn_capwap_tunid_unbind()
 *	Unbinds the tunnel id dev.
 * @dev: Dummy netdev hosting multiple netdevs.
 * @id: ID for tunnel configuration.
 * @return: bool
 */
bool netfn_capwap_tunid_unbind(struct net_device *dev, uint8_t id);

/*
 * netfn_capwap_tunid_stats()
 *	Gets the tunnel stats.
 * @dev: Parent netdevice.
 * @id: ID for tunnel configuration.
 * @stats: Tunnel stats object.
 */
int netfn_capwap_tunid_stats_get(struct net_device *dev, uint8_t id, struct netfn_capwap_tun_stats *stats);

/*
 * netfn_capwap_tunid_pvt_get()
 *	Returns pointer to pvt area
 * @dev: Netdevice.
 * @return: Pointer to private area.
 */
void *netfn_capwap_tunid_pvt_get(struct net_device *dev);

/*
 * netfn_capwap_tun_enable_flow_db()
 *	Enables the the Flow Cookie Lookup functionality.
 *
 * @dev: Netdevice.
 * @db: Netfn Flow Cookie DB.
 * @return: void.
 */
void netfn_capwap_tun_enable_flow_db(struct net_device *dev, struct netfn_flow_cookie_db *db);

/*
 * netfn_capwap_tun_disable_flow_db()
 *	Disables the the Flow Cookie Lookup functionality.
 *
 * @dev: Netdevice.
 * @return: void.
 */
void netfn_capwap_tun_disable_flow_db(struct net_device *dev);

/*
 * netfn_capwap_tun_config_dtls()
 *	Enable/disable DTLS feature
 *
 * @dev: Netdevice
 * @bool: DTLS enabled/disabled
 * @return: void
 */
void netfn_capwap_tun_config_dtls(struct net_device *dev, bool enable);
#endif /* __NETFN_CAPWAP_H */
