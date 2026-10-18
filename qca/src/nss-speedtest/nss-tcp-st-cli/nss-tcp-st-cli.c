/*
 **************************************************************************
 * Copyright (c) 2024 Qualcomm Innovation Center, Inc. All rights reserved.
 *
 * Permission to use, copy, modify, and/or distribute this software for
 * any purpose with or without fee is hereby granted, provided that the
 * above copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT
 * OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 **************************************************************************
 */

/*
 * @file NSS TCP Speedtest Userspace
 */

#include <string.h>
#include <getopt.h>
#include <stdbool.h>
#include <arpa/inet.h>
#include <curl/curl.h>
#include <inttypes.h>
#include "nss-tcp-st.h"

#define NSS_TCPST_CL_GB_TO_BYTES 1000000000
#define NSS_TCPST_CLI_CFG_MAX_LEN 64
#define NSS_TCPST_CLI_MAX_OPT 64
#define NSS_TCPST_CLI_BUF_LEN (128 * 1024)

static struct option primary_longopt[] =
{
	{"mode", required_argument, NULL, 'm'},
	{"help", no_argument, NULL, 'h'},
	{0, 0, 0, 0}
};

static struct option start_longopt[] = {
	{"file_name", required_argument, NULL, 'a'},
	{"file_size", required_argument, NULL, 's'},
	{"type", required_argument, NULL, 'g'},
	{"connections", required_argument, NULL, 'n'},
	{"core_mask", required_argument, NULL, 'c'},
	{"server_ip", required_argument, NULL, 'i'},
	{"port", required_argument, NULL, 'p'},
	{"buffer_len", required_argument, NULL, 'l'},
	{"user_agent", required_argument, NULL, 'u'},
	{"content_type", required_argument, NULL, 'x'},
	{"file_based", optional_argument, NULL, 'f'},
	{"time_based", required_argument, NULL, 't'},
	{"verbose", optional_argument, NULL, 'v'},
	{0, 0, 0, 0}
};

char *time_subopts[] = {"duration", "offset", NULL};

/*
 * nss_tcp_st_cli_get_short_option
 *	Derive short opstring from long_options
 */
static void nss_tcp_st_cli_get_short_option(const struct option *long_options, char *short_opt)
{
	const struct option *opt = long_options;
	int shortopt_len = 0, idx = 0;

	while (opt->name != NULL) {
		if (opt->val != 0) {
			shortopt_len++;
			if (opt->has_arg == required_argument) {
				shortopt_len++;
			}
		}
		opt++;
	}

	opt = long_options;
	while (opt->name != NULL) {
		if (opt->val != 0) {
			short_opt[idx++] = (char)opt->val;
			if (opt->has_arg == required_argument) {
				short_opt[idx++] = ':';
			}
		}
		opt++;
	}

	short_opt[idx] = '\0';

}

/*
 * nss_tcp_st_cli_usage()
 *	Usage for command line arguments
 */
