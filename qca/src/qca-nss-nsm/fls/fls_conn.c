/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: ISC
 */

#include "fls_conn.h"
#include "fls_debug.h"

struct fls_conn_tracker fct;
s64 fls_conn_timeout = 200;

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

struct fls_conn *fls_conn_create_flow(uint8_t ip_version,
								uint8_t protocol,
								uint32_t *src_ip,
								uint16_t src_port,
								uint32_t *dest_ip,
								uint16_t dest_port)
{
	struct fls_conn *connection;
	uint32_t hash;
	connection = fct.free_list;
	if (!connection) {
		FLS_ERROR("Connection max reached.\n");
		return NULL;
	}
	fct.free_list = connection->all_next;
	if(fct.free_list)
		fct.free_list->all_prev = NULL;

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

	memset(&connection->stats, 0, sizeof(connection->stats));

	hash = fls_conn_get_connection_hash(ip_version, protocol, src_ip, src_port, dest_ip, dest_port);
	connection->hash = hash;
	connection->flags = SFE_FLS_CONNECTION_FLAG_DEF_ENABLE;

	connection->all_next = fct.all_connections_head;

	if (fct.all_connections_head) {
		fct.all_connections_head->all_prev = connection;
	}

	fct.all_connections_head = connection;

	if (!fct.all_connections_tail) {
		fct.all_connections_tail = connection;
	}

	connection->hash_next = fct.hash[hash];
	if (fct.hash[hash]) {
		fct.hash[hash]->hash_prev = connection;
	}

	fct.hash[hash] = connection;

	return connection;
}

uint8_t fls_conn_stats_update(void *connection, struct sk_buff *skb)
{
	struct fls_conn *conn = (struct fls_conn *)connection;

	fls_sensor_manager_call_all(&fct.fsm, conn, skb);
	return conn->flags;
}
EXPORT_SYMBOL(fls_conn_stats_update);

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

	spin_lock_bh(&(fct.lock));
	connection = fct.hash[hash];
	hash_head = connection;

	while (connection) {
		if (fls_conn_matches(connection, ip_version, protocol, src_ip, src_port, dest_ip, dest_port)) {
			if(connection == hash_head) {
				spin_unlock_bh(&(fct.lock));
				return connection;
			}
			connection->hash_prev->hash_next = connection->hash_next;
			if(connection->hash_next)
				connection->hash_next->hash_prev = connection->hash_prev;
			connection->hash_prev = NULL;
			connection->hash_next = hash_head;
			hash_head->hash_prev = connection;
			fct.hash[hash] = connection;
			spin_unlock_bh(&(fct.lock));
			return connection;
		}
		connection = connection->hash_next;
	}

	spin_unlock_bh(&(fct.lock));
	return NULL;
}
EXPORT_SYMBOL(fls_conn_lookup);

void fls_conn_delete_internal(void *conn)
{
	struct fls_conn *connection = (struct fls_conn *)conn;
	struct fls_conn *reply = connection->reverse;
	if (reply) {
		reply->reverse = NULL;
	}

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

	if (connection->hash_prev) {
		connection->hash_prev->hash_next = connection->hash_next;
	} else {
		fct.hash[connection->hash] = connection->hash_next;
	}

	if (connection->hash_next) {
		connection->hash_next->hash_prev = connection->hash_prev;
	}

	connection->all_next = fct.free_list;
	if (fct.free_list) {
		fct.free_list->all_prev = connection;
	}
	connection->all_prev = NULL;
	connection->hash_next = NULL;
	connection->hash_prev = NULL;
	connection->externalrule = false;
	memset(&connection->stats, 0, sizeof(connection->stats));
	fct.free_list = connection;
}

void fls_conn_flush() {
	struct fls_conn *conn;
	int i;
	FLS_TRACE("flush external connection\n");
	spin_lock_bh(&(fct.lock));
	for (i = 0; i < FLS_CONN_MAX; i++) {
		conn = &(fct.connections[i]);
		if(!conn->externalrule)
			continue;
		FLS_INFO("FID: Deleting connection.");
		fls_debug_print_conn_info(conn);
		fls_conn_delete_internal(conn);
	}
	spin_unlock_bh(&(fct.lock));
}

/*
 * fls_conn_delete()
 *	Delete one connection.
 */
void fls_conn_delete(void *conn)
{
	FLS_INFO("FID: Deleting connection.");
	spin_lock_bh(&(fct.lock));
	fls_debug_print_conn_info(conn);
	fls_conn_delete_internal(conn);
	spin_unlock_bh(&(fct.lock));
}
EXPORT_SYMBOL(fls_conn_delete);

/*
 * fls_conn_delete_timeout()
 *	Delete all timeout connection.
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
	orig = fls_conn_create_flow(ip_version, protocol, orig_src_ip, orig_src_port, orig_dest_ip, orig_dest_port);
	if (!orig && !isexternal) {
		return NULL;
	}

	if(!orig) {
		if(fls_conn_delete_timeout(last_ts, fls_conn_timeout)){
			orig = fls_conn_create_flow(ip_version, protocol, orig_src_ip, orig_src_port, orig_dest_ip, orig_dest_port);
		} else {
			return NULL;
		}
	}

	orig->last_ts = last_ts;

	reply = fls_conn_create_flow(ip_version, protocol, orig_dest_ip, orig_dest_port, orig_src_ip, orig_src_port);
	if (!reply && !isexternal) {
		fls_conn_delete_internal(orig);
		return NULL;
	}

	if(!reply) {
		if(fls_conn_delete_timeout(last_ts, fls_conn_timeout)) {
			reply = fls_conn_create_flow(ip_version, protocol, orig_dest_ip, orig_dest_port, orig_src_ip, orig_src_port);
		} else {
			fls_conn_delete_internal(orig);
			return NULL;
		}
	}

	reply->last_ts = last_ts;

	if(isexternal)
		FLS_INFO("FID: creating fls %sconnection.", isexternal?"external ":"");
	fls_debug_print_conn_info(orig);

	orig->externalrule = isexternal;
	reply->externalrule = isexternal;

	orig->reverse = reply;
	reply->reverse = orig;
	orig->dir = FLS_CONN_DIRECTION_ORIG;
	reply->dir = FLS_CONN_DIRECTION_RET;

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

	spin_lock_bh(&(fct.lock));
	orig = fls_conn_create_bidiflow(ip_version,
						protocol,
						orig_src_ip,
						orig_src_port,
						orig_dest_ip,
						orig_dest_port,
						false, 0);
	spin_unlock_bh(&(fct.lock));

	if(orig) {
		*orig_conn = orig;
		*repl_conn = orig->reverse;
		return;
	}

	*orig_conn = NULL;
	*repl_conn = NULL;

}
EXPORT_SYMBOL(fls_conn_create);

void fls_conn_tracker_init(void)
{
	uint32_t i;
	struct fls_conn *conn;
	memset(&fct, 0, sizeof(fct));
	spin_lock_init(&fct.lock);
	fls_sensor_manager_init(&fct.fsm);
	for (i = 0; i < FLS_CONN_MAX; i++) {
		conn = &(fct.connections[i]);

		/*
		 * The free list is maintained as a singly-linked list because there's no need
		 * to traverse it backward.
		 */
		if(fct.free_list)
			fct.free_list->all_prev = conn;
		conn->all_next = fct.free_list;
		fct.free_list = conn;
	}
}
