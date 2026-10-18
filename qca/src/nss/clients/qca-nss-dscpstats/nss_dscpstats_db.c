/*
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

#include <linux/module.h>
#include <linux/types.h>
#include <linux/hashtable.h>
#include <linux/slab.h>
#include <linux/atomic.h>
#include <linux/vmalloc.h>
#include <linux/netdevice.h>
#include <linux/debugfs.h>

#include "nss_dscpstats.h"
#include "nss_dscpstats_nl.h"
#include <ecm_classifier_dscp_stats_public.h>

struct nss_dscpstats nss_dscpstats_ctx;
uint32_t num_clients;

/**
 * nss_dscpstats_db_generate_hash_index()
 *	hash function to generate the index for the hash table
 */
static unsigned int nss_dscpstats_db_generate_hash_index(uint64_t mac_address)
{
	return hash_min(mac_address, ilog2(nss_dscpstats_ctx.hash_size));
}

/**
 * nss_dscpstats_db_delete_dscp_node()
 *	delete dscp_node from table
 */
static void nss_dscpstats_db_delete_dscp_node(struct nss_dscpstats_dscp_node *node)
{
	hlist_del(&node->dscp_node);
	kfree(node);
}

/**
 * nss_dscpstats_db_create_dscp_node()
 *	add a new dscp node to the mac node
 */
static struct nss_dscpstats_dscp_node *nss_dscpstats_db_create_dscp_node(uint8_t dscp_value)
{
	struct nss_dscpstats_dscp_node *new_node;

	new_node = kzalloc(sizeof(struct nss_dscpstats_dscp_node), GFP_KERNEL);
	if (!new_node) {
		return NULL;
	}

	INIT_HLIST_NODE(&new_node->dscp_node);
	new_node->dscp = dscp_value;
	return new_node;
}

/**
 * nss_dscpstats_db_remove_mac_node()
 *	delete a mac node and all of its dscp nodes
 */
static void nss_dscpstats_db_remove_mac_node(struct nss_dscpstats_mac_node *mac_node)
{
	struct nss_dscpstats_dscp_node *dscp_node;
	struct hlist_node *tmp;

	hlist_for_each_entry_safe(dscp_node, tmp, &mac_node->dscp_list, dscp_node) {
		nss_dscpstats_db_delete_dscp_node(dscp_node);
	}

	hlist_del(&mac_node->mac_node);
	kfree(mac_node);
	atomic_dec(&nss_dscpstats_ctx.common_stats.tot_act_mac);
	nss_dscpstats_ctx.common_stats.tot_mac_del++;
}

/**
 * nss_dscpstats_db_delete_wan_node()
 *	delete for dev from WAN list.
 */
static void nss_dscpstats_db_delete_wan_node(struct nss_dscpstats_wanif *wan_if_node)
{
	hlist_del(&wan_if_node->wan_node);
	kfree(wan_if_node);
}

/**
 * nss_dscpstats_db_create_mac_node()
 *	create a new mac node
 */
static struct nss_dscpstats_mac_node *nss_dscpstats_db_create_mac_node(uint64_t mac)
{
	struct nss_dscpstats_mac_node *new_node;

	new_node = kzalloc(sizeof(struct nss_dscpstats_mac_node), GFP_KERNEL);
	if (!new_node) {
		return NULL;
	}

	new_node->mac_addr = mac;
	INIT_HLIST_HEAD(&new_node->dscp_list);
	INIT_HLIST_NODE(&new_node->mac_node);
	atomic_inc(&nss_dscpstats_ctx.common_stats.tot_act_mac);
	nss_dscpstats_ctx.common_stats.tot_mac_add++;
	return new_node;
}

/**
 * nss_dscpstats_db_wan_node_get()
 *	search for dev name exists in WAN list, this is called under spinlock
 */
static struct nss_dscpstats_wanif *nss_dscpstats_db_wan_node_get(uint8_t *dev_name)
{
	struct nss_dscpstats_wanif *wan_if_node;

	hlist_for_each_entry(wan_if_node, &nss_dscpstats_ctx.wan_list, wan_node) {
		if (!strncmp(dev_name, wan_if_node->dev_name, 16)) {
			return wan_if_node;
		}
	}

	return NULL;
}

/**
 * nss_dscpstats_db_wan_node_remove_all()
 *	Remove all the wan nodes from list
 */
static void nss_dscpstats_db_wan_node_remove_all(void)
{
	struct nss_dscpstats_wanif *wan_if_node;
	struct hlist_node *tmp;

	spin_lock(&nss_dscpstats_ctx.lock);
	hlist_for_each_entry_safe(wan_if_node, tmp, &nss_dscpstats_ctx.wan_list, wan_node) {
		nss_dscpstats_db_delete_wan_node(wan_if_node);
	}
	spin_unlock(&nss_dscpstats_ctx.lock);
}

