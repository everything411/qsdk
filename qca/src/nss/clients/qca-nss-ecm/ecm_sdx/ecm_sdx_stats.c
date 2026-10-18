/*
 **************************************************************************
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: ISC
 **************************************************************************
 */

#include <linux/version.h>
#include <linux/types.h>
#include <linux/ip.h>
#include <net/ip.h>
#include <net/ipv6.h>
#include <linux/inet.h>
#include <linux/in.h>
#include <linux/etherdevice.h>
#include <linux/vmalloc.h>

/*
 * Debug output levels
 * 0 = OFF
 * 1 = ASSERTS / ERRORS
 * 2 = 1 + WARN
 * 3 = 2 + INFO
 * 4 = 3 + TRACE
 */
#define DEBUG_LEVEL 1

#include "ecm_types.h"
#include "ecm_db_types.h"
#include "ecm_state.h"
#include "ecm_tracker.h"
#include "ecm_classifier.h"
#include "ecm_db.h"
#include "ecm_sdx_stats.h"

/*
 * Locking of the host_stats database - concurrency control
 */
DEFINE_SPINLOCK(ecm_sdx_stats_db_lock);			/* Protect the hash table using lock */

/*
 * Max message size
 */
#define ECM_SDX_STATS_MSG_SIZE 600

/*
 * Max interface type size
 */
#define ECM_SDX_STATS_MAX_INTERFACE_SIZE 50

/*
 * IPv4 hash table size information for packet stats.
 */
#define ECM_SDX_STATS_HASH_SHIFT 12
#define ECM_SDX_STATS_HASH_SIZE (1 << ECM_SDX_STATS_HASH_SHIFT)

/*
 * Hash function to generate hash key for a particular ip
 */
#define ecm_sdx_stats_conn_hash(key, saddr)		\
{			\
	key = ((saddr[0] ^ saddr[1] ^ saddr[2] ^ saddr[3]) % ECM_SDX_STATS_HASH_SIZE);	\
	key = hash_32(key, ECM_SDX_STATS_HASH_SHIFT);		\
}

/*
 * ecm_sdx_stats_htable hash_table
 */
static DEFINE_HASHTABLE(ecm_sdx_stats_htable, ECM_SDX_STATS_HASH_SHIFT);

/*
 * v4 subnets structure
 * This is required inorder to store only the host entries which are in the lan subnet
 */
struct ecm_sdx_stats_ipv4_lan_prefix {
	ip_addr_t ip_addr;		/* ip address */
	uint32_t prefix;		/* subnet info */
};

/*
 * Structure to store v6 prefixes
 * This is required inorder to store only the host entries which are in the lan prefix
 */
struct ecm_sdx_stats_ipv6_lan_prefix {
	ip_addr_t ip_addr;		/* ip address */
	uint8_t prefix;			/* prefix info */
};

/*
 * ecm_sdx_stats_info_instance structure
 */
struct ecm_sdx_stats_info_instance {
	uint8_t num_of_pack_stat_nodes_v4;			/* number of v4 host entries */
	uint8_t num_of_pack_stat_nodes_v6;			/* number of v6 host entries */
	ip_addr_t per_ip_address;				/* ip for per ip client stats */
	ecm_db_iface_type_t packet_stats_iface_type;		/* iface_type on which call is up */
	struct ecm_sdx_stats_ipv4_lan_prefix v4_prefix;		/* v4 subnet information */
	struct ecm_sdx_stats_ipv6_lan_prefix v6_prefix;		/* v6 prefix information */
	struct ecm_sdx_stats_ipv4_lan_prefix ippt_v4_prefix;	/* v4 subnet information for ippt*/
};

/*
 * ecm_sdx_stats_host_instance to store host stats
 */
struct ecm_sdx_stats_host_instance {
	ip_addr_t client_src_addr;		/* host ip address */
	uint64_t packet_stat_rx_byte_count;	/* total routed rx_bytes */
	uint64_t packet_stat_tx_byte_count;	/* total routed tx_bytes */
	uint64_t prev_stats_rx_byte_cout;	/* prev rx_bytes required for calculation.
						 * when reset is done from userspace.
						 */
	uint64_t prev_stats_tx_byte_cout;	/* prev tx_bytes required for calculation
						 * when reset is done from userspace.
						 */
	uint64_t prev_host_rx_byte_count;	/* prev host tx_bytes required for calculation
						 * when host is removed from ecm_db due to
						 * no connections but still host is in connected state.
						 */
	uint64_t prev_host_tx_byte_count;	/* prev host tx_bytes required for calculation
						 * when host is removed from ecm_db due to
						 * no connections but still host is in connected state.
						 */
};

/*
 * ecm_sdx_stats_list hash_list
 */
struct ecm_sdx_stats_list {
	struct ecm_sdx_stats_host_instance host_instance;	/* node to maintain host info */
	struct hlist_node ecm_packet_hash_list;			/* require for hlist */
};

/*
 * Global ecm_sdx_stats_info_instance
 */
struct ecm_sdx_stats_info_instance *ecm_sdx_stats_instance;

/*
 * ecm_sdx_stats_reset_all_host_stats()
 * 	Reset packet stats for all host from stats db.
 */
static void ecm_sdx_stats_reset_all_host_stats(void)
{
	struct ecm_sdx_stats_list *curr = NULL;
	struct ecm_db_host_instance *hi = NULL;
	struct hlist_node *tmp = NULL;
	char ip_addr[ECM_IP_ADDR_STR_BUFF_SIZE] = {0};
	uint64_t rx_bytes = 0;
	uint64_t tx_bytes = 0;
	uint64_t prev_host_rx = 0;
	uint64_t prev_host_tx = 0;
	int bkt = 0;

	DEBUG_TRACE("reset all host stats \n");
	spin_lock_bh(&ecm_sdx_stats_db_lock);
	hash_for_each_safe(ecm_sdx_stats_htable, bkt, tmp, curr, ecm_packet_hash_list) {
		if (curr != NULL) {
			ecm_ip_addr_to_string(ip_addr, curr->host_instance.client_src_addr);
			rx_bytes = curr->host_instance.packet_stat_rx_byte_count;
			tx_bytes = curr->host_instance.packet_stat_tx_byte_count;
			prev_host_rx = curr->host_instance.prev_host_rx_byte_count;
			prev_host_tx = curr->host_instance.prev_host_tx_byte_count;
			hi = ecm_db_host_find_and_ref(curr->host_instance.client_src_addr);

			if(hi) {
				curr->host_instance.prev_stats_rx_byte_cout += rx_bytes - prev_host_rx;
				curr->host_instance.prev_stats_tx_byte_cout += tx_bytes - prev_host_tx;
				ecm_db_host_deref(hi);
			} else {
				curr->host_instance.prev_stats_rx_byte_cout = 0;
				curr->host_instance.prev_stats_tx_byte_cout = 0;
			}

			DEBUG_TRACE("ip=%s, prev_rx=%llu & rx=%llu\n", ip_addr,
				curr->host_instance.prev_stats_rx_byte_cout, rx_bytes);
			DEBUG_TRACE("ip=%s, prev_tx=%llu & tx=%llu\n", ip_addr,
				curr->host_instance.prev_stats_tx_byte_cout, tx_bytes);
			curr->host_instance.packet_stat_rx_byte_count = 0;
			curr->host_instance.packet_stat_tx_byte_count = 0;
			curr->host_instance.prev_host_rx_byte_count = 0;
			curr->host_instance.prev_host_tx_byte_count = 0;
		}
	}
	spin_unlock_bh(&ecm_sdx_stats_db_lock);
}

