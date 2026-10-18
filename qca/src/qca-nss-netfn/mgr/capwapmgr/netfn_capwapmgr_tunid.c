/*
 * netfn_capwapmgr_tunid.c
 *	Network function's CAPWAP manager's tunnel-ID configuration.
 *
 * Copyright (c) 2024 Qualcomm Innovation Center, Inc. All rights reserved.
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

#include <linux/debugfs.h>
#include <linux/netdevice.h>
#include <linux/rtnetlink.h>

#include <netfn_capwapmgr.h>
#include "netfn_capwapmgr_priv.h"
#include "netfn_capwapmgr_tun.h"
#include "netfn_capwapmgr_tunid.h"

/*
 * netfn_capwapmgr_tunid_dev_alloc()
 *	Allocate a CAPWAP tunid dev.
 */
struct net_device *netfn_capwapmgr_tunid_dev_alloc(void)
{
	struct netfn_capwapmgr_tunid_ctx *ctx = NULL;
	struct netfn_capwapmgr *mgr = &g_mgr;
	struct net_device *dev;

	dev = netfn_capwap_tunid_alloc(sizeof(struct netfn_capwapmgr_tunid_ctx));
	if (!dev) {
		netfn_capwapmgr_warn("Failed to allocate tunid dev\n");
		return NULL;
	}

	/*
	 * dev_put: netfn_capwapmgr_tunid_free.
	 */
	dev_hold(dev);
	ctx = netfn_capwap_tunid_pvt_get(dev);

	bitmap_zero(ctx->active_tunnels, NETFN_CAPWAPMGR_TUNNELS_MAX);
	memset(ctx->tunnels, 0, sizeof(ctx->tunnels));

	/*
	 * deref: netfn_capwapmgr_tunid_free.
	 */
	ctx->mgr = netfn_capwapmgr_ref(mgr);
	atomic_inc(&mgr->stats.tunid_dev_alloc);
	return dev;
}
EXPORT_SYMBOL(netfn_capwapmgr_tunid_dev_alloc);

/*
 * netfn_capwapmgr_tunid_dev_free()
 *	Free CAPWAP tunid dev.
 */
netfn_capwapmgr_ret_t netfn_capwapmgr_tunid_dev_free(struct net_device *dev)
{
	struct netfn_capwapmgr_tunid_ctx *ctx = NULL;
	struct netfn_capwapmgr *mgr = &g_mgr;

	mutex_lock(&mgr->lock);
	ctx = netfn_capwap_tunid_pvt_get(dev);
	if (!bitmap_empty(ctx->active_tunnels, NETFN_CAPWAPMGR_TUNNELS_MAX)) {
		mutex_unlock(&mgr->lock);
		netfn_capwapmgr_warn("%p: Tun-ID still contains active tunnels\n", dev);
		atomic64_inc(&g_mgr.stats.error_stats[NETFN_CAPWAPMGR_ERROR_TUNID_ACTIVE]);
		return NETFN_CAPWAPMGR_ERROR_TUNID_ACTIVE;
	}

	/*
	 * hold:netfn_capwapmgr_tunid_alloc()
	 */
	dev_put(dev);
	if (!netfn_capwap_tunid_free(dev)) {
		dev_hold(dev);
		mutex_unlock(&mgr->lock);
		atomic64_inc(&g_mgr.stats.error_stats[NETFN_CAPWAPMGR_ERROR_TUNID_FREE]);
		netfn_capwapmgr_warn("%px: Failed to free tunid dev\n", dev);
		return NETFN_CAPWAPMGR_ERROR_TUNID_FREE;
	}

	/*
	 * ref:netfn_capwapmgr_tunid_alloc().
	 */
	netfn_capwapmgr_deref(mgr);
	mutex_unlock(&mgr->lock);
	atomic_inc(&mgr->stats.tunid_dev_free);
	return NETFN_CAPWAPMGR_SUCCESS;
}
EXPORT_SYMBOL(netfn_capwapmgr_tunid_dev_free);

/*
 * netfn_capwapmgr_tunid_toggle_state
 *	Enable/disable a tunid tunnel.
 */
