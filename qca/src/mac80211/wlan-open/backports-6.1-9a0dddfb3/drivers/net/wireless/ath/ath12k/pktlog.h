// SPDX-License-Identifier: BSD-3-Clause-Clear
/*
 * Copyright (c) 2022 Qualcomm Innovation Center, Inc. All rights reserved.
 */

#ifndef _PKTLOG_H_
#define _PKTLOG_H_

#include "core.h"
#include <net/sock.h>

#define PKTLOG_EXTRA_PAGES      2
#define CUR_PKTLOG_VER          10010  /* Packet log version */
#define PKTLOG_MAGIC_NUM        7735225
#define PKTLOG_NEW_MAGIC_NUM    2453506
#define PKTLOG_MAGIC_NUM_FW_VERSION_SUPPORT 0xDECDF1F0
#define ATH12K_DEBUGFS_PKTLOG_SIZE_DEFAULT (10 * 1024 * 1024)
#define ATH12K_PKTLOG_SIZE_MIN  (16 * 1024)
#define ATH12K_PKTLOG_SIZE_MAX  (50 * 1024 * 1024)
#define INVALID_OFFSET		((size_t)-1)

#define ATH12K_PKTLOG_HDR_FLAGS_MASK 0xffff
#define ATH12K_PKTLOG_HDR_FLAGS_SHIFT 0
#define ATH12K_PKTLOG_HDR_FLAGS_OFFSET 0
#define ATH12K_PKTLOG_HDR_MISSED_CNT_MASK 0xffff0000
#define ATH12K_PKTLOG_HDR_MISSED_CNT_SHIFT 16
#define ATH12K_PKTLOG_HDR_MISSED_CNT_OFFSET 0
#define ATH12K_PKTLOG_HDR_LOG_TYPE_MASK 0xffff
#define ATH12K_PKTLOG_HDR_LOG_TYPE_SHIFT 0
#define ATH12K_PKTLOG_HDR_LOG_TYPE_OFFSET 1
#define ATH12K_PKTLOG_HDR_SIZE_MASK 0xffff0000
#define ATH12K_PKTLOG_HDR_SIZE_SHIFT 16
#define ATH12K_PKTLOG_HDR_SIZE_OFFSET 1
#define ATH12K_PKTLOG_HDR_TIMESTAMP_OFFSET 2
#define ATH12K_PKTLOG_HDR_TYPE_SPECIFIC_DATA_OFFSET 3

/* Remote pktlog constants */
#define ATH12K_DEFAULT_REMOTE_PKTLOG_PORT 2335
#define ATH12K_MAX_SEND_SIZE 1024
#define ATH12K_MAX_LISTEN_CONNECTIONS 5
#define ATH12K_PKTLOG_HEADER_SIZE 92
#define ATH12K_IP_ADDR_STR_MAX 20
#define ATH12K_PKTLOG_SEND_SLEEP_MS 10

enum ath12k_pktlog_flag {
        PKTLOG_FLG_FRM_TYPE_LOCAL_S = 0,
        PKTLOG_FLG_FRM_TYPE_REMOTE_S,
        PKTLOG_FLG_FRM_TYPE_CLONE_S,
        PKTLOG_FLG_FRM_TYPE_UNKNOWN_S
};

enum ath12k_pktlog_mode {
        ATH12K_PKTLOG_DISABLED,
        ATH12K_PKTLOG_MODE_LITE,
        ATH12K_PKTLOG_MODE_FULL,
        ATH12K_PKTLOG_MODE_INVALID,
};

enum ath12k_pktlog_filter {
        ATH12K_PKTLOG_RX                = BIT(0),
        ATH12K_PKTLOG_TX                = BIT(1),
        ATH12K_PKTLOG_RCFIND            = BIT(2),
        ATH12K_PKTLOG_RCUPDATE          = BIT(3),
        ATH12K_PKTLOG_DBG_PRINT         = BIT(4),
        ATH12K_PKTLOG_SMART_ANT         = BIT(5),
        ATH12K_PKTLOG_SW                = BIT(6),
        ATH12K_PKTLOG_PHY_LOGGING       = BIT(7),
        ATH12K_PKTLOG_CBF               = BIT(8),
        ATH12K_PKTLOG_HYBRID            = BIT(9),
	ATH12K_PKTLOG_REMOTE_ENABLE     = BIT(10),
};