/*
 * ecm_sdx_stats_reset_host_stats()
 * 	Reset stats for a particular host in db.
 */
static void ecm_sdx_stats_reset_host_stats(ip_addr_t host_ip)
{
	struct ecm_sdx_stats_list *curr = NULL;
	struct ecm_db_host_instance *hix = NULL;
	char ip_addr[ECM_IP_ADDR_STR_BUFF_SIZE] = {0};
	uint64_t rx_bytes = 0;
	uint64_t tx_bytes = 0;
	uint64_t prev_host_rx = 0;
	uint64_t prev_host_tx = 0;
	u32 key;

	ecm_ip_addr_to_string(ip_addr, host_ip);
	DEBUG_TRACE("reset host stats for ip_addr=%s\n", ip_addr);

	/*
	 * Get the ip address of the host and check if it already present in the list
	 * if it is then just update the corresponding rx and tx bytes
	 */
	ecm_sdx_stats_conn_hash(key, host_ip);
	spin_unlock_bh(&ecm_sdx_stats_db_lock);
	hash_for_each_possible(ecm_sdx_stats_htable, curr, ecm_packet_hash_list, key) {
		if(*(curr->host_instance.client_src_addr) == *(host_ip)) {
			rx_bytes = curr->host_instance.packet_stat_rx_byte_count;
			tx_bytes = curr->host_instance.packet_stat_tx_byte_count;
			prev_host_rx = curr->host_instance.prev_host_rx_byte_count;
			prev_host_tx = curr->host_instance.prev_host_tx_byte_count;
			hix = ecm_db_host_find_and_ref(curr->host_instance.client_src_addr);

			if(hix) {
				curr->host_instance.prev_stats_rx_byte_cout += rx_bytes - prev_host_rx;
				curr->host_instance.prev_stats_tx_byte_cout += tx_bytes - prev_host_tx;
				ecm_db_host_deref(hix);
			} else {
				curr->host_instance.prev_stats_rx_byte_cout = 0;
				curr->host_instance.prev_stats_tx_byte_cout = 0;
			}

			DEBUG_TRACE("ip=%s, prev_rx=%llu & rx=%llu\n", ip_addr,
				curr->host_instance.prev_stats_rx_byte_cout, rx_bytes);
			DEBUG_TRACE("ip=%s, prev_tx=%llu & tx=%llu\n", ip_addr,
				curr->host_instance.prev_stats_tx_byte_cout, tx_bytes);
			curr->host_instance.packet_stat_rx_byte_count = 0;
			curr->host_instance.packet_stat_tx_byte_count = 0;
			curr->host_instance.prev_host_rx_byte_count = 0;
			curr->host_instance.prev_host_tx_byte_count = 0;
		}
	}
	spin_unlock_bh(&ecm_sdx_stats_db_lock);
}

/*
 * ecm_sdx_stats_remove_host_stats()
 * 	Remove the host entry from db.
 */
static void ecm_sdx_stats_remove_host_stats(ip_addr_t host_ip)
{
	struct ecm_sdx_stats_list *curr = NULL;
	char ip_addr[ECM_IP_ADDR_STR_BUFF_SIZE] = {0};
	u32 key;

	ecm_ip_addr_to_string(ip_addr,host_ip);
	DEBUG_TRACE("ecm_sdx_stats_remove_host_stats called for ip=%s\n", ip_addr);

	/*
	 * Get the ip address of the host and check if it already present in the list
	 * if it is then just update the corresponding rx and tx bytes
	 */
	ecm_sdx_stats_conn_hash(key, host_ip);
	spin_lock_bh(&ecm_sdx_stats_db_lock);
	hash_for_each_possible(ecm_sdx_stats_htable, curr, ecm_packet_hash_list, key) {
		if(*(curr->host_instance.client_src_addr) == *(host_ip)) {
			DEBUG_TRACE("Remove host stats for ip_addr=%s\n", ip_addr);
			hash_del(&curr->ecm_packet_hash_list);
			if(ECM_IP_ADDR_IS_V4(host_ip)) {
				ecm_sdx_stats_instance->num_of_pack_stat_nodes_v4--;
			} else {
				ecm_sdx_stats_instance->num_of_pack_stat_nodes_v6--;
			}
			kfree(curr);
			break;
		}
	}
	spin_unlock_bh(&ecm_sdx_stats_db_lock);
}

/*
 * ecm_sdx_stats_is_valid_lan_prefix()
 * 	Check if host ip is in same subnet as lan prefixes.
 */