static void nss_tcp_st_cli_usage(void)
{
	nss_tcp_st_log_info("\nUsage: nss-tcp-st [-m | --mode] <mode>\n");
	nss_tcp_st_log_info("\tnss-tcp-st [-h | --help]\n");
	nss_tcp_st_log_info("\nStart TCP Speed Test\n");
	nss_tcp_st_log_info("\tnss-tcp-st [-m | --mode] start [options]\n");
	nss_tcp_st_log_info("\nOptions:\n");
	nss_tcp_st_log_info("-i, --server_ip\t\t< IPv4/v6 address of the server >\n");
	nss_tcp_st_log_info("-p, --port\t\t< Port number of the server >\n");
	nss_tcp_st_log_info("-l, --buffer_len\t< Socket buffer length for test >\n");
	nss_tcp_st_log_info("-g, --type\t\t< Test type: http_download / http_upload / raw_download / raw_upload >\n");
	nss_tcp_st_log_info("-a, --file_name\t\t< File name to be uploaded / downloaded >\n");
	nss_tcp_st_log_info("-s, --file_size\t\t< Size of file to be uploaded in GB >\n");
	nss_tcp_st_log_info("-n, --connections\t< Total connections >\n");
	nss_tcp_st_log_info("-c, --core_mask\t\t< Cores to be used for speedtest >\n");
	nss_tcp_st_log_info("-u, --user_agent\t< HTTP user agent >\n");
	nss_tcp_st_log_info("-x, --content_type\t< HTTP content type >\n");
	nss_tcp_st_log_info("-t, --time_based duration=val,offset=val\n");
	nss_tcp_st_log_info("\t\t\t< Duration and offset in seconds for time based result capture >\n");
	nss_tcp_st_log_info("-v, --verbose\t\t< Enable debug logs >\n");
	nss_tcp_st_log_info("\nStop TCP Speed Test\n");
	nss_tcp_st_log_info("\tnss-tcp-st [-m | --mode] stop\n");
	nss_tcp_st_log_info("\nGet TCP Speed Test stats\n");
	nss_tcp_st_log_info("\tnss-tcp-st [-m | --mode] stats\n");

}

/*
 * nss_tcp_st_cli_set_http_header
 *	Parse the http header contents
 */
static int nss_tcp_st_cli_set_http_header(struct netfn_tcpst_cfg *st_cfg, char *ip, char *file_name, char *user_agent, char *content_type)
{
	int ret;

	if (st_cfg->test == NETFN_TCPST_TEST_HTTP_DOWNLOAD) {
		ret = snprintf(st_cfg->http.hdr, NETFN_TCPST_HTTP_HDR_MAX, "GET /%s HTTP/1.1\r\n"
				"Host: %s:%u\r\n"
				"User-Agent: %s\r\n"
				"Accept: */*\r\n\r\n",
				file_name, ip, ntohs(st_cfg->remote.port), user_agent);

		if (ret < 0 || ret >= NETFN_TCPST_HTTP_HDR_MAX) {
			nss_tcp_st_log_error("%px: Http header exceeds the max http header length\n", st_cfg);
			return -EINVAL;
		}

	} else {
		ret = snprintf(st_cfg->http.hdr, NETFN_TCPST_HTTP_HDR_MAX, "PUT /%s HTTP/1.1\r\n"
				"Host: %s:%u\r\n"
				"Content-Length: %zu\r\n"
				"Content-Type: %s\r\n\r\n",
				file_name, ip, ntohs(st_cfg->remote.port), st_cfg->http.file_sz, content_type);

		if (ret < 0 || ret >= NETFN_TCPST_HTTP_HDR_MAX) {
			nss_tcp_st_log_error("%px: Http header exceeds the max http header length\n", st_cfg);
			return -EINVAL;
		}
	}

	st_cfg->http.hdr_len = strlen(st_cfg->http.hdr);

	return 0;
}

/*
 * nss_tcp_st_cli_log
 *	Store cfg in tcpst log file
 */
