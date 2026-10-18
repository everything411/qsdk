/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: ISC
 */

#include "fls_conn.h"
#include "fls_debug.h"

struct fls_conn_tracker fct;
s64 fls_conn_timeout = 200;

static struct kmem_cache *fls_conn_cache;

/*
 * fls_conn_get_connection_hash()
 *	Computes a hash for the connection based on IP version,
 *	protocol, IP addresses, and ports.
 */
static inline uint32_t fls_conn_get_connection_hash(uint8_t ip_version, uint8_t protocol, uint32_t *src_ip, uint16_t src_port, uint32_t *dest_ip, uint16_t dest_port)
{
	uint32_t hash = 0;
	uint32_t i;

	if (ip_version == 6) {
		for (i = 0; i < 4; i++) {
			hash ^= src_ip[i] ^ dest_ip[i];
		}
	} else {
		hash = *src_ip ^ *dest_ip;
	}

	hash ^= protocol ^ src_port ^ dest_port;
	return ((hash >> FLS_CONN_HASH_SHIFT) ^ hash) & FLS_CONN_HASH_MASK;
}

/*
 * fls_conn_matches()
 *	Checks if a connection matches the given 5-tuple (IPv4/IPv6,
 *	protocol, src/dest IPs, and ports).
 */
static inline bool fls_conn_matches(struct fls_conn *connection,
								uint8_t ip_version,
								uint8_t protocol,
								uint32_t *src_ip,
								uint16_t src_port,
								uint32_t *dest_ip,
								uint16_t dest_port)
{
	if (ip_version != connection->ip_version || protocol != connection->protocol) {
		return false;
	}

	if (ip_version == 4) {
		if (*(connection->src_ip) != *src_ip ||
			*(connection->dest_ip) != *dest_ip) {
			return false;
		}
	} else {
		if (connection->src_ip[0] != src_ip[0] ||
			connection->src_ip[1] != src_ip[1] ||
			connection->src_ip[2] != src_ip[2] ||
			connection->src_ip[3] != src_ip[3] ||
			connection->dest_ip[0] != dest_ip[0] ||
			connection->dest_ip[1] != dest_ip[1] ||
			connection->dest_ip[2] != dest_ip[2] ||
			connection->dest_ip[3] != dest_ip[3]) {
			return false;
		}
	}

	if (connection->src_port != src_port ||
		connection->dest_port != dest_port) {
		return false;
	}

	return true;
}

/*
 * fls_conn_create_flow()
 *	Allocates and initializes a connection, links it into global
 *	lists and hash bucket under lock, and returns the entry.
 *	TODO: There is a possibility that 2 flows with same 5-tuple can be created. Need more debug on this.
 */