static bool ecm_sdx_stats_is_valid_lan_prefix(ip_addr_t host_ip)
{
	int prefix, full_prefixes;
	int remaining_bits;
	int total_prefixes = 4;
	bool full_prefixes_matched = true;
	bool remaining_bits_matched = false;
	uint32_t mask;
	bool ippt_prefix_null = true;

	if(ECM_IP_ADDR_IS_V4(host_ip)) {
		DEBUG_TRACE("validate ipv4 host prefix ip=%u", *(host_ip));
		if(ECM_IP_ADDR_IS_NULL(ecm_sdx_stats_instance->v4_prefix.ip_addr)) {
			DEBUG_ERROR("NULL ipv4 lan prefix found");
			return false;
		}

		if(!ECM_IP_ADDR_IS_NULL(ecm_sdx_stats_instance->ippt_v4_prefix.ip_addr)) {
			DEBUG_ERROR("ippt lan prefix is not NULL");
			ippt_prefix_null = false;
		}

		if((host_ip[0] & ecm_sdx_stats_instance->v4_prefix.prefix) ==
			(ecm_sdx_stats_instance->v4_prefix.ip_addr[0] &
			ecm_sdx_stats_instance->v4_prefix.prefix)) {
			DEBUG_TRACE("In lan_prefix ip=%u,host_ip=%u,mask1=%u,mask2=%u\n",
				*(ecm_sdx_stats_instance->v4_prefix.ip_addr),
				*(host_ip),
				host_ip[0] & ecm_sdx_stats_instance->v4_prefix.prefix,
				(ecm_sdx_stats_instance->v4_prefix.ip_addr[0] &
				ecm_sdx_stats_instance->v4_prefix.prefix));
			return true;
		}

		if(!ippt_prefix_null &&
			(host_ip[0] & ecm_sdx_stats_instance->ippt_v4_prefix.prefix) ==
			(ecm_sdx_stats_instance->ippt_v4_prefix.ip_addr[0] &
			ecm_sdx_stats_instance->ippt_v4_prefix.prefix))
		{
			DEBUG_TRACE("In ippt_lan_prefix ip=%u,host_ip=%u,mask1=%u,mask2=%u\n",
				*(ecm_sdx_stats_instance->ippt_v4_prefix.ip_addr),
				*(host_ip),
				host_ip[0] & ecm_sdx_stats_instance->ippt_v4_prefix.prefix,
				(ecm_sdx_stats_instance->ippt_v4_prefix.ip_addr[0] &
				ecm_sdx_stats_instance->ippt_v4_prefix.prefix));
			return true;
		}
	} else {
		if(ECM_IP_ADDR_IS_NULL(ecm_sdx_stats_instance->v6_prefix.ip_addr)) {
			DEBUG_ERROR("NULL lan ip found");
			return false;
		}

		DEBUG_TRACE("validate ipv6 host subnet ip=%u", *(host_ip));
		full_prefixes_matched = true;
		remaining_bits_matched = true;
		prefix = ecm_sdx_stats_instance->v6_prefix.prefix;
		full_prefixes = prefix / 32;
		remaining_bits = prefix % 32;
		DEBUG_TRACE("In lan_prefix full_prefixes=%d, remaining=%d",
			full_prefixes, remaining_bits);
		for (int prefix_idx = 3; prefix_idx >= (total_prefixes - full_prefixes); prefix_idx--) {
			DEBUG_TRACE("validate ipv6 host subnet prefix_idx=%u, h_pre=%u, s_pre=%u",
			prefix_idx,
			host_ip[prefix_idx], ecm_sdx_stats_instance->v6_prefix.ip_addr[prefix_idx]);
			if((host_ip[prefix_idx] !=
				ecm_sdx_stats_instance->v6_prefix.ip_addr[prefix_idx])) {
				DEBUG_TRACE("In lan_prefix prefix=%d,ip_addr=%u,host_ip=%u \n",
					prefix,
					ecm_sdx_stats_instance->v6_prefix.ip_addr[prefix_idx],
					host_ip[prefix_idx]);
				full_prefixes_matched = false;
				remaining_bits_matched = false;
				break;
			}
		}

		if(full_prefixes_matched && remaining_bits > 0) {
			mask = (~0U << (32 - remaining_bits));
			DEBUG_TRACE("host remaning ip_addr=%u,host_ip=%u \n",
					ecm_sdx_stats_instance->v6_prefix.ip_addr[full_prefixes],
					host_ip[full_prefixes]);
			if((host_ip[full_prefixes] & mask) !=
			(ecm_sdx_stats_instance->v6_prefix.ip_addr[full_prefixes] & mask)) {
				DEBUG_TRACE("host prefix remaning didn't match db_ip=%u,host_ip=%u \n",
					ecm_sdx_stats_instance->v6_prefix.ip_addr[full_prefixes],
				host_ip[full_prefixes]);
				remaining_bits_matched = false;
			}
		}

		if(remaining_bits_matched) {
			DEBUG_TRACE("host is in lan_prefix ip=%u,host_ip=%u\n",
				*(ecm_sdx_stats_instance->v6_prefix.ip_addr),
				*(host_ip));
			return true;
		}
	}

	return false;
}

/*
 * ecm_sdx_stats_insert_host_stats()
 *	Insert host to classifier db in_case if the host is not already present in db.
 */
static void ecm_sdx_stats_insert_host_stats(ip_addr_t host_ip, uint64_t host_rx,
			uint64_t host_tx, bool host_removed)
{
	struct ecm_sdx_stats_list *packet_list;
	char ip_addr[ECM_IP_ADDR_STR_BUFF_SIZE] = {0};
	u32 key;

	ecm_ip_addr_to_string(ip_addr, host_ip);
	DEBUG_TRACE("Insert host stats to db for ip_addr=%s\n", ip_addr);
	if (!ecm_sdx_stats_is_valid_lan_prefix(host_ip)) {
		DEBUG_TRACE("host entry is not in lan_subnet ip_addr=%s\n", ip_addr);
		return;
	}

	packet_list = (struct ecm_sdx_stats_list *)
	                kmalloc(sizeof(struct ecm_sdx_stats_list), GFP_KERNEL);
	if (unlikely(!packet_list)) {
		DEBUG_WARN("Memory allocation failed for packet_list \n");
		return;
	}

	ECM_IP_ADDR_COPY(packet_list->host_instance.client_src_addr, host_ip);
	packet_list->host_instance.packet_stat_rx_byte_count = host_rx;
	packet_list->host_instance.packet_stat_tx_byte_count = host_tx;
	packet_list->host_instance.prev_stats_rx_byte_cout = 0;
	packet_list->host_instance.prev_stats_tx_byte_cout = 0;
	packet_list->host_instance.prev_host_rx_byte_count = 0;
	packet_list->host_instance.prev_host_tx_byte_count = 0;

	/*
	 * This is required in scenarios where host is deleted from ecm db
	 * due to no connections,but still host is in connected state.
	 */
	if(host_removed) {
		packet_list->host_instance.prev_host_rx_byte_count += host_rx;
		packet_list->host_instance.prev_host_tx_byte_count += host_tx;
	}

	INIT_HLIST_NODE(&packet_list->ecm_packet_hash_list);
	ecm_sdx_stats_conn_hash(key, packet_list->host_instance.client_src_addr);
	DEBUG_TRACE("Insert host stats to db for ip_addr=%s key=%u \n", ip_addr, key);
	spin_lock_bh(&ecm_sdx_stats_db_lock);
	hash_add(ecm_sdx_stats_htable, &packet_list->ecm_packet_hash_list, key);
	spin_unlock_bh(&ecm_sdx_stats_db_lock);

	if(ECM_IP_ADDR_IS_V4(host_ip)) {
		ecm_sdx_stats_instance->num_of_pack_stat_nodes_v4++;
	} else {
		ecm_sdx_stats_instance->num_of_pack_stat_nodes_v6++;
	}
}