enum ath12k_pktlog_enum {
        ATH12K_PKTLOG_TYPE_TX_CTRL      = 1,
        ATH12K_PKTLOG_TYPE_TX_STAT      = 2,
        ATH12K_PKTLOG_TYPE_TX_MSDU_ID   = 3,
        ATH12K_PKTLOG_TYPE_TX_FRM_HDR   = 4,
        ATH12K_PKTLOG_TYPE_RX_STAT      = 5,
        ATH12K_PKTLOG_TYPE_RC_FIND      = 6,
        ATH12K_PKTLOG_TYPE_RC_UPDATE    = 7,
        ATH12K_PKTLOG_TYPE_TX_VIRT_ADDR = 8,
        ATH12K_PKTLOG_TYPE_DBG_PRINT    = 9,
        ATH12K_PKTLOG_TYPE_RX_CBF       = 10,
        ATH12K_PKTLOG_TYPE_ANI          = 11,
        ATH12K_PKTLOG_TYPE_GRPID        = 12,
        ATH12K_PKTLOG_TYPE_TX_MU        = 13,
        ATH12K_PKTLOG_TYPE_SMART_ANTENNA = 14,
        ATH12K_PKTLOG_TYPE_TX_PFSCHED_CMD = 15,
        ATH12K_PKTLOG_TYPE_TX_FW_GENERATED1 = 19,
        ATH12K_PKTLOG_TYPE_TX_FW_GENERATED2 = 20,
        ATH12K_PKTLOG_TYPE_MAX = 21,
        ATH12K_PKTLOG_TYPE_RX_STATBUF   = 22,
        ATH12K_PKTLOG_TYPE_PPDU_STATS   = 23,
        ATH12K_PKTLOG_TYPE_LITE_RX      = 24,
        ATH12K_PKTLOG_TYPE_HOST_SW_EVENT = 30,
};

struct ath12k_pktlog_hdr_arg {
	u16 log_type;
	u8 *payload;
	u16 payload_size;
	u8 *pktlog_hdr;
};

struct ath12k_pktlog_bufhdr {
        u32 magic_num;  /* Used by post processing scripts */
        u32 version;    /* Set to CUR_PKTLOG_VER */
        u8 software_image[40];
        u8 chip_info[40];
        u32 pktlog_defs_json_version;
};

struct ath12k_pktlog_buf {
        struct ath12k_pktlog_bufhdr bufhdr;
        int rd_offset;
        int wr_offset;
	char log_data[];
};

/**
 * struct ath12k_pktlog_remote_service - Remote pktlog service
 * @connection_service: Work queue for server mode connection setup
 * @accept_service: Work queue for accepting incoming connections
 * @send_service: Work queue for data transmission
 * @client_service: Work queue for client mode connection
 * @listen_socket: Listen socket for server mode
 * @send_socket: Socket for data transmission (both modes)
 * @port: Port number for remote pktlog (default: 2335)
 * @running: Flag indicating service is running
 * @connect_done: Flag indicating connection established
 * @missed_records: Counter for dropped records
 * @fend_counts: File end marker counter
 */
struct ath12k_pktlog_remote_service {
	struct work_struct connection_service;
	struct work_struct accept_service;
	struct work_struct send_service;
	struct work_struct client_service;
	struct socket *listen_socket;
	struct socket *send_socket;
	u16 port;
	int running;
	int connect_done;
	u32 missed_records;
	u32 fend_counts;
	struct ath12k *ar;
};

/**
 * struct ath12k_pktlog_remote_id_hdr - Remote pktlog identification header
 *
 * Sent at the start of each remote pktlog connection to identify the source.
 * Total size is 72 bytes to match ODD_HEADER_SIZE in pktlog_remote_ud.py,
 * allowing the script to skip this header and find the magic number at offset 72.
 *
 * @mac_addr:   Device MAC address (6 bytes) - from ar->ab->mac_addr
 * @ip_addr:    Device local IP address in network byte order (4 bytes)
 *              Obtained from kernel_getsockname() after connection
 * @reserved:   Zero-padded reserved bytes (54 bytes)
 * @radio_name: Radio PHY name, e.g., "phy00" (8 bytes, null-padded)
 *              Obtained from ar->pdev->phy_name
 */
struct ath12k_pktlog_remote_id_hdr {
	u8   mac_addr[ETH_ALEN];
	u8   ip_addr[4];
	u8   reserved[54];
	char radio_name[8];
} __packed;

