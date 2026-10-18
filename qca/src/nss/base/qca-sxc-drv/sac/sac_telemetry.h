/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: ISC
 */
#ifndef SAC_TELEMETRY_H
#define SAC_TELEMETRY_H

/*
 * Buffer size for mmap.
 */
#define SAC_TELEMETRY_BUF_SZ	4096

/*
 * Maximum number of outstanding DNS responses in telemetry buffer.
 * It can be modified based on allocated mmap buffer size.
 */
#define SAC_TELEMETRY_DNS_RESPONSE_COUNT_MAX	(10)

/*
 * sac_telemetry_msg_type
 *	Message types for SAC
 */
enum sac_telemetry_msg_type {
	SAC_TELEMETRY_MSG_TYPE_RESET,
	SAC_TELEMETRY_MSG_TYPE_ENABLE,
	SAC_TELEMETRY_MSG_TYPE_DISABLE,
	SAC_TELEMETRY_MSG_TYPE_CONFIG,
	SAC_TELEMETRY_MSG_TYPE_MAX
};

/*
 * sac_request
 *	SAC request structure
 */
struct sac_telemetry_request {
	enum sac_telemetry_msg_type msg_type;	/* Message type. */
	struct sac_iface_data iface;		/* Interface details. */
};

extern struct sac_ctx sac_ctx_g;

bool sac_telemetry_send_data(struct sac_ctx *ctx, uint8_t *data, uint32_t data_len);
int sac_telemetry_init(struct sac_ctx *ctx);
void sac_telemetry_deinit(struct sac_ctx *ctx);
#endif /* SAC_TELEMETRY_H */