/*
 * ecm_sdx_stats_update_all_host_stats()
 *	To update stats for all the host in classifier db.
 */
void ecm_sdx_stats_update_all_host_stats(void)
{
	struct ecm_db_host_instance *hi;
	struct ecm_sdx_stats_list *curr = NULL;
	bool hash_entry_exists = false;
	char host_ip_addr[ECM_IP_ADDR_STR_BUFF_SIZE] = {0};
	uint64_t rx_bytes = 0;
	uint64_t tx_bytes = 0;
	uint64_t prev_rx = 0;
	uint64_t prev_tx = 0;
	uint64_t prev_host_rx = 0;
	uint64_t prev_host_tx = 0;
	uint64_t host_rx = 0;
	uint64_t host_tx = 0;
	ip_addr_t host_ip;
	u32 key;

	DEBUG_TRACE("update stats for all hosts \n");
	if(ecm_sdx_stats_instance->packet_stats_iface_type != ECM_DB_IFACE_TYPE_RAWIP) {
		DEBUG_ERROR("Packet stats is not supported for non-wwan backhaul\n");
		return;
	}

	hi = ecm_db_hosts_get_and_ref_first();
	while(hi) {
		struct ecm_db_host_instance *hin;
		hash_entry_exists = false;
		ECM_IP_ADDR_COPY(host_ip, hi->address);
		ecm_ip_addr_to_string(host_ip_addr, host_ip);
		host_rx = hi->rx_routed_bytes[ECM_DB_IFACE_TYPE_RAWIP];
		host_tx = hi->tx_routed_bytes[ECM_DB_IFACE_TYPE_RAWIP];

		if(host_rx == 0 && host_tx == 0) {
			DEBUG_TRACE("host stats are zero for ip=%s \n", host_ip_addr);
			goto next_hi_instance;
		}

		/*
		 * Get the ip address of the host and check if it already present in the list
		 * if it is then just update the corresponding rx and tx bytes
		 */
		ecm_sdx_stats_conn_hash(key, host_ip);
		DEBUG_TRACE("update host stats for host_ip=%s\n", host_ip_addr);
		spin_lock_bh(&ecm_sdx_stats_db_lock);
		hash_for_each_possible(ecm_sdx_stats_htable, curr, ecm_packet_hash_list, key) {
			if(*(curr->host_instance.client_src_addr) == *(host_ip)) {
				prev_rx = curr->host_instance.prev_stats_rx_byte_cout;
				prev_tx = curr->host_instance.prev_stats_tx_byte_cout;
				prev_host_rx = curr->host_instance.prev_host_rx_byte_count;
				prev_host_tx = curr->host_instance.prev_host_tx_byte_count;
				rx_bytes = host_rx;
				DEBUG_TRACE("host_ip=%s, ecm_rx=%llu, prev_rx=%llu, host_rx=%llu\n",
						host_ip_addr, rx_bytes, prev_rx, prev_host_rx);
				rx_bytes = prev_host_rx + rx_bytes - prev_rx;
				tx_bytes = host_tx;
				DEBUG_TRACE("host_ip=%s, ecm_tx=%llu, prev_tx=%llu, host_tx=%llu\n",
						host_ip_addr, tx_bytes, prev_tx, prev_host_tx);
				tx_bytes = prev_host_tx + tx_bytes - prev_tx;
				curr->host_instance.packet_stat_rx_byte_count = rx_bytes;
				curr->host_instance.packet_stat_tx_byte_count = tx_bytes;
				hash_entry_exists = true;
			}
		}
		spin_unlock_bh(&ecm_sdx_stats_db_lock);

		/*
		 * if it is not present then insert the new host entry to our hash list
		 */
		if(!hash_entry_exists) {
			ecm_sdx_stats_insert_host_stats(host_ip, host_rx, host_tx, false);
		}

next_hi_instance:
		hin = ecm_db_host_get_and_ref_next(hi);
		ecm_db_host_deref(hi);
		hi = hin;
	}
}

/*
 * ecm_sdx_stats_update_host_stats()
 *	Invoked when a connection is removed from the DB.
 */
void ecm_sdx_stats_update_host_stats(ip_addr_t host_ip, uint64_t host_rx,
			uint64_t host_tx, bool host_removed)
{
	struct ecm_sdx_stats_list *curr = NULL;
	bool hash_entry_exists = false;
	char ip_addr[ECM_IP_ADDR_STR_BUFF_SIZE] = {0};
	uint64_t rx_bytes = 0;
	uint64_t tx_bytes = 0;
	uint64_t prev_rx = 0;
	uint64_t prev_tx = 0;
	uint64_t prev_host_rx = 0;
	uint64_t prev_host_tx = 0;
	u32 key;

	if(ecm_sdx_stats_instance->packet_stats_iface_type != ECM_DB_IFACE_TYPE_RAWIP) {
		DEBUG_ERROR("Packet stats is not supported for non-wwan backhaul\n");
		return;
	}

	ecm_ip_addr_to_string(ip_addr, host_ip);
	DEBUG_TRACE("ecm_sdx_stats_update_host_stats for ip_addr=%s host_removed=%d \n",
			ip_addr, host_removed);

	/*
	 * Get the ip address of the host and check if it already present in the list
	 * if it is then just update the corresponding rx and tx bytes
	 */
	spin_lock_bh(&ecm_sdx_stats_db_lock);
	ecm_sdx_stats_conn_hash(key, host_ip);
	hash_for_each_possible(ecm_sdx_stats_htable, curr, ecm_packet_hash_list, key) {
		if(*(curr->host_instance.client_src_addr) == *(host_ip)) {
			prev_rx = curr->host_instance.prev_stats_rx_byte_cout;
			prev_tx = curr->host_instance.prev_stats_tx_byte_cout;
			prev_host_rx = curr->host_instance.prev_host_rx_byte_count;
			prev_host_tx = curr->host_instance.prev_host_tx_byte_count;
			rx_bytes = host_rx;
			DEBUG_TRACE("host_ip=%s, ecm_rx=%llu, prev_rx=%llu, host_rx=%llu\n",
					ip_addr, rx_bytes, prev_rx, prev_host_rx);
			rx_bytes = prev_host_rx + rx_bytes - prev_rx;
			tx_bytes = host_tx;
			DEBUG_TRACE("host_ip=%s, ecm_tx=%llu, prev_tx=%llu, host_tx=%llu\n",
					ip_addr, tx_bytes, prev_tx, prev_host_tx);
			tx_bytes = prev_host_tx + tx_bytes - prev_tx;
			curr->host_instance.packet_stat_rx_byte_count = rx_bytes;
			curr->host_instance.packet_stat_tx_byte_count = tx_bytes;
			hash_entry_exists = true;

			/*
			 * This is required in scenarios where host is deleted from ecm db
			 * due to no connections,but still host is in connected state.
			 */
			if(host_removed) {
				curr->host_instance.prev_host_rx_byte_count = rx_bytes;
				curr->host_instance.prev_host_tx_byte_count = tx_bytes;
				curr->host_instance.prev_stats_rx_byte_cout = 0;
				curr->host_instance.prev_stats_tx_byte_cout = 0;
			}
			break;
		}
	}
	spin_unlock_bh(&ecm_sdx_stats_db_lock);

	/*
	 * if it is not present then insert the new host entry to our hash list
	 */
	if(!hash_entry_exists) {
		ecm_sdx_stats_insert_host_stats(host_ip, host_rx, host_tx, host_removed);
	}
}