static int nss_tcp_st_cli_log_cfg(struct netfn_tcpst_cfg *st_cfg, bool time_based)
{
	FILE *fp;

	fp = fopen("/tmp/tcpst", "w");
	if (!fp) {
		nss_tcp_st_log_error("%px:Failed to create tcpst file\n", st_cfg);
		return -ENOMEM;
	}

	fprintf(fp, "Test type:%d [1- HTTP upload, 2- HTTP download 3- RAW_UPLOAD 4- RAW_DOWNLOAD]\n", st_cfg->test);
	fprintf(fp, "Result type:%d [0 - File based, 1 - Time based]\n", time_based);
	fprintf(fp, "IP version:%d\nPort:%d\n", st_cfg->remote.ip_version, ntohs(st_cfg->remote.port));

	if (st_cfg->remote.ip_version == 4) {
		fprintf(fp, "IP address:%x\n", ntohl(st_cfg->remote.ip.v4.s_addr));
	} else {
		fprintf(fp, "IP address:%x%x%x%x\n", ntohl(st_cfg->remote.ip.v6.s6_addr32[0]),
				ntohl(st_cfg->remote.ip.v6.s6_addr32[1]),
				ntohl(st_cfg->remote.ip.v6.s6_addr32[2]),
				ntohl(st_cfg->remote.ip.v6.s6_addr32[3]));
	}

	if (st_cfg->test == NETFN_TCPST_TEST_HTTP_DOWNLOAD || st_cfg->test == NETFN_TCPST_TEST_HTTP_UPLOAD) {
		fprintf(fp, "Hdr:%s\nHdr len:%d\n", st_cfg->http.hdr, st_cfg->http.hdr_len);
	}

	fprintf(fp, "Connections:%d\nCore mask:%d\nOffset:%d\nDuration:%d\n",
			st_cfg->conn, st_cfg->core_mask, st_cfg->offset, st_cfg->duration);
	fprintf(fp, "Buffer length:%"PRIu64"\n", st_cfg->buf_len);
	fclose(fp);

	return 0;
}

/*
 * nss_tcp_st_cli_start()
 *	Parse command line arguments for TCPST and start the test
 */