netfn_capwapmgr_ret_t netfn_capwapmgr_tunid_toggle_state(struct net_device *dev, uint8_t tun_id, bool enable)
{
	struct netfn_capwapmgr_tun_ctx *tunid_ctx = NULL;
	struct netfn_capwapmgr_tunid_ctx *ctx = NULL;
	struct netfn_capwapmgr *mgr = &g_mgr;
	struct net_device *tunid_dev = NULL;
	long bit_pos;

	/*
	 * Check for out of bound tunnel id.
	 */
	if(tun_id >= NETFN_CAPWAPMGR_TUNNELS_MAX) {
		netfn_capwapmgr_warn("%p: Tunnel ID %d out of bound\n", dev, tun_id);
		atomic64_inc(&g_mgr.stats.error_stats[NETFN_CAPWAPMGR_ERROR_TUNID_OUT_OF_RANGE]);
		return NETFN_CAPWAPMGR_ERROR_TUNID_OUT_OF_RANGE;
	}

	/*
	 * Get tunid dev context from offload engine.
	 */
	mutex_lock(&mgr->lock);
	ctx = netfn_capwap_tunid_pvt_get(dev);
	if (!test_bit(tun_id, ctx->active_tunnels)) {
		mutex_unlock(&mgr->lock);
		netfn_capwapmgr_warn("%px: Tunnel Id %d Does not exist\n", dev, tun_id);
		atomic64_inc(&g_mgr.stats.error_stats[NETFN_CAPWAPMGR_ERROR_TUNID_INACTIVE]);
		return NETFN_CAPWAPMGR_ERROR_TUNID_INACTIVE;
	}

	tunid_ctx = ctx->tunnels[tun_id];
	tunid_dev = tunid_ctx->capwap_dev;

	bit_pos = __builtin_ffs(NETFN_CAPWAPMGR_TUN_STATE_ENABLED) - 1;

	rtnl_lock();
	if (enable) {
		dev_open(tunid_dev, NULL);
		__set_bit(bit_pos, tunid_ctx->state);
	} else {
		dev_close(tunid_dev);
		__clear_bit(bit_pos, tunid_ctx->state);
	}

	rtnl_unlock();
	mutex_unlock(&mgr->lock);
	return NETFN_CAPWAPMGR_SUCCESS;
}
EXPORT_SYMBOL(netfn_capwapmgr_tunid_toggle_state);

/*
 * netfn_capwapmgr_tunid_del()
 *	Deletes the tunnel with a specific ID under the tunid dev.
 *
 */
netfn_capwapmgr_ret_t netfn_capwapmgr_tunid_del(struct net_device *dev, uint8_t tun_id)
{
	netfn_capwapmgr_ret_t status = NETFN_CAPWAPMGR_SUCCESS;
	struct netfn_capwapmgr_tun_ctx *tunid_ctx = NULL;
	struct netfn_capwapmgr_tunid_ctx *ctx = NULL;
	struct netfn_capwapmgr *mgr = &g_mgr;
	struct net_device *tunid_dev = NULL;

	/*
	 * Check for out of bound tunnel id.
	 */
	if(tun_id >= NETFN_CAPWAPMGR_TUNNELS_MAX) {
		netfn_capwapmgr_warn("%p: Tunnel ID %d out of bound\n", dev, tun_id);
		atomic64_inc(&g_mgr.stats.error_stats[NETFN_CAPWAPMGR_ERROR_TUNID_OUT_OF_RANGE]);
		return NETFN_CAPWAPMGR_ERROR_TUNID_OUT_OF_RANGE;
	}

	/*
	 * Get tunid dev context from offload engine.
	 */
	mutex_lock(&mgr->lock);
	ctx = netfn_capwap_tunid_pvt_get(dev);
	if (!__test_and_clear_bit(tun_id, ctx->active_tunnels)) {
		mutex_unlock(&mgr->lock);
		netfn_capwapmgr_warn("%px: Tunnel Id %d Does not exist\n", dev, tun_id);
		atomic64_inc(&g_mgr.stats.error_stats[NETFN_CAPWAPMGR_ERROR_TUNID_INACTIVE]);
		return NETFN_CAPWAPMGR_ERROR_TUNID_INACTIVE;
	}

	tunid_ctx = ctx->tunnels[tun_id];
	tunid_dev = tunid_ctx->capwap_dev;

	/*
	 * De-initialize the tunnel.
	 */
	if (!netfn_capwapmgr_tun_deinit(tunid_ctx)) {
		mutex_unlock(&mgr->lock);
		netfn_capwapmgr_warn("%px: Tunnel Id %d deinit failed\n", dev, tun_id);
		status = NETFN_CAPWAPMGR_ERROR_TUN_DEINIT;
		goto fail;
	}

	/*
	 * dev_hold:netfn_capwapmgr_tunid_add()
	 */
	dev_put(tunid_dev);

	/*
	 * Delete capwap tunnel.
	 */
	if (!netfn_capwap_tunid_del(dev, tun_id)) {
		dev_hold(tunid_dev);
		mutex_unlock(&mgr->lock);
		netfn_capwapmgr_warn("%px: Failed to free capwap tun \n", dev);
		status = NETFN_CAPWAPMGR_ERROR_TUNID_DEL;
		goto fail;
	}

	/*
	 * Clear the tunnel context
	 */
	ctx->tunnels[tun_id] = NULL;
	mutex_unlock(&mgr->lock);
	return NETFN_CAPWAPMGR_SUCCESS;
fail:
	__set_bit(tun_id, ctx->active_tunnels);
	atomic64_inc(&g_mgr.stats.error_stats[status]);
	return status;
}
EXPORT_SYMBOL(netfn_capwapmgr_tunid_del);