/*
 * ecm_sdx_stats_update_per_ip_stats()
 *	Invoked to update stats for particular ip.
 */
void ecm_sdx_stats_update_per_ip_stats(void)
{
	struct ecm_db_host_instance *hi, *hin;

	hi = ecm_db_hosts_get_and_ref_first();
	while(hi) {
		if(*(ecm_sdx_stats_instance->per_ip_address) == *(hi->address)) {
			ecm_sdx_stats_update_host_stats(hi->address,
					hi->rx_routed_bytes[ECM_DB_IFACE_TYPE_RAWIP],
					hi->tx_routed_bytes[ECM_DB_IFACE_TYPE_RAWIP],
					false);
			ecm_db_host_deref(hi);
			break;
		}

		hin = ecm_db_host_get_and_ref_next(hi);
		ecm_db_host_deref(hi);
		hi = hin;
	}
}

/*
 * ecm_sdx_stats_handler_read()
 *	Proc handler function for packet stats read operation.
 */
int ecm_sdx_stats_handler_read(void *buffer, size_t *lenp, loff_t *ppos)
{
	struct ecm_sdx_stats_list *curr = NULL;
	struct hlist_node *tmp = NULL;
	char *read_buf;
	char sip_addr[ECM_IP_ADDR_STR_BUFF_SIZE] = {0};
	uint64_t rx_bytes = 0;
	uint64_t tx_bytes = 0;
	uint8_t num_connections_v4 = 0;
	uint8_t num_connections_v6 = 0;
	uint32_t ip6[4];
	int bkt = 0;
	int bytes = 0;
	int len;

	read_buf = kzalloc(ECM_SDX_STATS_HASH_SIZE * ECM_SDX_STATS_MSG_SIZE * sizeof(char),
					GFP_KERNEL);
	if(!read_buf) {
		DEBUG_ERROR("read_buf memory allocation failed \n");
		return -ENOMEM;
	}

	memset(read_buf, 0, ECM_SDX_STATS_HASH_SIZE * ECM_SDX_STATS_MSG_SIZE * sizeof(char));
	num_connections_v4 = ecm_sdx_stats_instance->num_of_pack_stat_nodes_v4;
	num_connections_v6 = ecm_sdx_stats_instance->num_of_pack_stat_nodes_v6;
	len = snprintf(read_buf, ECM_SDX_STATS_MSG_SIZE,
			"<?xml version = '1.0' encoding = 'UTF-8'?>\n");
	bytes += len;
	len = snprintf(read_buf + bytes, ECM_SDX_STATS_MSG_SIZE, "\t<packet_stats>\n");
	bytes += len;
	len = snprintf(read_buf + bytes, ECM_SDX_STATS_MSG_SIZE, "\t\t<stats "
			"num_connections_v4=\"%u\" num_connections_v6=\"%u\" />\n",
			num_connections_v4,
			num_connections_v6);
	bytes += len;
	len = snprintf(read_buf + bytes, ECM_SDX_STATS_MSG_SIZE, "\t\t<connections>\n");
	bytes += len;

	if (num_connections_v4 == 0 && num_connections_v6 == 0) {
		DEBUG_TRACE("no v4 and v6 connections in db \n");
		goto no_connections;
	}

	hash_for_each_safe(ecm_sdx_stats_htable, bkt, tmp, curr,
		ecm_packet_hash_list) {
		if (curr != NULL) {
			ecm_ip_addr_to_string(sip_addr, curr->host_instance.client_src_addr);
			DEBUG_TRACE("client_sip_addr=%s \n", sip_addr);
			rx_bytes = curr->host_instance.packet_stat_rx_byte_count;
			tx_bytes = curr->host_instance.packet_stat_tx_byte_count;
			if (ECM_IP_ADDR_IS_V4(curr->host_instance.client_src_addr)) {
				len = snprintf(read_buf + bytes, ECM_SDX_STATS_MSG_SIZE,
				"\t\t\t<ipv4 "
				"client_addr=\"%pI4\" "
				"rx_bytes=\"%llu\" tx_bytes=\"%llu\" />\n",
				&curr->host_instance.client_src_addr,
				rx_bytes, tx_bytes);
			} else {
				ECM_IP_ADDR_TO_NET_IPV6_ADDR(ip6,
					curr->host_instance.client_src_addr);
				len = snprintf(read_buf + bytes, ECM_SDX_STATS_MSG_SIZE,
				"\t\t\t<ipv6 "
				"client_addr=\"%pI6\" "
				"rx_bytes=\"%llu\" tx_bytes=\"%llu\" />\n",
				ip6,
				rx_bytes, tx_bytes);
			}
			bytes += len;
		}
	}

no_connections:
	len = snprintf(read_buf + bytes, ECM_SDX_STATS_MSG_SIZE, "\t\t</connections>\n");
	bytes += len;
	len = snprintf(read_buf + bytes, ECM_SDX_STATS_MSG_SIZE, "\t</packet_stats>\n");
	bytes += len;

	bytes = memory_read_from_buffer(buffer, *lenp, ppos, read_buf, bytes);
	*lenp = bytes;

	kfree(read_buf);
	return 0;
}

/*
 * ecm_sdx_stats_handler_write()
 *	Proc handler function for packet stats write operation.
 */
int ecm_sdx_stats_handler_write(void *buffer, size_t *lenp)
{
	char *buf;
	char *pfree;
	char *token;
	int count;
	count = *lenp;

	buf = kzalloc(ECM_SDX_STATS_MSG_SIZE * sizeof(char), GFP_KERNEL);
	if (!buf) {
		DEBUG_ERROR("buf creation failed");
		return -ENOMEM;
	}

	pfree = buf;
	memcpy(buf, buffer, count);
	DEBUG_INFO("buffer=%s \n", buf);

	token = strsep(&buf, " ");
	if(!token) {
		DEBUG_ERROR("Null token passed\n");
		goto fail;
	}

	if (strncmp(token, "RESET_ALL", 9)) {
		DEBUG_ERROR("Invalid command passed \n");
		goto fail;
	}
	ecm_sdx_stats_reset_all_host_stats();

	kfree(pfree);
	return 0;
fail:
	kfree(pfree);
	return -EINVAL;
}