struct fls_conn *fls_conn_create_flow(uint8_t ip_version,
								uint8_t protocol,
								uint32_t *src_ip,
								uint16_t src_port,
								uint32_t *dest_ip,
								uint16_t dest_port)
{
	struct fls_conn *connection;
	uint32_t hash;
	atomic_inc(&fct.fls_gbl_counters[FLS_GBL_CREATE_REQUESTS]);

	hash = fls_conn_get_connection_hash(ip_version, protocol, src_ip, src_port, dest_ip, dest_port);

	/*
	 * Allocate connection outside lock
	 */
	connection = kmem_cache_zalloc(fls_conn_cache, GFP_ATOMIC);
	if (!connection) {
		FLS_ERROR("Failed to allocate FLS connection (out of memory).\n");
		atomic_inc(&fct.fls_gbl_exception_counters[FLS_GBL_EXCEPTION_MEM_ALLOC_FAIL]);
		return NULL;
	}

	spin_lock_bh(&fct.lock);

	/*
	 * Check capacity under lock
	 */
	if (unlikely(fct.num_connections >= fct.max_connections)) {
		spin_unlock_bh(&fct.lock);
		kmem_cache_free(fls_conn_cache, connection);
		FLS_ERROR("FLS connection limit (%u) reached (allocation denied).\n", fct.max_connections);
		atomic_inc(&fct.fls_gbl_exception_counters[FLS_GBL_EXCEPTION_MAX_CONN_LIMIT]);
		return NULL;
	}

	/*
	 * Initialize connection fields
	 */
	if (ip_version == 6) {
		connection->src_ip[0] = src_ip[0];
		connection->src_ip[1] = src_ip[1];
		connection->src_ip[2] = src_ip[2];
		connection->src_ip[3] = src_ip[3];
		connection->dest_ip[0] = dest_ip[0];
		connection->dest_ip[1] = dest_ip[1];
		connection->dest_ip[2] = dest_ip[2];
		connection->dest_ip[3] = dest_ip[3];

	} else {
		connection->src_ip[0] = src_ip[0];
		connection->src_ip[1] = 0;
		connection->src_ip[2] = 0;
		connection->src_ip[3] = 0;
		connection->dest_ip[0] = dest_ip[0];
		connection->dest_ip[1] = 0;
		connection->dest_ip[2] = 0;
		connection->dest_ip[3] = 0;
	}

	connection->ip_version = ip_version;
	connection->protocol = protocol;
	connection->src_port = src_port;
	connection->dest_port = dest_port;
	connection->hash = hash;
	connection->flags = SFE_FLS_CONNECTION_FLAG_DEF_ENABLE;
	connection->traffic_class = 0xFF;

	connection->all_next = fct.all_connections_head;
	if (fct.all_connections_head) {
		fct.all_connections_head->all_prev = connection;
	}

	fct.all_connections_head = connection;

	if (!fct.all_connections_tail) {
		fct.all_connections_tail = connection;
	}

	/*
	 * Insert into hash bucket
	 */
	connection->hash_next = fct.hash[hash];
	if (fct.hash[hash]) {
		fct.hash[hash]->hash_prev = connection;
	}
	fct.hash[hash] = connection;

	/*
	 * Count increment happens after successful link
	 */
	fct.num_connections++;
	FLS_WARN("Created fls_conn: %p ip_version=%u, protocol=%u, src_ip=%pI4, src_port=%u, dest_ip=%pI4, dest_port=%u, current connections: %u\n",
			connection, ip_version, protocol, src_ip, ntohs(src_port), dest_ip, ntohs(dest_port), fct.num_connections);
	spin_unlock_bh(&fct.lock);
	atomic_inc(&fct.fls_gbl_counters[FLS_GBL_ACTIVE_COUNT]);

	return connection;
}

/*
 * fls_conn_stats_update()
 *	Updates connection stats using sensor manager callbacks
 *	and returns current connection flags.
 */
uint8_t fls_conn_stats_update(void *connection, struct sk_buff *skb)
{
	struct fls_conn *conn = (struct fls_conn *)connection;

	fls_sensor_manager_call_all(&fct.fsm, conn, skb);
	return conn->flags;
}
EXPORT_SYMBOL(fls_conn_stats_update);

/*
 * fls_conn_lookup()
 *	Finds a connection in the hash table by 5-tuple; promotes it to
 *	the bucket head on hit (move-to-front) and returns the entry.
 */
struct fls_conn *fls_conn_lookup(uint8_t ip_version,
								uint8_t protocol,
								uint32_t *src_ip,
								uint16_t src_port,
								uint32_t *dest_ip,
								uint16_t dest_port)
{
	uint32_t hash = fls_conn_get_connection_hash(ip_version, protocol, src_ip, src_port, dest_ip, dest_port);
	struct fls_conn *connection;
	struct fls_conn *hash_head;

	spin_lock_bh(&fct.lock);
	connection = fct.hash[hash];
	hash_head = connection;

	while (connection) {
		if (fls_conn_matches(connection, ip_version, protocol, src_ip, src_port, dest_ip, dest_port)) {
			if(connection == hash_head) {
				spin_unlock_bh(&fct.lock);
				return connection;
			}

			connection->hash_prev->hash_next = connection->hash_next;
			if(connection->hash_next)
				connection->hash_next->hash_prev = connection->hash_prev;
			connection->hash_prev = NULL;
			connection->hash_next = hash_head;
			hash_head->hash_prev = connection;
			fct.hash[hash] = connection;
			spin_unlock_bh(&fct.lock);
			return connection;
		}
		connection = connection->hash_next;
	}

	spin_unlock_bh(&fct.lock);
	return NULL;
}
EXPORT_SYMBOL(fls_conn_lookup);