/*
 * netfn_capwapmgr_tunid_add()
 *	Adds a new tunnel with specific ID under the tunid dev.
 */
netfn_capwapmgr_ret_t netfn_capwapmgr_tunid_add(struct net_device *dev, uint8_t tun_id, struct netfn_capwapmgr_tun_cfg *cfg)
{

	netfn_capwapmgr_ret_t status = NETFN_CAPWAPMGR_SUCCESS;
	struct netfn_capwapmgr_tun_ctx *tunid_ctx = NULL;
	struct netfn_capwapmgr_tunid_ctx *ctx = NULL;
	struct netfn_capwapmgr *mgr = &g_mgr;
	struct net_device *tunid_dev = NULL;
	bool tun_init_ret = false;

	/*
	 * Check for out of bound tunnel id.
	 */
	if(tun_id >= NETFN_CAPWAPMGR_TUNNELS_MAX) {
		netfn_capwapmgr_warn("%p: Tunnel ID %d out of bound\n", dev, tun_id);
		atomic64_inc(&g_mgr.stats.error_stats[NETFN_CAPWAPMGR_ERROR_TUNID_OUT_OF_RANGE]);
		return NETFN_CAPWAPMGR_ERROR_TUNID_OUT_OF_RANGE;
	}

	/*
	 * Validate and initialize the configuration.
	 */
	status = netfn_capwapmgr_tun_init_cfg(cfg);
	if (status != NETFN_CAPWAPMGR_SUCCESS) {
		netfn_capwapmgr_warn("%px: Tunnel Id %d Failed to validate tunnel config\n", dev, tun_id);
		return status;
	}

	mutex_lock(&mgr->lock);
	tunid_dev = netfn_capwap_tunid_add(dev, tun_id, &cfg->capwap, &cfg->tuple, sizeof(struct netfn_capwapmgr_tun_ctx));
	if (!tunid_dev) {
		mutex_unlock(&mgr->lock);
		netfn_capwapmgr_warn("%px: Tunnel Id %d add failed\n", dev, tun_id);
		status = NETFN_CAPWAPMGR_ERROR_TUNID_ADD;
		goto fail;
	}

	tunid_ctx = netfn_capwap_tun_pvt_get(tunid_dev);
	tunid_ctx->capwap_dev = tunid_dev;

	/*
	 * Initialize the tunnel.
	 */
	tun_init_ret = netfn_capwapmgr_tun_init(tunid_ctx, cfg);
	if (!tun_init_ret) {
		mutex_unlock(&mgr->lock);
		netfn_capwapmgr_warn("%px:Failed to initialize the capwap tunnel\n", cfg);
		status = NETFN_CAPWAPMGR_ERROR_TUN_INIT;
		goto fail;
	}

	/*
	 * Get tunid dev context from offload engine.
	 */
	ctx = netfn_capwap_tunid_pvt_get(dev);
	if (__test_and_set_bit(tun_id, ctx->active_tunnels)) {
		mutex_unlock(&mgr->lock);
		netfn_capwapmgr_warn("%px: Tunnel ID %d unavailable\n", dev, tun_id);
		status = NETFN_CAPWAPMGR_ERROR_TUNID_INACTIVE;
		goto fail;
	}

	/*
	 * dev_put:netfn_capwapmgr_tunid_del()
	 */
	dev_hold(tunid_dev);

	ctx->tunnels[tun_id] = tunid_ctx;
	mutex_unlock(&mgr->lock);
	return NETFN_CAPWAPMGR_SUCCESS;

fail:
	if (tun_init_ret) {
		netfn_capwapmgr_tun_deinit(tunid_ctx);
	}

	if (tunid_dev) {
		netfn_capwap_tunid_del(dev, tun_id);
	}

	atomic64_inc(&g_mgr.stats.error_stats[status]);
	return status;
}
EXPORT_SYMBOL(netfn_capwapmgr_tunid_add);

/*
 * netfn_capwapmgr_tunid_get_dtls_dev()
 *	Get DTLS dev associated with the tunnel
 *
 * A reference is held on the returned DTLS dev.
 */