static bool nss_tcp_st_cli_start(int args, char **argv)
{
	char content_type[NSS_TCPST_CLI_CFG_MAX_LEN];
	char start_shortopt[NSS_TCPST_CLI_MAX_OPT];
	char user_agent[NSS_TCPST_CLI_CFG_MAX_LEN];
	char file_name[NSS_TCPST_CLI_CFG_MAX_LEN];
	int option_index = 0, error, opt = 0;
	struct netfn_tcpst_cfg st_cfg;
	char ip[INET6_ADDRSTRLEN];
	bool time_based = false;
	size_t file_size = 0;
	bool http = false;

	memset(&st_cfg, 0, sizeof(st_cfg));
	nss_tcp_st_cli_get_short_option(start_longopt, start_shortopt);

	while (1) {
		opt = getopt_long_only(args, argv, start_shortopt, start_longopt, &option_index);

		if (opt == -1) {
			break;
		}

		switch (opt) {
		case 'p':
			uint16_t port = atoi(optarg);
			st_cfg.remote.port = htons(port);
			if (st_cfg.remote.port == 0) {
				nss_tcp_st_log_error("%px:Invalid port\n", argv);
				return false;
			}

			break;

		case 'i':
			if (inet_pton(AF_INET, optarg, &st_cfg.remote.ip.v4)) {
				st_cfg.remote.ip_version = 4;
				snprintf(ip, INET_ADDRSTRLEN, "%s", optarg);
			} else if (inet_pton(AF_INET6, optarg, &st_cfg.remote.ip.v6)) {
				st_cfg.remote.ip_version = 6;
				/*
				 * HTTP requires IPv6 address to be enclosed in [ ]
				 */
				snprintf(ip, INET6_ADDRSTRLEN + 2, "[%s]", optarg);
			} else {
				nss_tcp_st_log_error("%px:Invalid server IP Address %s\n", argv, ip);
				return false;
			}

			break;

		case 'l':
			st_cfg.buf_len = (uint64_t)(atoi(optarg));
			break;

		case 's':
			file_size = (uint64_t)(atoi(optarg)) * NSS_TCPST_CL_GB_TO_BYTES;
			break;

		case 'g':
			if (!strncmp("http_download", optarg, 13)) {
				st_cfg.test = NETFN_TCPST_TEST_HTTP_DOWNLOAD;
				http = true;
			} else if (!strncmp("http_upload", optarg, 11)) {
				st_cfg.test = NETFN_TCPST_TEST_HTTP_UPLOAD;
				http = true;
			} else if (!strncmp("raw_download", optarg, 12)) {
				st_cfg.test = NETFN_TCPST_TEST_RAW_DOWNLOAD;
			} else if (!strncmp("raw_upload", optarg, 10)) {
				st_cfg.test = NETFN_TCPST_TEST_RAW_UPLOAD;
			} else {
				nss_tcp_st_log_error("%px: Invalid test type\n", argv);
				return false;
			}

			break;

		case 'n':
			st_cfg.conn = atoi(optarg);
			break;

		case 'c':
			st_cfg.core_mask = atoi(optarg);
			break;

		case 'a':
			strlcpy(file_name, optarg, sizeof(file_name));
			break;

		case 'x':
			strlcpy(content_type, optarg, sizeof(content_type));
			break;

		case 'u':
			strlcpy(user_agent, optarg, sizeof(user_agent));
			break;

		case 't':
			char *t_subopts = optarg;
			char *t_value;

			time_based = true;

			while (*t_subopts != '\0') {
				int s_opt = getsubopt(&t_subopts, time_subopts, &t_value);
				switch (s_opt) {
				case 0:
					if (t_value) {
						st_cfg.duration = atoi(t_value);
					} else {
						nss_tcp_st_log_error("%px: Missing duration value\n", argv);
						return false;
					}

					break;

				case 1:
					if (t_value) {
						st_cfg.offset = atoi(t_value);
					} else {
						nss_tcp_st_log_error("%px: Missing offset value\n", argv);
						return false;
					}

					break;

				default:
					nss_tcp_st_cli_usage();
					return false;
				}
			}

			break;

		case 'v':
			system("echo 8 > /proc/sys/kernel/printk");
			system("echo \"module qca_nss_netfn_tcpst +p\" > /sys/kernel/debug/dynamic_debug/control");
			break;

		default:
			nss_tcp_st_cli_usage();
			return false;
		}
	}

	if (http) {
		st_cfg.http.file_sz = file_size;

		if (nss_tcp_st_cli_set_http_header(&st_cfg, ip, file_name, user_agent, content_type)) {
			nss_tcp_st_log_error("%px:Failed to parse http header\n", argv);
			return false;
		}
	} else {
		st_cfg.raw.max_bytes = file_size;
	}

	st_cfg.buf_len = !st_cfg.buf_len ? NSS_TCPST_CLI_BUF_LEN : st_cfg.buf_len;

	error = nss_tcp_st_cli_log_cfg(&st_cfg, time_based);
	if (error) {
		nss_tcp_st_log_error("%px:Failed to create tcpst logs\n", argv);
                return false;
	}

	return !nss_tcp_st_start(&st_cfg);
}

/*
 * nss_tcp_st_cli_show_stats
 *	Print tcpst stats
 */
static void nss_tcp_st_cli_show_stats(struct netfn_tcpst_stats *stats)
{
	nss_tcp_st_log_info("tcp_open_request_time\t: %"PRIu64" ns\n"
			"tcp_open_response_time\t: %"PRIu64" ns\n"
			"bom_time\t\t: %"PRIu64" ns\n"
			"rom_time\t\t: %"PRIu64" ns\n"
			"eom_time\t\t: %"PRIu64" ns\n"
			"test_bytes_sent\t\t: %"PRIu64" bytes\n"
			"test_bytes_received\t: %"PRIu64" bytes\n"
			"eth_bytes_sent\t\t: %"PRIu64" bytes\n"
			"eth_bytes_rcvd\t\t: %"PRIu64" bytes\n",
			stats->tcp_open.request_time,
			stats->tcp_open.response_time,
			stats->test.bom_time,
			stats->test.rom_time,
			stats->test.eom_time,
			stats->test_bytes.sent,
			stats->test_bytes.rcvd,
			stats->eth_bytes.sent,
			stats->eth_bytes.rcvd);
}

/*
 * nss_tcp_st_cli_get_offset
 *      Fetch the offset value passed during start
 */