/**
 * nss_dscpstats_db_enable_dscp()
 *	enable dscp values for a interface
 */
enum nss_dscpstats_ret nss_dscpstats_db_enable_dscp(uint64_t dscp_mask, uint8_t dev_name[])
{
	struct ecm_classifier_dscp_stats_registrant r;
	struct nss_dscpstats_wanif *wan_if_node;
	struct nss_dscpstats_wanif *new_node;
	bool register_cb;

	if (!dscp_mask) {
		nss_dscpstats_warn("Error: Invalid value of DSCP MASK\n");
		return DSCP_STATS_RET_INVALID_VALUE;
	}

	register_cb = hlist_empty(&nss_dscpstats_ctx.wan_list);

	spin_lock(&nss_dscpstats_ctx.lock);
	wan_if_node = nss_dscpstats_db_wan_node_get(dev_name);
	if (wan_if_node != NULL) {
		wan_if_node->dscp_mask = wan_if_node->dscp_mask | dscp_mask;
		spin_unlock(&nss_dscpstats_ctx.lock);
		return DSCP_STATS_RET_SUCCESS;
	}
	spin_unlock(&nss_dscpstats_ctx.lock);

	new_node = kzalloc(sizeof(struct nss_dscpstats_wanif), GFP_KERNEL);
	if (!new_node) {
		return DSCP_STATS_RET_ENABLE_FAIL_OOM;
	}

	strlcpy(new_node->dev_name, dev_name, 16);
	new_node->dscp_mask = dscp_mask;
	INIT_HLIST_NODE(&new_node->wan_node);
	spin_lock(&nss_dscpstats_ctx.lock);
	hlist_add_head(&new_node->wan_node, &nss_dscpstats_ctx.wan_list);
	spin_unlock(&nss_dscpstats_ctx.lock);

	if (register_cb) {
		/*
		 * register callback
		 */
		r.cb = nss_dscpstats_db_stats_process;
		r.this_module = THIS_MODULE;
		ecm_classifier_dscp_stats_register(&r);
	}

	return DSCP_STATS_RET_SUCCESS;
}

/**
 * nss_dscpstats_db_disable_dscp()
 *	disable dscp values for a interface
 */
enum nss_dscpstats_ret nss_dscpstats_db_disable_dscp(uint64_t dscp_mask, uint8_t dev_name[])
{
	struct nss_dscpstats_wanif *wan_if_node = NULL;
	struct ecm_classifier_dscp_stats_registrant r;
	struct nss_dscpstats_mac_node *mac_node;
	struct nss_dscpstats_dscp_node *dscp_node;
	struct hlist_node *tmp_dscp_node;
	struct hlist_node *tmp_mac_node;
	uint64_t tmp_mask = 0;
	uint8_t dscp_value;
	uint16_t index;

	if (!dscp_mask) {
		nss_dscpstats_warn("Error: Invalid value of DSCP MASK\n");
		return DSCP_STATS_RET_INVALID_VALUE;
	}

	spin_lock(&nss_dscpstats_ctx.lock);
	wan_if_node = nss_dscpstats_db_wan_node_get(dev_name);
	if (wan_if_node == NULL) {
		spin_unlock(&nss_dscpstats_ctx.lock);
		return DSCP_STATS_RET_ALREADY_DISABLED;
	}

	dscp_mask = dscp_mask & wan_if_node->dscp_mask;
	wan_if_node->dscp_mask = wan_if_node->dscp_mask ^ dscp_mask;
	if (!wan_if_node->dscp_mask) {
		nss_dscpstats_db_delete_wan_node(wan_if_node);
	}

	/*
	 * find out the bits that are also enabled in other wan interface dscp mask
	 */
	hlist_for_each_entry(wan_if_node, &nss_dscpstats_ctx.wan_list, wan_node) {
		tmp_mask |= wan_if_node->dscp_mask;
	}

	/*
	 * disable the dscp values which are not enabled in any of the wan interface
	 */
	dscp_mask = dscp_mask & (~tmp_mask);

