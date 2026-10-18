/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: ISC
*/
#ifndef SDC_TELEMETRY
#define SDC_TELEMETRY

#include <linux/fs.h>
#include <linux/wait.h>
#include <linux/uaccess.h>
#include <linux/miscdevice.h>

extern struct sdc_ctx sdc_ctx;

/*
 * For now we only support 10 outstanding device data to be sent to userspace.
 */
#define SDC_TELEMETRY_Q_MAX 128
#define SDC_TELEMETRY_BUF_SZ SDC_MACDB_BUF_SZ
#define SDC_TELEMETRY_TLV_MDATA_SZ 3
#define SDC_TELEMETRY_DEV_NAME "sdc_telemetry"

/*
 * +--------+----------------+-------------------+
 * | len(2) | mac_addr(6)    | tlvs (len - 8)    |
 * +--------+----------------+-------------------+
 */
struct sdc_telemetry_buf {
	uint16_t len;
	uint8_t mac[ETH_ALEN];
}__packed;

/*
 * +--------+--------+------------------+
 * | len(2) | type(1)| value (len - 3)  |
 * +--------+--------+------------------+
 */
struct sdc_telemetry_tlv {
	uint16_t len;
	uint8_t type;
	uint8_t *value;
}__packed;

/*
 * sdc_telemetry_msg_req_type
 * 	SDC Message type
 */
typedef enum sdc_telemetry_msg_req_type {
	SDC_TELEMETRY_MSG_ADDIF,
	SDC_TELEMETRY_MSG_DELIF,
	SDC_TELEMETRY_MSG_CLEAR,
	SDC_TELEMETRY_MSG_RESET,
	SDC_TELEMETRY_MSG_MAX
} sdc_telemetry_msg_req_type;

/*
 * sdc_telemetry_msg_add_del_if
 * 	SDC
 */
struct sdc_telemetry_msg_add_del_if {
	char ifname[IFNAMSIZ];
};

/*
 * sdc_request
 * 	SDC request structure
 */
struct sdc_telemetry_msg_req {
	sdc_telemetry_msg_req_type msg_type;
	union {
		struct sdc_telemetry_msg_add_del_if add_del_if;
	} msg;
};

typedef enum {
	SDC_TELEMETRY_TYPE_DHCP,
	SDC_TELEMETRY_TYPE_DNS,
	SDC_TELEMETRY_TYPE_MDNS,
	SDC_TELEMETRY_TYPE_SSDP,
	SDC_TELEMETRY_TYPE_TLS,
	SDC_TELEMETRY_TYPE_STATS,
} sdc_telemetry_type_t;

/*
 * sdc_telemetry_stats_val
 *	Per-device counters that cannot be derived from raw TLV counts alone —
 *	either because data comes from the egress hook (offer/ack/nak/responses)
 *	or a true total is needed beyond the stored TLV cap (tls_ch_count).
 *	Counters derivable by counting TLVs (query_count, request_count etc.)
 *	are intentionally omitted — userspace counts TLVs directly.
 */
struct sdc_telemetry_stats_val {
	uint8_t  dhcp_discover_count;
	uint8_t  dhcp_offer_count;
	uint8_t  dhcp_ack_count;
	uint8_t  dhcp_nak_count;
	uint8_t  drop_dhcp;
	uint16_t dns_response_count;
	uint16_t dns_nxdomain_count;
	uint16_t dns_servfail_count;
	uint8_t  drop_dns;
	uint8_t  drop_mdns;
	uint16_t ssdp_pkt_count;
	uint8_t  drop_ssdp;
	uint8_t  tls_ch_count;
	uint8_t  drop_tls;
	uint16_t quic_initial_count;
	uint8_t  quic_version_bitmap;
	uint8_t  drop_quic;
} __packed;

/*
 * sdc_telemetry_ssdp_val
 *	SSDP TLV value — one per unique USN string observed from the device.
 *	usn is a variable-length string immediately following the struct.
 */
struct sdc_telemetry_ssdp_val {
	uint8_t usn_len;
	uint8_t usn[0];
} __packed;

struct sdc_telemetry_ctx {
	/*
	 * TODO: Check if a dedicated kthread makes sense for our workload.
	 * Most probably we should be okay with using system_wq.
	 * Profile and come back to this.
	 */
	struct work_struct work;

	struct sdc_telemetry_buf *queue[SDC_TELEMETRY_Q_MAX];
	int head;
	int tail;
	spinlock_t lock;

	bool mmap_empty;
	void *mmap_buf;
	uint16_t mmap_size;
	bool write_allowed;

	wait_queue_head_t waitq;
};

extern int sdc_telemetry_init(struct sdc_telemetry_ctx *ctx);
extern void sdc_telemetry_deinit(struct sdc_telemetry_ctx *ctx);
extern bool sdc_telemetry_add_tlv(struct sdc_telemetry_buf *buf, struct sdc_telemetry_tlv *tlv, uint16_t max_len);
extern bool sdc_telemetry_enqueue(struct sdc_telemetry_ctx *ctx, struct sdc_telemetry_buf *buf);
#endif
