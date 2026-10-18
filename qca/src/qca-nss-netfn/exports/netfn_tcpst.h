/*
 * netfn_tcpst.h
 *	Network function's TCP Speedtest public object definitions.
 *
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
#ifndef __NETFN_TCPST_H
#define __NETFN_TCPST_H

#define NETFN_TCPST_HTTP_HDR_MAX 256 		/* Max size of HTTP header */
#define NETFN_TCPST_CONN_MAX 4			/* Max number of connections supported */

/*
 * netfn_tcpst_stats
 *	Speedtest stats
 */
struct netfn_tcpst_stats {
	struct {
		uint64_t request_time;		/* TCP connection opening time */
		uint64_t response_time;		/* TCP connection eastablish time */
	} tcp_open;

	struct {
		uint64_t rom_time;		/* Request of message */
		uint64_t bom_time;		/* Beginning of message */
		uint64_t eom_time;		/* End of message */
	} test;

	struct {
		uint64_t sent;			/* Test bytes sent in upload case */
		uint64_t rcvd;			/* Test bytes received in download case */
	} test_bytes;

	struct {
		uint64_t sent;			/* Ethernet sent in upload case */
		uint64_t rcvd;			/* Ethernet received in download case */
	} eth_bytes;
};

/*
 * netfn_tcpst_state
 *	State of the run
 */
enum netfn_tcpst_state {
	NETFN_TCPST_STATE_NONE = 0,		/* Status unknown */
	NETFN_TCPST_STATE_NOT_RUNNING,		/* Test is not running */
	NETFN_TCPST_STATE_ABORTED,		/* Test is aborted */
	NETFN_TCPST_STATE_INPROGRESS,		/* Test is in progress */
	NETFN_TCPST_STATE_COMPLETED,		/* Test has completed */
	NETFN_TCPST_STATE_STOPPED,		/* Test has been stopped */
};

/*
 * netfn_tcpst_err
 *	Error status code
 */
enum netfn_tcpst_err {
	NETFN_TCPST_ERR_NONE,			/* Operation successfully completed without error */
	NETFN_TCPST_ERR_UNAVAIL,		/* No Test data is available */
	NETFN_TCPST_ERR_ALREADY_RUNNING,	/* Test is already running error */
	NETFN_TCPST_ERR_INVALID_RESP,		/* Invalid response received */
	NETFN_TCPST_ERR_TX,			/* Tx error */
	NETFN_TCPST_ERR_RX,			/* Rx error */
	NETFN_TCPST_ERR_PARAM,			/* Incorrect parameter */
	NETFN_TCPST_ERR_PROTO,			/* TCP protocol not registered */
	NETFN_TCPST_ERR_NOMEM,			/* Not enough memory */
	NETFN_TCPST_ERR_SOCK,			/* Unable to alloc socket */
	NETFN_TCPST_ERR_BIND,			/* Unable to bind socket to local address */
	NETFN_TCPST_ERR_CONNECT,		/* Unable to connect socket to server */
	NETFN_TCPST_ERR_FLOW,			/* Unable to push PPE flow */
	NETFN_TCPST_ERR_VP,			/* Unable to allocate VP */
	NETFN_TCPST_ERR_ABORT,			/* Test was aborted */
	NETFN_TCPST_ERR_CONN_STATE,		/* Inconsistent connection state detected */
	NETFN_TCPST_ERR_HTTP_START = 100,	/* HTTP starting response code */
	/* Reserved for HTTP error
	 * 1xx: Information response
	 * 2xx: Success
	 * 3xx: Redirection
	 * 4xx: Client errors
	 * 5xx: Server errors
	 */
	NETFN_TCPST_ERR_HTTP_END = 599,		/* HTTP ending response code */
	NETFN_TCPST_MAX = 600,			/* Max status */
};

/*
 * netfn_tcpst_result
 *	Test result
 */
struct netfn_tcpst_result {
	enum netfn_tcpst_state state;		/* State of the test */
	enum netfn_tcpst_err error;		/* Error status */
	struct netfn_tcpst_stats stats;		/* Test stats */
};

/*
 * netfn_tcpst_test
 *	Test case type to run
 */
enum netfn_tcpst_test {
	NETFN_TCPST_TEST_HTTP_UPLOAD = 1,	/* HTTP PUT test */
	NETFN_TCPST_TEST_HTTP_DOWNLOAD,		/* HTTP GET test */
	NETFN_TCPST_TEST_RAW_UPLOAD,		/* General TCP upload test */
	NETFN_TCPST_TEST_RAW_DOWNLOAD,		/* General TCP download test */
};

/*
 * netfn_tcpst_server
 *	TCP server based speed test
 */
struct netfn_tcpst_server {
	union {
		struct in_addr v4;		/* IPv4 addr */
		struct in6_addr v6;		/* IPv6 addr */
	} ip;

	uint16_t port;				/* Port number */
	uint8_t ip_version;			/* IPv4 or IPv6 socket */
};

/*
 * netfn_tcpst_http
 *	HTTP based speed test
 */
struct netfn_tcpst_http {
	size_t file_sz;				/* File size to be sent in bytes */
	uint32_t hdr_len;			/* HTTP header length */
	char hdr[NETFN_TCPST_HTTP_HDR_MAX];	/* HTTP header as a string */
};

/*
 * netfn_tcpst_raw
 *	Raw TCP based speed test
 */
struct netfn_tcpst_raw {
	size_t max_bytes;			/* Maximum bytes to be transferred */
};

/*
 * netfn_tcpst_cfg
 *	Configuration for setting tcp speedtest
 */
struct netfn_tcpst_cfg {
	struct netfn_tcpst_server remote;	/* Remote server to test with */
	enum netfn_tcpst_test test;		/* Test type */
	union {
		struct netfn_tcpst_http http;	/* HTTP test params */
		struct netfn_tcpst_raw raw;	/* General test params */
	};

	uint64_t buf_len;			/* Buffer length to configure */
	uint32_t duration;         		/* Test Duration in seconds */
	uint32_t offset;           		/* Start offset for test result capture in seconds */
	uint8_t conn;       			/* Total connections */
	uint8_t core_mask;         		/* Cores that can be used for speedtest */
};

/*
 * netfn_tcpst_comp_t
 *	Completion callback
 *
 * @void*[in] void pointer
 * @res[in] Result
 *
 * @return
 * void
 */
typedef void (*netfn_tcpst_comp_t) (void *app_data, struct netfn_tcpst_result *res);

#ifdef __KERNEL__

/*
 * netfn_tcpst_start()
 *	Starts the TCP Speedtest
 *
 * @cfg[in] TCP speedtest configuration
 *
 * @return
 * State of test
 */
enum netfn_tcpst_state netfn_tcpst_start(struct netfn_tcpst_cfg *cfg);

/*
 * netfn_tcpst_stop()
 *	Stops the TCP Speedtest
 *
 * @cb[in] Callback called on completion
 * @void*[in] Application data
 *
 * @return
 * State of test
 */
enum netfn_tcpst_state netfn_tcpst_stop(netfn_tcpst_comp_t cb, void *app_data);

/*
 * netfn_tcpst_get_stats()
 *	Returns the TCP stats
 *
 * @stats[in] Stats to be prepared and returned back
 *
 * @return
 * State of test
 */
enum netfn_tcpst_state netfn_tcpst_get_stats(struct netfn_tcpst_stats *stats);

#endif /* __KERNEL__ */
#endif /* __NETFN_TCPST_TYPES_H */