/*
 * ecm_sdx_stats_per_client_handler_read()
 *	Proc handler function for packet stats read operation.
 */
int ecm_sdx_stats_per_client_handler_read(void *buffer, size_t *lenp, loff_t *ppos)
 {
	struct ecm_sdx_stats_list *curr = NULL;
	int bytes = 0;
	int len;
	char *read_buf;
	char sip_addr[ECM_IP_ADDR_STR_BUFF_SIZE] = {0};
	uint64_t rx_bytes = 0;
	uint64_t tx_bytes = 0;
	u32 key;
	uint32_t ip6[4];

	read_buf = kmalloc(ECM_SDX_STATS_MSG_SIZE * sizeof(char), GFP_KERNEL);
	if(!read_buf) {
		DEBUG_ERROR("read_buf memory allocation failed");
		return -ENOMEM;
	}

	len = snprintf(read_buf, ECM_SDX_STATS_MSG_SIZE,
			"<?xml version = '1.0' encoding = 'UTF-8'?>\n");
	bytes += len;
	len = snprintf(read_buf + bytes, ECM_SDX_STATS_MSG_SIZE, "\t<packet_stats>\n");
	bytes += len;

	if(ECM_IP_ADDR_IS_V4(ecm_sdx_stats_instance->per_ip_address)) {
		len = snprintf(read_buf + bytes, ECM_SDX_STATS_MSG_SIZE, "\t\t<stats "
			"num_connections_v4=\"%u\" num_connections_v6=\"%u\" />\n", 1, 0);
		bytes += len;
	} else {
		len = snprintf(read_buf + bytes, ECM_SDX_STATS_MSG_SIZE, "\t\t<stats "
			"num_connections_v4=\"%u\" num_connections_v6=\"%u\" />\n", 0, 1);
		bytes += len;
	}

	len = snprintf(read_buf + bytes, ECM_SDX_STATS_MSG_SIZE, "\t\t<connections>\n");
	bytes += len;

	ecm_sdx_stats_conn_hash(key, ecm_sdx_stats_instance->per_ip_address);
	hash_for_each_possible(ecm_sdx_stats_htable, curr, ecm_packet_hash_list, key) {
		if (*(curr->host_instance.client_src_addr) ==
				*(ecm_sdx_stats_instance->per_ip_address)) {
			ecm_ip_addr_to_string(sip_addr, curr->host_instance.client_src_addr);
			DEBUG_TRACE("client addr=%s \n", sip_addr);
			rx_bytes = curr->host_instance.packet_stat_rx_byte_count;
			tx_bytes = curr->host_instance.packet_stat_tx_byte_count;
			if (ECM_IP_ADDR_IS_V4(curr->host_instance.client_src_addr)) {
				len = snprintf(read_buf + bytes, ECM_SDX_STATS_MSG_SIZE,
				"\t\t\t<ipv4 "
				"client_addr=\"%pI4\" "
				"rx_bytes=\"%llu\" tx_bytes=\"%llu\" />\n",
				&curr->host_instance.client_src_addr,
				rx_bytes, tx_bytes);
			} else {
				ECM_IP_ADDR_TO_NET_IPV6_ADDR(ip6,
					curr->host_instance.client_src_addr);
				len = snprintf(read_buf + bytes, ECM_SDX_STATS_MSG_SIZE,
				"\t\t\t<ipv6 "
				"client_addr=\"%pI6\" "
				"rx_bytes=\"%llu\" tx_bytes=\"%llu\" />\n",
				ip6,
				rx_bytes, tx_bytes);
			}

			bytes += len;
			break;
		}
	}

	len = snprintf(read_buf + bytes, ECM_SDX_STATS_MSG_SIZE, "\t\t</connections>\n");
	bytes += len;
	len = snprintf(read_buf + bytes, ECM_SDX_STATS_MSG_SIZE, "\t</packet_stats>\n");
	bytes += len;

	DEBUG_TRACE("Dump packet stats\n");
	bytes = memory_read_from_buffer(buffer, *lenp, ppos, read_buf, bytes);
	*lenp = bytes;

	kfree(read_buf);
	return 0;
}

/*
 * ecm_sdx_stats_per_client_handler_write()
 *	Proc handler function for packet stats write operation.
 */
int ecm_sdx_stats_per_client_handler_write(void *buffer, size_t *lenp)
 {
	char *buf;
	char *pfree;
	char *token;
	int count;
	ip_addr_t ip_address;

	count = *lenp;
	buf = kzalloc(ECM_SDX_STATS_MSG_SIZE * sizeof(char), GFP_KERNEL);
	if (!buf) {
		 DEBUG_ERROR("buf creation failed");
		 return -ENOMEM;
	}

	pfree = buf;
	memcpy(buf, buffer, count);
	DEBUG_TRACE("per_client write_command_passed=%s", buf);

	token = strsep(&buf, " ");
	if(!token) {
		DEBUG_ERROR("Null token passed\n");
		goto fail;
	}

	if (!strncmp(token, "PER_IP_STATS", 12)) {
		DEBUG_INFO("update per_ip_address invoked \n");
		token = strsep(&buf, " ");
		if(!token || !ecm_string_to_ip_addr(ecm_sdx_stats_instance->per_ip_address,token)) {
			DEBUG_ERROR("null token or invalid ip passed \n");
			goto fail;
		}
	} else if (!strncmp(token, "RESET", 5)) {
		DEBUG_INFO("Reset per ip is invoked \n");
		token = strsep(&buf, " ");
		if(!token || !ecm_string_to_ip_addr(ip_address, token)) {
			DEBUG_ERROR("null token or invalid ip passed \n");
			goto fail;
		}

		ecm_sdx_stats_reset_host_stats(ip_address);
	}  else if (!strncmp(token, "DELETE", 6)) {
		token = strsep(&buf, " ");
		if(!token || !ecm_string_to_ip_addr(ip_address, token)) {
			DEBUG_ERROR("null token or invalid ip passed \n");
			goto fail;
		}

		ecm_sdx_stats_remove_host_stats(ip_address);
	} else {
		DEBUG_ERROR("Invalid command passed \n");
		goto fail;
	}

	kfree(pfree);
	return 0;
fail:
	kfree(pfree);
	return -EINVAL;
}
/*
* ecm_sdx_stats_interface_type_handler_read()
*	Proc handler function for packet stats interface_type read operation.
*/
int ecm_sdx_stats_interface_type_handler_read(void *buffer, size_t *lenp, loff_t *ppos)
{
	int bytes = 0;
	int len;
	char *read_buf;
	ecm_db_iface_type_t iface_type;

	read_buf = kmalloc(ECM_SDX_STATS_MAX_INTERFACE_SIZE * sizeof(char), GFP_KERNEL);
	if(!read_buf) {
		DEBUG_ERROR("read_buf memory allocation failed");
		return -ENOMEM;
	}

	iface_type = ecm_sdx_stats_instance->packet_stats_iface_type;
	len = scnprintf(read_buf + bytes, ECM_SDX_STATS_MSG_SIZE, "%d ", iface_type);
	bytes += len;
	len = scnprintf(read_buf + bytes, ECM_SDX_STATS_MSG_SIZE, "\n");
	bytes += len;

	DEBUG_TRACE("Dump packet_stats interface type\n");
	bytes = memory_read_from_buffer(buffer, *lenp, ppos, read_buf, bytes);
	*lenp = bytes;

	kfree(read_buf);
	return 0;
}

