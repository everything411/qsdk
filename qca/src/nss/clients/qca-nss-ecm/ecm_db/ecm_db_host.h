/*
 **************************************************************************
 * Copyright (c) 2014-2018, 2020, The Linux Foundation. All rights reserved.
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: ISC
 **************************************************************************
 */

/*
 * Magic number
 */
#define ECM_DB_HOST_INSTANCE_MAGIC 0x2873

typedef uint32_t ecm_db_host_hash_t;

/*
 * Host flags
 */
#define ECM_DB_HOST_FLAGS_INSERTED 1			/* Host is inserted into connection database tables */

/*
 * struct ecm_db_host_instance
 */
struct ecm_db_host_instance {
	struct ecm_db_host_instance *next;		/* Next instance in global list */
	struct ecm_db_host_instance *prev;		/* Previous instance in global list */
	struct ecm_db_host_instance *hash_next;		/* Next host in the chain of hosts */
	struct ecm_db_host_instance *hash_prev;		/* previous host in the chain of hosts */
	ip_addr_t address;				/* RO: IPv4/v6 Address of this host */
	bool on_link;					/* RO: false when this host is reached via a gateway */
	uint32_t time_added;				/* RO: DB time stamp when the host was added into the database */

#ifdef ECM_DB_XREF_ENABLE
	/*
	 * Normally the mapping refers to the host it requires.
	 * However the host also keeps a list of all mappings that are associated with it.
	 */
	struct ecm_db_mapping_instance *mappings;	/* Mappings made on this host */
	int mapping_count;				/* Number of mappings in the mapping list */
#endif

#ifdef ECM_DB_ADVANCED_STATS_ENABLE
	uint64_t from_data_total;			/* Total of data sent by this host */
	uint64_t to_data_total;				/* Total of data sent to this host */
	uint64_t from_packet_total;			/* Total of packets sent by this host */
	uint64_t to_packet_total;			/* Total of packets sent to this host */
	uint64_t from_data_total_dropped;
	uint64_t to_data_total_dropped;
	uint64_t from_packet_total_dropped;
	uint64_t to_packet_total_dropped;
#ifdef ECM_DB_PER_CLIENT_ROUTED_STATS_ENABLE
	uint64_t from_data_routed;			/* Total of routed data sent by this host */
	uint64_t to_data_routed;			/* Total of routed data sent to this host */
	uint64_t from_packet_routed;			/* Total of routed packets sent by this host */
	uint64_t to_packet_routed;			/* Total of routed packets sent to this host */
	uint64_t rx_routed_bytes[ECM_DB_IFACE_TYPE_COUNT];		/* Total of routed bytes received by this host from per interface type */
	uint64_t rx_routed_packets[ECM_DB_IFACE_TYPE_COUNT];		/* Total of routed packet received by this host from per interface type */
	uint64_t tx_routed_bytes[ECM_DB_IFACE_TYPE_COUNT];		/* Total of routed bytes sent by this host through per interface type */
	uint64_t tx_routed_packets[ECM_DB_IFACE_TYPE_COUNT];		/* Total of routed packets sent by this host through per interface type */
#endif
#endif

	ecm_db_host_final_callback_t final;		/* Callback to owner when object is destroyed */
	void *arg;					/* Argument returned to owner in callbacks */
	uint32_t flags;
	int refs;					/* Integer to trap we never go negative */
	ecm_db_host_hash_t hash_index;
#if (DEBUG_LEVEL > 0)
	uint16_t magic;
#endif
};

int _ecm_db_host_count_get(void);

void _ecm_db_host_ref(struct ecm_db_host_instance *hi);
void ecm_db_host_ref(struct ecm_db_host_instance *hi);
int ecm_db_host_deref(struct ecm_db_host_instance *hi);

void ecm_db_host_address_get(struct ecm_db_host_instance *hi, ip_addr_t addr);
bool ecm_db_host_on_link_get(struct ecm_db_host_instance *hi);

struct ecm_db_host_instance *ecm_db_host_find_and_ref(ip_addr_t address);
struct ecm_db_host_instance *ecm_db_hosts_get_and_ref_first(void);
struct ecm_db_host_instance *ecm_db_host_get_and_ref_next(struct ecm_db_host_instance *hi);

#ifdef ECM_DB_XREF_ENABLE
int ecm_db_host_mapping_count_get(struct ecm_db_host_instance *hi);
void ecm_db_host_connections_defunct_by_dir(ip_addr_t addr, ecm_db_obj_dir_t dir);
void ecm_db_host_connections_defunct_by_src_and_dest(ip_addr_t src_addr, ip_addr_t dest_addr);
#endif

struct ecm_db_host_instance *ecm_db_host_alloc(void);
void ecm_db_host_add(struct ecm_db_host_instance *hi,
		     ip_addr_t address,
		     bool on_link,
		     ecm_db_host_final_callback_t final, void *arg);

#ifdef ECM_STATE_OUTPUT_ENABLE
int ecm_db_host_state_get(struct ecm_state_file_instance *sfi, struct ecm_db_host_instance *hi);
int ecm_db_host_hash_table_lengths_get(int index);
int ecm_db_host_hash_index_get_next(int index);
int ecm_db_host_hash_index_get_first(void);
#endif

bool ecm_db_host_init(struct dentry *dentry);
void ecm_db_host_exit(void);