struct net_device *netfn_capwapmgr_tunid_get_dtls_dev(struct net_device *dev, uint8_t tun_id)
{
	struct netfn_capwapmgr_tun_ctx *tunid_ctx = NULL;
	struct netfn_capwapmgr_tunid_ctx *ctx = NULL;
	struct netfn_capwapmgr *mgr = &g_mgr;
	struct net_device *dtls_dev = NULL;

	/*
	 * Check for out of bound tunnel id.
	 */
	if(tun_id >= NETFN_CAPWAPMGR_TUNNELS_MAX) {
		netfn_capwapmgr_warn("%p: Tunnel ID %d out of bound\n", dev, tun_id);
		atomic64_inc(&g_mgr.stats.error_stats[NETFN_CAPWAPMGR_ERROR_TUNID_OUT_OF_RANGE]);
		return dtls_dev;
	}

	mutex_lock(&mgr->lock);	
	ctx = netfn_capwap_tunid_pvt_get(dev);

	tunid_ctx = ctx->tunnels[tun_id];
	if (test_bit(tun_id, ctx->active_tunnels)) {
		dtls_dev = netfn_capwapmgr_tun_get_dtls_dev(tunid_ctx->capwap_dev);
		dev_hold(dtls_dev);
	}

	mutex_unlock(&mgr->lock);
	return dtls_dev;
}
EXPORT_SYMBOL(netfn_capwapmgr_tunid_get_dtls_dev);

/*
 * netfn_capwapmgr_tunid_update()
 *	Update tunnel configuration
 *
 */
netfn_capwapmgr_ret_t netfn_capwapmgr_tunid_update(struct net_device *dev, uint8_t tun_id, struct netfn_capwapmgr_tun_update *cfg)
{
	netfn_capwapmgr_ret_t ret = NETFN_CAPWAPMGR_ERROR_MAX;
	struct netfn_capwapmgr_tun_ctx *tunid_ctx = NULL;
	struct netfn_capwapmgr_tunid_ctx *ctx = NULL;
	struct netfn_capwapmgr *mgr = &g_mgr;

	/*
	 * Check for out of bound tunnel id.
	 */
	mutex_lock(&mgr->lock);
	if(tun_id >= NETFN_CAPWAPMGR_TUNNELS_MAX) {
		mutex_unlock(&mgr->lock);
		netfn_capwapmgr_warn("%p: Tunnel ID %d out of bound\n", dev, tun_id);
		atomic64_inc(&g_mgr.stats.error_stats[NETFN_CAPWAPMGR_ERROR_TUNID_OUT_OF_RANGE]);
		return NETFN_CAPWAPMGR_ERROR_TUNID_OUT_OF_RANGE;
	}

	ctx = netfn_capwap_tunid_pvt_get(dev);

	tunid_ctx = ctx->tunnels[tun_id];
	if (test_bit(tun_id, ctx->active_tunnels)) {
		ret = netfn_capwapmgr_tun_update(tunid_ctx->capwap_dev, cfg);
	}

	mutex_unlock(&mgr->lock);
	return ret;
}
EXPORT_SYMBOL(netfn_capwapmgr_tunid_update);

/*
 * netfn_capwapmgr_tunid_get_stats()
 *	Get stats associated with the tunnel ID
 */
netfn_capwapmgr_ret_t netfn_capwapmgr_tunid_get_stats(struct net_device *dev, uint8_t tun_id, struct netfn_capwapmgr_tun_stats *stats)
{
	netfn_capwapmgr_ret_t status = NETFN_CAPWAPMGR_ERROR_MAX;
	struct netfn_capwapmgr_tun_ctx *tunid_ctx = NULL;
	struct netfn_capwapmgr *mgr = &g_mgr;
	struct netfn_capwapmgr_tunid_ctx *ctx = NULL;

	/*
	 * Check for out of bound tunnel id.
	 */
	mutex_lock(&mgr->lock);
	if(tun_id >= NETFN_CAPWAPMGR_TUNNELS_MAX) {
		mutex_unlock(&mgr->lock);
		netfn_capwapmgr_warn("%p: Tunnel ID %d out of bound\n", dev, tun_id);
		atomic64_inc(&g_mgr.stats.error_stats[NETFN_CAPWAPMGR_ERROR_TUNID_OUT_OF_RANGE]);
		return NETFN_CAPWAPMGR_ERROR_TUNID_OUT_OF_RANGE;
	}

	ctx = netfn_capwap_tunid_pvt_get(dev);

	tunid_ctx = ctx->tunnels[tun_id];
	if (test_bit(tun_id, ctx->active_tunnels)) {
		status = netfn_capwapmgr_tun_get_stats(tunid_ctx->capwap_dev, stats);
	}

	mutex_unlock(&mgr->lock);
	return status;
}
EXPORT_SYMBOL(netfn_capwapmgr_tunid_get_stats);