static int nss_tcp_st_cli_get_offset(FILE *fp)
{
	char line[100];
	int offset;

	/*
	 * Fetch the offset from the tcpst configuration stored
	 * in /tmp/tcpst during START.
	 */
	while (fgets(line, sizeof(line), fp)) {
		if (sscanf(line, "Offset:%d", &offset) == 1) {
			return offset;
		}
	}

	return -1;
}

/*
 * nss_tcp_st_cli_completion
 *	Completion callback to print the result
 */
void nss_tcp_st_cli_completion(void *app_data, struct netfn_tcpst_result *res)
{
	double tcp_rtt, resp_time, req_rtt, rate, duration;
	struct netfn_tcpst_stats *stats = &res->stats;
	int offset = 0;
	FILE *fp;

	nss_tcp_st_cli_show_stats(stats);

	fp = fopen("/tmp/tcpst", "a+");
	if (!fp) {
		nss_tcp_st_log_error("%px:Failed to open tcpst file\n", res);
		return;
	}

	offset = nss_tcp_st_cli_get_offset(fp);
	if (offset < 0) {
		nss_tcp_st_log_error("%px: Perf metrics cannot be calculated\n", res);
		fclose(fp);
		return;
	}

	nss_tcp_st_log_info("\n\n******************PERF METRICS****************:\n");
	fprintf(fp, "\n\n******************PERF METRICS****************:\n");

	tcp_rtt = (double)(stats->tcp_open.response_time - stats->tcp_open.request_time) / 1000000;
	resp_time = (double)(stats->test.eom_time - stats->test.rom_time) / 1000000;
	req_rtt = (double)(stats->test.bom_time - stats->test.rom_time) / 1000000;
	duration = (double)(stats->test.eom_time - stats->test.bom_time) / 1000000;
	duration = duration - (offset * 1000);

	nss_tcp_st_log_info("Test Connection Handshake Round Trip Time\t: %f ms\n"
			"Test Transaction Response Time\t\t\t: %f ms\n"
			"Test Transaction Request Round Trip Time\t: %f ms\n"
			"Test Duration\t\t\t\t\t: %f ms\n",
			tcp_rtt, resp_time, req_rtt, duration);

	fprintf(fp, "Test Connection Handshake Round Trip Time\t: %f ms\n"
			"Test Transaction Response Time\t\t\t: %f ms\n"
			"Test Transaction Request Round Trip Time\t: %f ms\n"
			"Test Duration\t\t\t\t\t: %f ms\n",
			tcp_rtt, resp_time, req_rtt, duration);

	rate = 8 * stats->test_bytes.sent / (1000000 * duration);
	nss_tcp_st_log_info("Test Transaction Response Throughput (Tx)\t: %f Gbps\n", rate);
	fprintf(fp, "Test Transaction Response Throughput (Tx)\t: %f Gbps\n", rate);

	rate = 8 * stats->eth_bytes.sent / (1000000 * duration);
	nss_tcp_st_log_info("Test Line Interface Throughput (Tx)\t\t: %f Gbps\n", rate);
	fprintf(fp, "Test Line Interface Throughput (Tx)\t\t: %f Gbps\n", rate);

	rate = 8 * stats->test_bytes.rcvd / (1000000 * duration);
	nss_tcp_st_log_info("Test Transaction Response Throughput (Rx)\t: %f Gbps\n", rate);
	fprintf(fp, "Test Transaction Response Throughput (Rx)\t: %f Gbps\n", rate);

	rate = 8 * stats->eth_bytes.rcvd / (1000000 * duration);
	nss_tcp_st_log_info("Test Line Interface Throughput (Rx)\t\t: %f Gbps\n", rate);
	fprintf(fp, "Test Line Interface Throughput (Rx)\t\t: %f Gbps\n", rate);

	nss_tcp_st_log_info("******************PERF METRICS****************:\n\n");

	fprintf(fp, "******************PERF METRICS****************:\n\n");

	if (res->state == NETFN_TCPST_STATE_COMPLETED) {
		nss_tcp_st_log_info("...Test executed successfully...\n");
		fprintf(fp, "...Test executed successfully...\n");
	} else {
		nss_tcp_st_log_error("...Test failed to execute...\n");
		fprintf(fp, "...Test failed to execute...\n");
	}

	fclose(fp);
}