/*
 * fls_conn_free_cmn()
 *	Free all dynamically allocated resources in fls_conn_cmn :
 *	release timers, clear back-references, and free the common struct.
 */
static void fls_conn_free_cmn(struct fls_conn_cmn *cmn)
{
	if (!cmn) {
		return;
	}

	/*
	 * Ensure timers are deleted before freeing memory
	 */
	if (cmn->timers) {
		if (cmn->timers->delay_timer && cmn->timers->delay_timer->timer.function) {
			hrtimer_cancel(&cmn->timers->delay_timer->timer);
			kfree(cmn->timers->delay_timer);
		}

		if (cmn->timers->window_timer && cmn->timers->window_timer->timer.function) {
			hrtimer_cancel(&cmn->timers->window_timer->timer);
			kfree(cmn->timers->window_timer);
		}

		if (cmn->timers->xl_xxl_timer && cmn->timers->xl_xxl_timer->timer.function) {
			hrtimer_cancel(&cmn->timers->xl_xxl_timer->timer);
			kfree(cmn->timers->xl_xxl_timer);
		}

		kfree(cmn->timers);
	}

	if(cmn->orig) {
		cmn->orig->cmn = NULL;
	}

	if(cmn->reply) {
		cmn->reply->cmn = NULL;
	}

	kfree(cmn);
}

/*
 * fls_conn_delete_internal()
 *	Removes a connection from lists and hash table, updates counters,
 *	and frees memory. Also clears reverse link and common resources.
 */
static void fls_conn_delete_internal(void *conn)
{
	struct fls_conn *connection = (struct fls_conn *)conn;
	struct fls_conn *reply;

	if (!connection) {
		FLS_WARN("fls_conn_delete_internal called with null connections");
		return;
	}

	reply = connection->reverse;

	FLS_TRACE("Deleting fls_conn: \n");
	fls_debug_print_conn_info(conn);

	if (connection->cmn) {
		fls_def_sensor_timer_delete(connection);
		fls_conn_free_cmn(connection->cmn);
	}

	if (reply) {
		reply->reverse = NULL;
	}

	/*
	 * Unlink from all_connections list
	 */
	if (connection->all_prev) {
		connection->all_prev->all_next = connection->all_next;
	} else {
		fct.all_connections_head = connection->all_next;
	}

	if (connection->all_next) {
		connection->all_next->all_prev = connection->all_prev;
	} else {
		fct.all_connections_tail = connection->all_prev;
	}

	/*
	 * Unlink from hash bucket
	 */
	if (connection->hash_prev) {
		connection->hash_prev->hash_next = connection->hash_next;
	} else {
		fct.hash[connection->hash] = connection->hash_next;
	}

	if (connection->hash_next) {
		connection->hash_next->hash_prev = connection->hash_prev;
	}

	fct.num_connections--;
	FLS_WARN("Deleting fls_conn: %p ip_version=%u, protocol=%u, src_ip=%pI4, src_port=%u, dest_ip=%pI4, dest_port=%u, current connections: %u\n",
			connection, connection->ip_version, connection->protocol, connection->src_ip, ntohs(connection->src_port), connection->dest_ip, ntohs(connection->dest_port), fct.num_connections);
	atomic_dec(&fct.fls_gbl_counters[FLS_GBL_ACTIVE_COUNT]);

	/*
	 * Free after list/hash removal
	 */
	kmem_cache_free(fls_conn_cache, connection);
}