/*
 * ecm_sdx_stats_interface_type_handler_write()
 *	Proc handler function for write operation on packet_stats_interface.
 */
int ecm_sdx_stats_interface_type_handler_write(void *buffer, size_t *lenp)
{
	char *buf;
	char *pfree;
	char *token;
	int count;
	ip_addr_t temp_ip = ECM_IP_ADDR_NULL;

	count = *lenp;
	buf = kzalloc(ECM_SDX_STATS_MAX_INTERFACE_SIZE * sizeof(char), GFP_KERNEL);
	if (!buf) {
		 DEBUG_ERROR("buf creation failed");
		 return -ENOMEM;
	}

	pfree = buf;
	memcpy(buf, buffer, count);
	DEBUG_TRACE("interface command passed=%s", buf);

	token = strsep(&buf, " ");
	if (token && !strncmp(token, "bh_type", 7)) {
		DEBUG_INFO("Reset per ip is invoked \n");

		token = strsep(&buf, " ");
		if(!token) {
			DEBUG_ERROR("Null token passed\n");
			goto fail;
		}

		if(!strncmp(token, "wwan", 4)) {
			ecm_sdx_stats_instance->packet_stats_iface_type = ECM_DB_IFACE_TYPE_RAWIP;
			DEBUG_TRACE("iface_type=%d \n",
					ecm_sdx_stats_instance->packet_stats_iface_type);
		} else if(!strncmp(token, "NULL", 4)) {
			ecm_sdx_stats_instance->packet_stats_iface_type = ECM_DB_IFACE_TYPE_COUNT;
			DEBUG_TRACE("iface_type=%d \n",
					ecm_sdx_stats_instance->packet_stats_iface_type);
			ECM_IP_ADDR_COPY(ecm_sdx_stats_instance->v4_prefix.ip_addr, temp_ip);
			ECM_IP_ADDR_COPY(ecm_sdx_stats_instance->v6_prefix.ip_addr, temp_ip);
			ECM_IP_ADDR_COPY(ecm_sdx_stats_instance->ippt_v4_prefix.ip_addr, temp_ip);
		} else {
			goto fail;
		}
	} else {
		goto fail;
	}

	kfree(pfree);
	return 0;
fail:
	DEBUG_ERROR("Invalid command passed \n");
	kfree(pfree);
	return -EINVAL;
}

/*
 * ecm_sdx_stats_update_lan_prefix()
 *	Proc handler function for packet_stats_interface read/write operation.
 */
static void ecm_sdx_stats_update_lan_prefix(ip_addr_t ip, uint32_t prefix,
			bool ipv4, bool ippt_enabled)
{
	char ip_addr[ECM_IP_ADDR_STR_BUFF_SIZE] = {0};

	if (ipv4) {
		if(ippt_enabled) {
			ECM_IP_ADDR_COPY(ecm_sdx_stats_instance->ippt_v4_prefix.ip_addr, ip);
			ecm_sdx_stats_instance->ippt_v4_prefix.prefix = prefix;
			ecm_ip_addr_to_string(ip_addr,
					ecm_sdx_stats_instance->ippt_v4_prefix.ip_addr);
			DEBUG_INFO("ip=%s, subnet=%u\n",
				ip_addr,
				ecm_sdx_stats_instance->ippt_v4_prefix.prefix);
		} else {
			ECM_IP_ADDR_COPY(ecm_sdx_stats_instance->v4_prefix.ip_addr, ip);
			ecm_sdx_stats_instance->v4_prefix.prefix = prefix;
			ecm_ip_addr_to_string(ip_addr,ecm_sdx_stats_instance->v4_prefix.ip_addr);
			DEBUG_INFO("ip=%s, subnet=%u\n",
				ip_addr,
				ecm_sdx_stats_instance->v4_prefix.prefix);
		}

	} else {
		ECM_IP_ADDR_COPY(ecm_sdx_stats_instance->v6_prefix.ip_addr, ip);
		ecm_sdx_stats_instance->v6_prefix.prefix = prefix;
		ecm_ip_addr_to_string(ip_addr, ecm_sdx_stats_instance->v6_prefix.ip_addr);
		DEBUG_INFO("ip=%s, subnet=%u\n",
			ip_addr,
			ecm_sdx_stats_instance->v6_prefix.prefix);
	}
}

/*
 * ecm_sdx_stats_lan_prefix_handler_read()
 *	Proc handler function for packet stats interface_type read operation.
 */