	for (index = 0; index < nss_dscpstats_ctx.hash_size; index++) {
		hlist_for_each_entry_safe(mac_node, tmp_mac_node, &nss_dscpstats_ctx.mac_hash_table[index], mac_node) {
			dscp_value = 0;
			while (dscp_value < 64) {
				if (dscp_mask & ((uint64_t)1 << dscp_value)) {
					hlist_for_each_entry_safe(dscp_node, tmp_dscp_node, &mac_node->dscp_list, dscp_node) {
						if (dscp_node->dscp == dscp_value) {
							nss_dscpstats_db_delete_dscp_node(dscp_node);
						}
					}
				}
				dscp_value += 1;
			}

			if (hlist_empty(&mac_node->dscp_list)) {
				nss_dscpstats_db_remove_mac_node(mac_node);
			}
		}
	}
	spin_unlock(&nss_dscpstats_ctx.lock);

	if (hlist_empty(&nss_dscpstats_ctx.wan_list)) {
		/*
		 * unregister callback
		 */
		r.cb = nss_dscpstats_db_stats_process;
		r.this_module = THIS_MODULE;
		ecm_classifier_dscp_stats_unregister(&r);
	}

	return DSCP_STATS_RET_SUCCESS;
}

/**
 * nss_dscpstats_db_reset()
 *	function to reset the table by deleting all mac and dscp nodes
 */
void nss_dscpstats_db_reset(void)
{
	struct nss_dscpstats_mac_node *mac_node;
	struct hlist_node *tmp;
	uint16_t index;

	spin_lock(&nss_dscpstats_ctx.lock);
	for (index = 0; index < nss_dscpstats_ctx.hash_size; index++) {
		hlist_for_each_entry_safe(mac_node, tmp, &nss_dscpstats_ctx.mac_hash_table[index], mac_node) {
			nss_dscpstats_db_remove_mac_node(mac_node);
		}
	}
	spin_unlock(&nss_dscpstats_ctx.lock);
}

/**
 * nss_dscpstats_db_remove_inactive_mac()
 *	remove inactive mac nodes
 */
static bool nss_dscpstats_db_remove_inactive_mac(void)
{
	unsigned int index;
	struct nss_dscpstats_mac_node *mac_node;
	struct nss_dscpstats_dscp_node *dscp_node;
	struct hlist_node *tmp;
	int free_mac = 1;

	spin_lock(&nss_dscpstats_ctx.lock);
	for (index = 0; index < nss_dscpstats_ctx.hash_size; index++) {
		hlist_for_each_entry_safe(mac_node, tmp, &nss_dscpstats_ctx.mac_hash_table[index], mac_node) {
			free_mac = 1;
			hlist_for_each_entry(dscp_node, &mac_node->dscp_list, dscp_node) {
				if (dscp_node->txBytes != 0 || dscp_node->rxBytes != 0) {
					free_mac = 0;
					break;
				}
			}

			if (free_mac) {
				nss_dscpstats_db_remove_mac_node(mac_node);
				spin_unlock(&nss_dscpstats_ctx.lock);
				return true;
			}
		}
	}
	spin_unlock(&nss_dscpstats_ctx.lock);
	return false;
}

/**
 * nss_dscpstats_db_count_store()
 *	update tx rx bytes count of client
 */