/*
 * fls_conn_flush()
 *	Flushes all existing connections from the FLS connection tracker
 *	by iterating through the connection list and deleting each entry.
 *	Ensures proper cleanup while holding the tracker lock.
 */
void fls_conn_flush() {
	struct fls_conn *conn;
	struct fls_conn *next;

	FLS_TRACE("flush all connections\n");
	spin_lock_bh(&fct.lock);
	conn = fct.all_connections_head;
	while(conn) {
		next = conn->all_next;
		FLS_TRACE("FID: Deleting connections (FLUSH).");
		fls_conn_delete_internal(conn);
		conn = next;
	}
	spin_unlock_bh(&fct.lock);
}

/*
 * fls_conn_delete()
 *	Delete one connection.
 */
void fls_conn_delete(void *conn)
{
	FLS_INFO("FID: Deleting connection (CONN_DELETE).");
	atomic_inc(&fct.fls_gbl_counters[FLS_GBL_DELETE_REQUESTS]);
	spin_lock_bh(&fct.lock);
	fls_conn_delete_internal(conn);
	spin_unlock_bh(&fct.lock);
}
EXPORT_SYMBOL(fls_conn_delete);

/*
 * fls_conn_delete_timeout()
 *	Delete all timeout connection.
 *	TODO : 1. This function gets called only when flsp is running.
 *			Return true if atleast one connection is deleted.
 *			2. Connection lookup should happen within spin_lock .
 *			3. Add error check for all_connections_head being NULL.
 */
bool fls_conn_delete_timeout(ktime_t now, s64 threshold) {
	struct fls_conn *cur = fct.all_connections_head;
	struct fls_conn *tmp;
	bool findtimeout = false;
	int32_t abs_diff;

	ktime_t oldest = cur->last_ts;
	cur = cur->all_next;
	while(cur) {
		if(!cur->externalrule)
			continue;
		tmp = cur->all_next;
		oldest = ktime_compare(oldest, cur->last_ts) == 1? cur->last_ts:oldest;
		abs_diff = ktime_to_ms(ktime_sub(now, cur->last_ts));
		if(abs_diff / 1000 > threshold) {
			findtimeout = true;
			spin_lock_bh(&(fct.lock));
			fls_conn_delete_internal(cur);
			spin_unlock_bh(&(fct.lock));
		}
		cur = tmp;
	}

	if(!findtimeout)
		FLS_ERROR("FID: Cannot find old enough connections for reply, \
				Oldest one = %ld \
				(Try increase timeout value \
				 echo xx(seconds) > /proc/sys/net/fls/conn_timeout\n", oldest);

	return false;
}

/*
 * fls_conn_alloc_cmn
 *	Dynamically allocates timers for each connection
 */
struct fls_conn_cmn *fls_conn_alloc_cmn(void)
{
	struct fls_conn_cmn *cmn;

	cmn = kmalloc(sizeof(struct fls_conn_cmn), GFP_ATOMIC);
	if (!cmn) {
		FLS_WARN("failed to alloc common stats\n");
		atomic_inc(&fct.fls_gbl_exception_counters[FLS_GBL_EXCEPTION_MEM_ALLOC_FAIL]);
		return NULL;
	}

	cmn->timers = kmalloc(sizeof(struct fls_def_sensor_timers), GFP_ATOMIC);
	if (!cmn->timers) {
		FLS_WARN("failed to alloc timers struct\n");
		atomic_inc(&fct.fls_gbl_exception_counters[FLS_GBL_EXCEPTION_MEM_ALLOC_FAIL]);
		goto cmn_free;
	}

	cmn->timers->delay_timer = kmalloc(sizeof(struct fls_def_sensor_timer_data), GFP_ATOMIC);
	if (!cmn->timers->delay_timer) {
		FLS_WARN("failed to alloc delay timer struct\n");
		atomic_inc(&fct.fls_gbl_exception_counters[FLS_GBL_EXCEPTION_MEM_ALLOC_FAIL]);
		goto timers_free;
	}
	cmn->timers->delay_timer->cmn = cmn;
	cmn->timers->delay_timer->timer.function = NULL;

