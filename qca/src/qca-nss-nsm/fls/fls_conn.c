/*
 **************************************************************************
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
 **************************************************************************
 */

#include "fls_conn.h"
#include "fls_debug.h"

struct fls_conn_tracker fct;

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
			connection->src_ip[4] != src_ip[3] ||
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

static inline struct fls_conn *fls_conn_create_flow(uint8_t ip_version,
								uint8_t protocol,
								uint32_t *src_ip,
								uint16_t src_port,
								uint32_t *dest_ip,
								uint16_t dest_port)
{
	struct fls_conn *connection;
	uint32_t hash;
	spin_lock(&(fct.lock));
	connection = fct.free_list;
	if (!connection) {
		spin_unlock(&(fct.lock));
		return NULL;
	}
	fct.free_list = connection->all_next;
	spin_unlock(&(fct.lock));

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
	connection->flags = FLS_CONNECTION_FLAG_ENABLE_MASK;

	spin_lock(&(fct.lock));
	connection->all_next = fct.all_connections_head;

	if (fct.all_connections_head) {
		fct.all_connections_head->all_prev = connection;
	}

	fct.all_connections_head = connection;

	connection->hash_next = fct.hash[hash];
	if (fct.hash[hash]) {
		fct.hash[hash]->hash_prev = connection;
	}

	fct.hash[hash] = connection;
	spin_unlock(&(fct.lock));
	return connection;
}

void fls_conn_stats_update(void *connection, struct sk_buff *skb)
{
	fls_sensor_manager_call_all(&fct.fsm, (struct fls_conn *)connection, skb);
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

	spin_lock(&(fct.lock));
	connection = fct.hash[hash];

	while (connection) {
		if (fls_conn_matches(connection, ip_version, protocol, src_ip, src_port, dest_ip, dest_port)) {
			if (connection == fct.all_connections_head) {
				spin_unlock(&(fct.lock));
				return connection;
			}

			if (connection == fct.all_connections_tail) {
				fct.all_connections_tail = connection->all_prev;
				connection->all_prev->all_next = NULL;
				connection->all_prev = NULL;
				connection->all_next = fct.all_connections_head;
				fct.all_connections_head = connection;
				spin_unlock(&(fct.lock));
				return connection;
			}

			connection->all_prev->all_next = connection->all_next;
			connection->all_next->all_prev = connection->all_prev;
			connection->all_next = fct.all_connections_head;
			fct.all_connections_head = connection;
			spin_unlock(&(fct.lock));
			return connection;
		}

		connection = connection->hash_next;
	}

	spin_unlock(&(fct.lock));
	return NULL;
}
EXPORT_SYMBOL(fls_conn_lookup);

void fls_conn_delete(void *conn)
{
	struct fls_conn *connection = (struct fls_conn *)conn;
	struct fls_conn *reply = connection->reverse;
	if (reply) {
		reply->reverse = NULL;
	}

	FLS_INFO("FID: Deleting connection.");
	fls_debug_print_conn_info(connection);

	spin_lock(&(fct.lock));
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
	connection->all_prev = NULL;
	connection->hash_next = NULL;
	connection->hash_prev = NULL;
	memset(&connection->stats, 0, sizeof(connection->stats));
	fct.free_list = connection;
	spin_unlock(&(fct.lock));
}
EXPORT_SYMBOL(fls_conn_delete);

/*
 * fls_conn_create()
 *	Creates a bidirectional connection in the connection database.
 */
void fls_conn_create(uint8_t ip_version,
						uint8_t protocol,
						uint32_t *orig_src_ip,
						uint16_t orig_src_port,
						uint32_t *orig_dest_ip,
						uint16_t orig_dest_port,
						uint32_t *ret_src_ip,
						uint16_t ret_src_port,
						uint32_t *ret_dest_ip,
						uint16_t ret_dest_port,
						void **orig_conn,
						void **repl_conn)
{
	struct fls_conn *orig;
	struct fls_conn *reply;
	orig = fls_conn_create_flow(ip_version, protocol, orig_src_ip, orig_src_port, orig_dest_ip, orig_dest_port);
	if (!orig) {
		*orig_conn = NULL;
		*repl_conn = NULL;
		return;
	}

	reply = fls_conn_create_flow(ip_version, protocol, ret_src_ip, ret_src_port, ret_dest_ip, ret_dest_port);
	if (!reply) {
		fls_conn_delete(orig);
		*orig_conn = NULL;
		*repl_conn = NULL;
		return;
	}

	FLS_INFO("FID: creating fls connection.");
	fls_debug_print_conn_info(orig);

	orig->reverse = reply;
	reply->reverse = orig;
	orig->dir = FLS_CONN_DIRECTION_ORIG;
	reply->dir = FLS_CONN_DIRECTION_RET;

	*orig_conn = orig;
	*repl_conn = reply;
}
EXPORT_SYMBOL(fls_conn_create);

void fls_conn_tracker_init(void)
{
	uint32_t i;
	memset(&fct, 0, sizeof(fct));
	spin_lock_init(&fct.lock);
	fls_sensor_manager_init(&fct.fsm);
	for (i = 0; i < FLS_CONN_MAX; i++) {
		struct fls_conn *conn = &(fct.connections[i]);

		/*
		 * The free list is maintained as a singly-linked list because there's no need
		 * to traverse it backward.
		 */
		conn->all_next = fct.free_list;
		fct.free_list = conn;
	}
}