int ecm_sdx_stats_lan_prefix_handler_read(void *buffer, size_t *lenp, loff_t *ppos)
{
	int bytes = 0;
	int len;
	char *read_buf;
	char ip_addr[ECM_IP_ADDR_STR_BUFF_SIZE] = {0};

	read_buf = kmalloc(ECM_SDX_STATS_MSG_SIZE * sizeof(char), GFP_KERNEL);
	if(!read_buf) {
		DEBUG_ERROR("read_buf memory allocation failed");
		return -ENOMEM;
	}

	len = scnprintf(read_buf + bytes, ECM_SDX_STATS_MSG_SIZE, "%s ", "ipv4");
	bytes += len;
	ecm_ip_addr_to_string(ip_addr, ecm_sdx_stats_instance->v4_prefix.ip_addr);
	len = scnprintf(read_buf + bytes, ECM_SDX_STATS_MSG_SIZE, "ip_addr=%s ", ip_addr);
	bytes += len;
	len = scnprintf(read_buf + bytes, ECM_SDX_STATS_MSG_SIZE, "prefix = %u ",
	ecm_sdx_stats_instance->v4_prefix.prefix);
	bytes += len;
	DEBUG_TRACE("ipv4 ip=%s, prefix=%u\n",
			ip_addr,
			ecm_sdx_stats_instance->v4_prefix.prefix);
	len = scnprintf(read_buf + bytes, ECM_SDX_STATS_MSG_SIZE, "\n");
	bytes += len;

	len = scnprintf(read_buf + bytes, ECM_SDX_STATS_MSG_SIZE, "%s ", "ipv4_ippt");
	bytes += len;
	ecm_ip_addr_to_string(ip_addr, ecm_sdx_stats_instance->ippt_v4_prefix.ip_addr);
	len = scnprintf(read_buf + bytes, ECM_SDX_STATS_MSG_SIZE, "ip_addr=%s ", ip_addr);
	bytes += len;
	len = scnprintf(read_buf + bytes, ECM_SDX_STATS_MSG_SIZE, "prefix = %u ",
	ecm_sdx_stats_instance->ippt_v4_prefix.prefix);
	bytes += len;
	DEBUG_TRACE("ipv4 ip=%s, prefix=%u\n",
			ip_addr,
			ecm_sdx_stats_instance->ippt_v4_prefix.prefix);
	len = scnprintf(read_buf + bytes, ECM_SDX_STATS_MSG_SIZE, "\n");
	bytes += len;

	len = scnprintf(read_buf + bytes, ECM_SDX_STATS_MSG_SIZE, "%s ", "ipv6");
	bytes += len;
	ecm_ip_addr_to_string(ip_addr, ecm_sdx_stats_instance->v6_prefix.ip_addr);
	len = scnprintf(read_buf + bytes, ECM_SDX_STATS_MSG_SIZE, "ip_addr=%s ", ip_addr);
	bytes += len;
	len = scnprintf(read_buf + bytes, ECM_SDX_STATS_MSG_SIZE, "prefix = %u ",
			ecm_sdx_stats_instance->v6_prefix.prefix);
	bytes += len;
	DEBUG_TRACE("ipv6 ip=%s, prefix=%u\n",
		ip_addr,
		ecm_sdx_stats_instance->v6_prefix.prefix);
	len = scnprintf(read_buf + bytes, ECM_SDX_STATS_MSG_SIZE, "\n");
	bytes += len;

	bytes = memory_read_from_buffer(buffer, *lenp, ppos, read_buf, bytes);
	*lenp = bytes;

	kfree(read_buf);
	return 0;
}

/*
 * ecm_sdx_stats_lan_prefix_handler_write()
 *	Proc handler function for packet_stats_lan_subnet write operation.
 */
int ecm_sdx_stats_lan_prefix_handler_write(void *buffer, size_t *lenp)
{
	char *buf;
	char *pfree;
	char *token;
	int count;
	int mask_value;
	ip_addr_t ip_addr;
	ip_addr_t tmp_subnet;
	uint32_t subnet;

	count = *lenp;
	buf = kzalloc(ECM_SDX_STATS_MSG_SIZE * sizeof(char), GFP_KERNEL);
	if (!buf) {
		 DEBUG_ERROR("buf creation failed");
		 return -ENOMEM;
	}

	pfree = buf;
	memcpy(buf, buffer, count);
	DEBUG_TRACE("lan_prefix buf=%s", buf);

	token = strsep(&buf, " ");
	if(!token) {
		DEBUG_ERROR("Null token passed\n");
		goto fail;
	}

	if (!strncmp(token, "ipv4", 4)) {
		DEBUG_TRACE("update ipv4 lan subnet to db\n");
		token = strsep(&buf, " ");
		if(!token || !ecm_string_to_ip_addr(ip_addr, token)) {
			DEBUG_ERROR("null token or invalid ip passed \n");
			goto fail;
		}

		token = strsep(&buf, " ");
		if(!token || !ecm_string_to_ip_addr(tmp_subnet, token)) {
			DEBUG_ERROR("null token or invalid ip passed \n");
			goto fail;
		}

		subnet = *tmp_subnet;
		ecm_sdx_stats_update_lan_prefix(ip_addr, subnet, true, false);
	} else if (!strncmp(token, "ipv6", 4)) {
		DEBUG_TRACE("update ipv6 lan subnet to db\n");
		token = strsep(&buf, "/");
		if(!token || !ecm_string_to_ip_addr(ip_addr, token)) {
			DEBUG_ERROR("null token or invalid ip passed \n");
			goto fail;
		}

		token = strsep(&buf, " ");
		if(!token || sscanf(token, "%d", &mask_value) != 1) {
			DEBUG_ERROR("error in parsing token");
			goto fail;
		}

		subnet = mask_value;
		ecm_sdx_stats_update_lan_prefix(ip_addr, subnet, false, false);
	} else if (!strncmp(token, "ippt", 4)) {
		DEBUG_TRACE ("update ippt lan subnet to db\n");
		token = strsep(&buf, " ");
		if(!token || !ecm_string_to_ip_addr(ip_addr, token)) {
			DEBUG_ERROR("null token or invalid ip passed \n");
			goto fail;
		}

		token = strsep(&buf, " ");
		if(!token || !ecm_string_to_ip_addr(tmp_subnet, token)) {
			DEBUG_ERROR("null token or invalid ip passed \n");
			goto fail;
		}

		subnet = *tmp_subnet;
		ecm_sdx_stats_update_lan_prefix(ip_addr, subnet, true, true);
	} else {
		DEBUG_ERROR("Invalid command passed \n");
		goto fail;
	}

	kfree(pfree);
	return 0;
fail:
	kfree(pfree);
	return -EINVAL;
}

/*
 * ecm_sdx_stats_init()
 *	Init function for ecm_stats.
 */
int ecm_sdx_stats_init(struct dentry *dentry)
{
	printk(KERN_INFO "ecm_sdx_stats_init\n");

	ecm_sdx_stats_instance = (struct ecm_sdx_stats_info_instance *)
	                kzalloc(sizeof(struct ecm_sdx_stats_info_instance), GFP_KERNEL);
	if (unlikely(!ecm_sdx_stats_instance)) {
		DEBUG_ERROR("Failed to allocate memory for packet_instance node\n");
		return -ENOMEM;
	}

	hash_init(ecm_sdx_stats_htable);

	return 0;
}

/*
 * ecm_sdx_stats_exit()
 *	Exit function for ecm_stats.
 */
void ecm_sdx_stats_exit(void)
{
	printk(KERN_INFO "ecm_sdx_stats_exit\n");

	if(ecm_sdx_stats_instance) {
		kfree(ecm_sdx_stats_instance);
	}
}