	cmn->timers->window_timer = kmalloc(sizeof(struct fls_def_sensor_timer_data), GFP_ATOMIC);
	if (!cmn->timers->window_timer) {
		FLS_WARN("failed to alloc window timer struct\n");
		atomic_inc(&fct.fls_gbl_exception_counters[FLS_GBL_EXCEPTION_MEM_ALLOC_FAIL]);
		goto delay_timer_free;
	}
	cmn->timers->window_timer->cmn = cmn;
	cmn->timers->window_timer->timer.function = NULL;

	cmn->timers->xl_xxl_timer = kmalloc(sizeof(struct fls_def_sensor_timer_data), GFP_ATOMIC);
	if (!cmn->timers->xl_xxl_timer) {
		FLS_WARN("failed to alloc xl timer struct\n");
		atomic_inc(&fct.fls_gbl_exception_counters[FLS_GBL_EXCEPTION_MEM_ALLOC_FAIL]);
		goto window_timer_free;
	}
	cmn->timers->xl_xxl_timer->cmn = cmn;
	cmn->timers->xl_xxl_timer->timer.function = NULL;

	return cmn;

window_timer_free:
	kfree(cmn->timers->window_timer);
delay_timer_free:
	kfree(cmn->timers->delay_timer);
timers_free:
	kfree(cmn->timers);
cmn_free:
	kfree(cmn);
	return NULL;
}

/*
 * fls_conn_create_bidiflow()
 *	Creates a bidirectional flow in the connection database.
 */
struct fls_conn *fls_conn_create_bidiflow(uint8_t ip_version,
						uint8_t protocol,
						uint32_t *orig_src_ip,
						uint16_t orig_src_port,
						uint32_t *orig_dest_ip,
						uint16_t orig_dest_port,
						bool isexternal, ktime_t last_ts) {
	struct fls_conn *orig;
	struct fls_conn *reply;
	struct fls_conn_cmn *cmn;

	FLS_INFO("Creating fls_conn bidirectional flow: ip_version=%u, protocol=%u, orig_src_ip=%pI4, orig_src_port=%u, orig_dest_ip=%pI4, orig_dest_port=%u\n",
			ip_version, protocol, orig_src_ip, ntohs(orig_src_port), orig_dest_ip, ntohs(orig_dest_port));

	cmn = fls_conn_alloc_cmn();
	if (!cmn) {
		return NULL;
	}

	orig = fls_conn_create_flow(ip_version, protocol, orig_src_ip, orig_src_port, orig_dest_ip, orig_dest_port);
	if (!orig && !isexternal) {
		fls_conn_free_cmn(cmn);
		return NULL;
	}

	if (!orig) {
		if (fls_conn_delete_timeout(last_ts, fls_conn_timeout)) {
			orig = fls_conn_create_flow(ip_version, protocol, orig_src_ip, orig_src_port, orig_dest_ip, orig_dest_port);
			if (!orig) {
				fls_conn_free_cmn(cmn);
				return NULL;
			}
		} else {
			fls_conn_free_cmn(cmn);
			return NULL;
		}
	}

	orig->last_ts = last_ts;

	reply = fls_conn_create_flow(ip_version, protocol, orig_dest_ip, orig_dest_port, orig_src_ip, orig_src_port);
	if (!reply && !isexternal) {
		spin_lock_bh(&fct.lock);
		fls_conn_delete_internal(orig);
		fls_conn_free_cmn(cmn);
		spin_unlock_bh(&fct.lock);
		return NULL;
	}

	if (!reply) {
		if (fls_conn_delete_timeout(last_ts, fls_conn_timeout)) {
			reply = fls_conn_create_flow(ip_version, protocol, orig_dest_ip, orig_dest_port, orig_src_ip, orig_src_port);
			if (!reply) {
				FLS_ERROR("FLS create reply flow failed, now free common stats");
				spin_lock_bh(&fct.lock);
				fls_conn_delete_internal(orig);
				fls_conn_free_cmn(cmn);
				spin_unlock_bh(&fct.lock);
				return NULL;
			}
		} else {
			FLS_ERROR("FLS create reply flow failed, now free common stats");
			spin_lock_bh(&fct.lock);
			fls_conn_delete_internal(orig);
			fls_conn_free_cmn(cmn);
			spin_unlock_bh(&fct.lock);
			return NULL;
		}
	}

	reply->last_ts = last_ts;

	if (isexternal)
		FLS_INFO("FID: creating fls %sconnection.", isexternal?"external ":"");

	orig->externalrule = isexternal;
	reply->externalrule = isexternal;

	orig->reverse = reply;
	reply->reverse = orig;
	orig->dir = FLS_CONN_DIRECTION_ORIG;
	reply->dir = FLS_CONN_DIRECTION_RET;

	cmn->orig = orig;
	cmn->reply = reply;
	orig->cmn = cmn;
	reply->cmn = cmn;

	/*
	 * Initialize event timers for common data
	 */
	fls_def_sensor_timer_init(cmn->timers);

	return orig;
}

