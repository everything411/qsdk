/*
 **************************************************************************
 * Copyright (c) 2022, Qualcomm Innovation Center, Inc. All rights reserved.
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
 **************************************************************************
 */

#include <linux/netdevice.h>

/*
 * nsm_lat_debug_get()
 *	Gets latency debug stats from the netdevice with the given name.
 */
bool nsm_lat_debug_get(char *netdev_name, uint8_t *sid, uint32_t *max, uint32_t *min, uint32_t *avg, uint32_t *last)
{
	struct net_device *dev = dev_get_by_name(&init_net, netdev_name);
	bool ret;
	if (!dev) {
		return false;
	}

	ret = netdev_sawf_debug_get(dev, sid, max, min, avg, last);
	dev_put(dev);
	return ret;
}

/*
 * nsm_lat_get()
 *	Gets latency statistics for a netdevice.
 */
bool nsm_lat_get(char *netdev_name, uint8_t sid, uint64_t *hist, uint64_t *avg)
{
	struct net_device *dev = dev_get_by_name(&init_net, netdev_name);
	bool ret;
	if (!dev) {
		return false;
	}

	ret = netdev_sawf_lat_get(dev, sid, hist, avg);
	dev_put(dev);
	return ret;
}

/*
 * nsm_lat_init()
 *	Initialize and allocate latency statistics for a netdevice.
 */
bool nsm_lat_init(char *netdev_name)
{
	struct net_device *dev = dev_get_by_name(&init_net, netdev_name);
	bool ret;
	if (!dev) {
		return false;
	}

	ret = netdev_sawf_init(dev, NETDEV_SAWF_FLAG_ENABLED);
	dev_put(dev);
	return ret;
}

/*
 * nsm_lat_deinit()
 *	Frees latency statistics for a netdevice.
 */
bool nsm_lat_deinit(char *netdev_name)
{
	struct net_device *dev = dev_get_by_name(&init_net, netdev_name);
	bool ret;
	if (!dev) {
		return false;
	}

	if (dev->flags & IFF_UP) {
		printk("Deinit of %s failed. Device must be down. Current flags: %x", netdev_name, dev->flags);
		return false;
	}

	ret = netdev_sawf_deinit(dev);
	dev_put(dev);
	return ret;
}

/*
 * nsm_lat_disable()
 *	Stops collection of latency statistics on a netdevice.
 */
bool nsm_lat_disable(char *netdev_name)
{
	struct net_device *dev = dev_get_by_name(&init_net, netdev_name);
	bool ret;
	if (!dev) {
		return false;
	}

	ret = netdev_sawf_disable(dev);
	dev_put(dev);
	return ret;
}

/*
 * nsm_lat_enable()
 *	Resumes collection of latency statistics on a netdevice.
 */
bool nsm_lat_enable(char *netdev_name)
{
	struct net_device *dev = dev_get_by_name(&init_net, netdev_name);
	bool ret;
	if (!dev) {
		return false;
	}

	ret = netdev_sawf_enable(dev);
	dev_put(dev);
	return ret;
}

/*
 * nsm_lat_set_debug()
 *	Sets a service class ID for collection of latency debug statistics.
 */
bool nsm_lat_set_debug(char *netdev_name, uint8_t sid)
{
	struct net_device *dev = dev_get_by_name(&init_net, netdev_name);
	bool ret;
	if (!dev) {
		return false;
	}

	ret = netdev_sawf_debug_set(dev, sid);
	dev_put(dev);
	return ret;
}

/*
 * nsm_lat_unset_debug()
 *	Stops collection of latency debug statistics.
 */
bool nam_lat_unset_debug(char *netdev_name)
{
	struct net_device *dev = dev_get_by_name(&init_net, netdev_name);
	bool ret;
	if (!dev) {
		return false;
	}

	ret = netdev_sawf_debug_unset(dev);
	dev_put(dev);
	return ret;
}