/**
 * struct ath12k_pktlog - Packet log context
 * @buf: Pointer to the circular buffer for storing packet log data
 * @filter: Bitmask of enabled packet log filters (ATH12K_PKTLOG_RX,
 *          ATH12K_PKTLOG_TX, ATH12K_PKTLOG_CBF, ATH12K_PKTLOG_HYBRID, etc.)
 * @buf_size: Size of the packet log buffer in bytes
 * @lock: Spinlock to protect concurrent access to packet log buffer and indices
 * @hdr_size: Size of packet log header structure (struct ath12k_pktlog_hdr)
 * @hdr_size_field_offset: Offset of the size field within packet log header
 * @fw_version_record: Flag indicating firmware version record support
 * @invalid_decode_info: Flag indicating invalid decode information received
 *                       from firmware (set when firmware doesn't support
 *                       WMI_TLV_SERVICE_PKTLOG_DECODE_INFO_SUPPORT)
 * @rlog_write_index: Write index for remote packet log circular buffer.
 *                    Points to the next position where data will be written.
 *                    Protected by @lock.
 * @rlog_read_index: Read index for remote packet log circular buffer.
 *                   Points to the next position to be read and sent to remote.
 *                   Protected by @lock.
 * @rlog_max_size: Maximum size of remote packet log buffer in bytes.
 *                 Typically same as @buf_size but can be configured separately.
 * @is_wrap: Flag indicating if write index has wrapped around the buffer.
 *           Set to 1 when @rlog_write_index wraps to 0, cleared when
 *           @rlog_read_index catches up. Used to detect buffer overrun.
 * @ipaddr: IP address string for remote packet log server in client mode.
 *          Format: "xxx.xxx.xxx.xxx" (IPv4). Maximum length defined by
 *          ATH12K_IP_ADDR_STR_MAX. Empty string in server mode.
 * @pktlog_remote_client: Flag indicating remote packet log mode.
 *                        0 = server mode (listen for connections)
 *                        1 = client mode (connect to @ipaddr)
 * @rpktlog_svc: Remote packet log service context containing work queues,
 *               socket handles, connection state, and buffer management for
 *               streaming packet log data over TCP to a remote machine.
 *
 * This structure maintains the state for packet logging functionality including
 * local circular buffer management and optional remote streaming capability.
 * The remote pktlog feature allows streaming captured packet log data to a
 * backbone machine over TCP socket connection for long-duration captures that
 * exceed local buffer capacity.
 *
 * Buffer Management:
 * - Local buffer (@buf) uses @buf->rd_offset and @buf->wr_offset
 * - Remote buffer uses @rlog_read_index and @rlog_write_index
 * - @is_wrap flag prevents read/write index collision detection issues
 *
 */
struct ath12k_pktlog {
	struct ath12k_pktlog_buf *buf;
	u32 filter;
	u32 buf_size;
	u32 buf_alloc_size;
	spinlock_t lock;
	u8 hdr_size;
	u8 hdr_size_field_offset;
	u32 fw_version_record;
	u32 invalid_decode_info;
	u32 rlog_write_index;
	u32 rlog_read_index;
	u32 rlog_max_size;
	int is_wrap;
	char ipaddr[ATH12K_IP_ADDR_STR_MAX];
	u8 pktlog_remote_client;
	struct ath12k_pktlog_remote_service *rpktlog_svc;
};

struct ath12k_pktlog_decode_info {
        u8 software_image[40];
        u8 chip_info[40];
        u32 pktlog_defs_json_version;
};

struct ath12k_pl_fw_info {
        u32 pdev_id;
        u8 software_image[40];
        u8 chip_info[40];
        u32 pktlog_defs_json_version;
} __packed;

/* Remote pktlog definitions */
int ath12k_pktlog_remote_service_send(struct ath12k_pktlog_remote_service *service,
				      char *buf, int len);
int ath12k_pktlog_stop_service(struct ath12k *ar);
void ath12k_pktlog_init_remote_service_work(struct ath12k *ar);
int ath12k_pktlog_remote_enable(struct ath12k *ar, u32 enable);
int ath12k_pktlog_buf_alloc_if_needed(struct ath12k *ar, bool *allocated);
void ath12k_pktlog_buf_release_if_allocated(struct ath12k *ar, bool allocated);
void ath12k_pktlog_buf_reset(struct ath12k *ar);

#endif /* _PKTLOG_H_ */