static enum nss_dscpstats_ret nss_dscpstats_db_count_store(uint8_t tx_dscp, uint8_t rx_dscp, uint64_t mac,
								uint32_t tx_bytes, uint32_t rx_bytes)
{
	unsigned int index = nss_dscpstats_db_generate_hash_index(mac);
	struct nss_dscpstats_mac_node *mac_node;
	struct nss_dscpstats_dscp_node *dscp_node;
	bool tx_dscp_found = false;
	bool rx_dscp_found = false;

	spin_lock(&nss_dscpstats_ctx.lock);
	hlist_for_each_entry(mac_node, &nss_dscpstats_ctx.mac_hash_table[index], mac_node) {
		if (mac_node->mac_addr == mac) {
			hlist_for_each_entry(dscp_node, &mac_node->dscp_list, dscp_node) {
					if (dscp_node->dscp == tx_dscp) {
						dscp_node->txBytes += tx_bytes;
						dscp_node->total_txBytes += tx_bytes;
						tx_dscp_found = true;
					}

					if (dscp_node->dscp == rx_dscp) {
						dscp_node->rxBytes += rx_bytes;
						dscp_node->total_rxBytes += rx_bytes;
						rx_dscp_found = true;
					}
			}

			if (tx_dscp_found && rx_dscp_found) {
				spin_unlock(&nss_dscpstats_ctx.lock);
				return DSCP_STATS_RET_SUCCESS;
			}

			break;
		}
	}
	spin_unlock(&nss_dscpstats_ctx.lock);

	if (mac_node != NULL) {
		if (tx_dscp == rx_dscp) {
			dscp_node = nss_dscpstats_db_create_dscp_node(tx_dscp);
			if (!dscp_node) {
				return DSCP_STATS_RET_FAIL;
			}

			dscp_node->txBytes += tx_bytes;
			dscp_node->total_txBytes += tx_bytes;
			dscp_node->rxBytes += rx_bytes;
			dscp_node->total_rxBytes += rx_bytes;
			spin_lock(&nss_dscpstats_ctx.lock);
			hlist_add_head(&dscp_node->dscp_node, &mac_node->dscp_list);
			spin_unlock(&nss_dscpstats_ctx.lock);
			return DSCP_STATS_RET_SUCCESS;
		}

		if (!tx_dscp_found) {
			dscp_node = nss_dscpstats_db_create_dscp_node(tx_dscp);
			if (!dscp_node) {
				return DSCP_STATS_RET_FAIL;
			}

			dscp_node->txBytes += tx_bytes;
			dscp_node->total_txBytes += tx_bytes;
			spin_lock(&nss_dscpstats_ctx.lock);
			hlist_add_head(&dscp_node->dscp_node, &mac_node->dscp_list);
			spin_unlock(&nss_dscpstats_ctx.lock);
		}

		if (!rx_dscp_found) {
			dscp_node = nss_dscpstats_db_create_dscp_node(rx_dscp);
			if (!dscp_node) {
				return DSCP_STATS_RET_FAIL;
			}
			dscp_node->rxBytes += rx_bytes;
			dscp_node->total_rxBytes += rx_bytes;
			spin_lock(&nss_dscpstats_ctx.lock);
			hlist_add_head(&dscp_node->dscp_node, &mac_node->dscp_list);
			spin_unlock(&nss_dscpstats_ctx.lock);
		}

		return DSCP_STATS_RET_SUCCESS;
	}

	if (atomic_read(&nss_dscpstats_ctx.common_stats.tot_act_mac) >= num_clients) {
		nss_dscpstats_warn("max client limit has reached, try removing inactive clients\n");

		if (!nss_dscpstats_db_remove_inactive_mac()) {
			nss_dscpstats_warn("No inactive clients found, could not create client %pM\n", (uint8_t *)(&mac));
			nss_dscpstats_ctx.common_stats.mac_node_add_fail++;
			return DSCP_STATS_RET_ENABLE_FAIL_MAX_CLIENT_LIMIT;
		}
	}

	mac_node = nss_dscpstats_db_create_mac_node(mac);
	if (!mac_node) {
		nss_dscpstats_warn("Could not create new client node %pM\n", (uint8_t *)(&mac));
		return DSCP_STATS_RET_ENABLE_FAIL_OOM;
	}
	spin_lock(&nss_dscpstats_ctx.lock);
	hlist_add_head(&mac_node->mac_node, &nss_dscpstats_ctx.mac_hash_table[index]);
	spin_unlock(&nss_dscpstats_ctx.lock);

	if (tx_dscp == rx_dscp) {
		dscp_node = nss_dscpstats_db_create_dscp_node(tx_dscp);
		if (!dscp_node) {
			return DSCP_STATS_RET_FAIL;
		}
		dscp_node->txBytes += tx_bytes;
		dscp_node->total_txBytes += tx_bytes;
		dscp_node->rxBytes += rx_bytes;
		dscp_node->total_rxBytes += rx_bytes;
		spin_lock(&nss_dscpstats_ctx.lock);
		hlist_add_head(&dscp_node->dscp_node, &mac_node->dscp_list);
		spin_unlock(&nss_dscpstats_ctx.lock);
		return DSCP_STATS_RET_SUCCESS;
	}

	dscp_node = nss_dscpstats_db_create_dscp_node(tx_dscp);
	if (!dscp_node) {
		return DSCP_STATS_RET_FAIL;
	}
	dscp_node->txBytes += tx_bytes;
	dscp_node->total_txBytes += tx_bytes;
	spin_lock(&nss_dscpstats_ctx.lock);
	hlist_add_head(&dscp_node->dscp_node, &mac_node->dscp_list);
	spin_unlock(&nss_dscpstats_ctx.lock);

	dscp_node = nss_dscpstats_db_create_dscp_node(rx_dscp);
	if (!dscp_node) {
		return DSCP_STATS_RET_FAIL;
	}
	dscp_node->rxBytes += rx_bytes;
	dscp_node->total_rxBytes += rx_bytes;
	spin_lock(&nss_dscpstats_ctx.lock);
	hlist_add_head(&dscp_node->dscp_node, &mac_node->dscp_list);
	spin_unlock(&nss_dscpstats_ctx.lock);

	return DSCP_STATS_RET_SUCCESS;
}