/*
 * nss_tcp_st_cli_stop()
 *      Stop the test
 */
static bool nss_tcp_st_cli_stop(int args, char **argv)
{
	return !nss_tcp_st_stop(nss_tcp_st_cli_completion, NULL);
}

/*
 * nss_tcp_st_cli_get_stats()
 *      Fetch the stats
 */
static bool nss_tcp_st_cli_get_stats(int args, char **argv)
{
	struct netfn_tcpst_stats stats = {0};
	enum netfn_tcpst_state state;

	if (nss_tcp_st_get_stats(&stats, &state)) {
		nss_tcp_st_log_error("%px: Unable to fetch statistics\n", argv);
		return false;
	}

	switch(state) {
	case NETFN_TCPST_STATE_INPROGRESS:
		nss_tcp_st_log_info("%px: Test in progress\n", argv);
		nss_tcp_st_cli_show_stats(&stats);
		break;

	case NETFN_TCPST_STATE_COMPLETED:
		nss_tcp_st_log_info("%px: Test completed\n", argv);
		nss_tcp_st_cli_show_stats(&stats);
		break;

	case NETFN_TCPST_STATE_ABORTED:
		nss_tcp_st_log_info("%px: Test aborted\n", argv);
		nss_tcp_st_cli_show_stats(&stats);
		break;

	default:
		nss_tcp_st_log_error("%px: No test statistics available\n", argv);
		return false;
	}

	return true;
}

/*
 * main()
 *	Invoke helper function based on command line arguments
 */
int main(int args, char **argv)
{
	char primary_shortopt[NSS_TCPST_CLI_MAX_OPT];
	int opt, option_index = 0;
	char cmd[8];

	nss_tcp_st_cli_get_short_option(primary_longopt, primary_shortopt);

	opt = getopt_long(args, argv, primary_shortopt, primary_longopt, &option_index);
	switch (opt) {
	case 'm':
		strlcpy(cmd, optarg, sizeof(cmd));
		if (!strncmp("start", cmd, strlen(cmd))) {
			system("modprobe qca-nss-netfn-tcpst");

			if (!nss_tcp_st_cli_start(args, argv)) {
				nss_tcp_st_log_error("%px:Failed to start test\n", argv);
				system("rmmod qca-nss-netfn-tcpst.ko");

				return -EINVAL;
			}

			nss_tcp_st_log_info("%px: Test started successfully\n", argv);
		} else if (!strncmp("stop", cmd, strlen(cmd))) {
			if (!nss_tcp_st_cli_stop(args, argv)) {
				nss_tcp_st_log_error("%px:Failed to stop test\n", argv);
				system("rmmod qca-nss-netfn-tcpst.ko");

				return -EINVAL;
			}

			/*
			 * Sleep to flush the previous prints
			 */
			sleep(1);
			system("rmmod qca-nss-netfn-tcpst.ko");
		} else if (!strncmp("stats", cmd, strlen(cmd))) {
			if (!nss_tcp_st_cli_get_stats(args, argv)) {
				nss_tcp_st_log_error("%px:Failed to fetch stats\n", argv);
				return -EINVAL;
			}
		}

		break;

	case 'h':
		nss_tcp_st_cli_usage();
		break;

	default:
		nss_tcp_st_log_error("%px:Invalid command\n", argv);
		nss_tcp_st_cli_usage();
		return -EINVAL;
	}

	return 0;
}