/*
 * fls_conn_create()
 *	Creates a bidirectional connection for non-external connection
 *	in the connection database.
 */
void fls_conn_create(uint8_t ip_version,
						uint8_t protocol,
						uint32_t *orig_src_ip,
						uint16_t orig_src_port,
						uint32_t *orig_dest_ip,
						uint16_t orig_dest_port,
						void **orig_conn,
						void **repl_conn) {
	struct fls_conn *orig;

	orig = fls_conn_create_bidiflow(ip_version,
						protocol,
						orig_src_ip,
						orig_src_port,
						orig_dest_ip,
						orig_dest_port,
						false, 0);

	if(orig) {
		*orig_conn = orig;
		*repl_conn = orig->reverse;
		return;
	}

	*orig_conn = NULL;
	*repl_conn = NULL;

}
EXPORT_SYMBOL(fls_conn_create);

/*
 * fls_conn_tracker_init()
 *	Initializes the FLS connection tracker by setting up internal
 *	data structures, locks, and memory caches required for managing
 *	connections.
 */
int fls_conn_tracker_init(void)
{
	memset(&fct, 0, sizeof(fct));
	spin_lock_init(&fct.lock);
	fls_sensor_manager_init(&fct.fsm);

	/*
	 * Create slab cache for fls_conn objects
	 */
	fls_conn_cache = kmem_cache_create("fls_conn_cache",
			sizeof(struct fls_conn), 0,
			0, NULL);
	if (!fls_conn_cache) {
		FLS_ERROR("Failed to create FLS connection slab cache\n");
		return -ENOMEM;
	}

	/*
	 * Dynamically allocate hash table
	 */
	fct.hash = kvzalloc(sizeof(struct fls_conn *) * FLS_CONN_HASH_SIZE, GFP_KERNEL);
	if (!fct.hash) {
		FLS_ERROR("Failed to allocate FLS connection hash table\n");
		kmem_cache_destroy(fls_conn_cache);
		fls_conn_cache = NULL;
		return -ENOMEM;
	}

	/*
	 * Max connections is now a static limit
	 */
	fct.max_connections = FLS_CONN_MAX;
	fct.num_connections = 0;
	return 0;
}

/*
 * fls_conn_tracker_exit()
 *	Cleans up resources allocated by the FLS connection tracker,
 *	including hash tables and slab caches, during module shutdown.
 */
void fls_conn_tracker_exit(void)
{
	FLS_TRACE("fls_conn_tracker_exit\n");

	if (fct.hash) {
		kvfree(fct.hash);
		fct.hash = NULL;
	}

	if (fls_conn_cache) {
		kmem_cache_destroy(fls_conn_cache);
		fls_conn_cache = NULL;
	}
}