/**
 * nss_dscpstats_db_stats_process()
 *	Process the dscp and bytes to store.
 */
void nss_dscpstats_db_stats_process(struct net_device *from_dev, struct net_device *to_dev, uint8_t *smac,
		uint8_t *dmac, uint8_t dscp_marked, uint8_t flow_mark, uint8_t reply_mark, uint8_t flow_dscp,
		uint8_t reply_dscp, uint32_t tx_bytes, uint32_t rx_bytes, bool is_ipv6)
{
	struct nss_dscpstats_wanif *wan_if_node;
	struct net_device *dev;
	uint64_t real_smac = 0;
	uint8_t real_tx_dscp, real_rx_dscp;
	uint32_t real_tx_bytes = 0, real_rx_bytes = 0;

	/*
	 * skip MAP-T outer rule, for MAP-T inner rule stats will be considered
	 * since we cant get smac of LAN client from outer rule.
	 */

	if ((from_dev->priv_flags_ext & IFF_EXT_MAPT) && is_ipv6) {
		return;
	}

	spin_lock(&nss_dscpstats_ctx.lock);
	hlist_for_each_entry(wan_if_node, &nss_dscpstats_ctx.wan_list, wan_node) {
		if (!strncmp(to_dev->name, wan_if_node->dev_name, 16)) {
			dev = from_dev;
			memcpy(&real_smac, smac, 6);

			real_tx_dscp = flow_dscp;
			real_rx_dscp = reply_dscp;

			if (dscp_marked) {
				real_tx_dscp = flow_mark;
			}

			if (wan_if_node->dscp_mask & (1ULL << real_tx_dscp)) {
				real_tx_bytes = tx_bytes;
			}

			if (wan_if_node->dscp_mask & (1ULL << real_rx_dscp)) {
				real_rx_bytes = rx_bytes;
			}
			break;
		} else if (!strncmp(from_dev->name, wan_if_node->dev_name, 16)) {
			dev = to_dev;
			memcpy(&real_smac, dmac, 6);

			real_tx_dscp = reply_dscp;
			real_rx_dscp = flow_dscp;

			if (dscp_marked) {
				real_rx_dscp = flow_mark;
			}

			if (wan_if_node->dscp_mask & (1ULL << real_tx_dscp)) {
				real_tx_bytes = rx_bytes;
			}

			if (wan_if_node->dscp_mask & (1ULL << real_rx_dscp)) {
				real_rx_bytes = tx_bytes;
			}
			break;
		}
	}
	spin_unlock(&nss_dscpstats_ctx.lock);

	if (real_tx_bytes || real_rx_bytes) {
		nss_dscpstats_db_count_store(real_tx_dscp, real_rx_dscp, real_smac, real_tx_bytes, real_rx_bytes);
	}
}

/**
 * nss_dscpstats_init_table()
 *	function to initialize hashtable
 */
uint32_t nss_dscpstats_db_init_table(void)
{
	struct hlist_head *mac_hash_table;
	uint16_t hash_size;
	uint32_t index;

	hash_size = num_clients / NSS_DSCPSTATS_CLIENT_HASH_RATIO;
	mac_hash_table = vzalloc(hash_size * sizeof(struct hlist_head));
	if (!mac_hash_table) {
		nss_dscpstats_warn("Error: Unable to initialize hash table\n");
		return -1;
	}

	spin_lock_init(&nss_dscpstats_ctx.lock);

	for (index = 0; index < hash_size; index++) {
		INIT_HLIST_HEAD(&mac_hash_table[index]);
	}

	spin_lock(&nss_dscpstats_ctx.lock);
	nss_dscpstats_ctx.hash_size = hash_size;
	nss_dscpstats_ctx.mac_hash_table = mac_hash_table;
	spin_unlock(&nss_dscpstats_ctx.lock);

	return 0;
}

/**
 * nss_dscpstats_db_free_table()
 *	free the memory assigned to hash table
 */
void nss_dscpstats_db_free_table(void)
{
	nss_dscpstats_db_wan_node_remove_all();
	nss_dscpstats_db_reset();
	vfree(nss_dscpstats_ctx.mac_hash_table);
	nss_dscpstats_ctx.mac_hash_table = NULL;
}

/**
 * nss_dscpstats_db_stop()
 *	stop the statistics update
 */
void nss_dscpstats_db_stop(void)
{
	struct ecm_classifier_dscp_stats_registrant r;
	/*
	 * unregister callback
	 */
	r.cb = nss_dscpstats_db_stats_process;
	r.this_module = THIS_MODULE;
	ecm_classifier_dscp_stats_unregister(&r);
}
