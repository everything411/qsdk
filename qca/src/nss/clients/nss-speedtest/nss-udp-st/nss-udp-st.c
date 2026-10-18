/*
 **************************************************************************
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: ISC
 **************************************************************************
 */

/*
 * @file NSS UDP Speetest App
 */

#include <string.h>
#include <getopt.h>
#include <stdbool.h>
#include <arpa/inet.h>
#include "nss-udp-st.h"

struct nss_udp_st_param st_param;
struct nss_udp_st_opt st_opt;
struct nss_udp_st_cfg st_cfg;
struct nss_udp_st_stat st_stat;

struct option long_options[] =
{
	{"mode", required_argument, NULL, 'm'},
	{"type", required_argument, NULL, 'x'},
	{"sip", required_argument, NULL, 's'},
	{"dip", required_argument, NULL, 'd'},
	{"sport", required_argument, NULL, 'y'},
	{"dport", required_argument, NULL, 'z'},
	{"net_dev", required_argument, NULL, 'n'},
	{"version", required_argument, NULL, 'f'},
	{"time", required_argument, NULL, 't'},
	{"rate", required_argument, NULL, 'r'},
	{"buffer_sz", required_argument, NULL, 'b'},
	{"dscp", required_argument, NULL, 'c'},
	{"timestamp", no_argument, NULL, '0'},
	{"vp", no_argument, NULL, 'v'},
	{"hw_offload", no_argument, NULL, 'p'},
	{"help", no_argument, NULL, 'h'},
	{"cpu_bitmap", required_argument, NULL, 'u'},
	{"burst", required_argument, NULL, 'g'},
	{0, 0, 0, 0}
};

/*
 * nss_udp_st_init()
 *	Load nss speedtest driver and initialize test parameters
 */
static int nss_udp_st_init(void)
{
	int ret = 0;

	mkdir(NSS_UDP_ST_LOG, 0777);
	ret = ioctl(st_cfg.handle, NSS_UDP_ST_IOCTL_INIT, &st_param);
	if (ret < 0) {
		printf("ioctl error %d\n", ret);
		return -EINVAL;
	}

	return 0;
}

/*
 * nss_udp_st_final()
 *	Unload nss speedtest driver
 */
static int nss_udp_st_final(void)
{
	int ret = 0;

	ret = ioctl(st_cfg.handle, NSS_UDP_ST_IOCTL_FINAL);
	if (ret < 0) {
		printf("ioctl error %d\n", ret);
		return -EINVAL;
	}

	return 0;
}

/*
 * nss_udp_st_rule_check()
 *	Checks if configured rule is valid
 */
static bool nss_udp_st_rule_check(void)
{
	struct in_addr addr;
	struct in6_addr addr6;

	if (st_opt.ip_version == 4) {
		if (inet_pton(AF_INET, st_opt.sip, &addr) != 1) {
			printf("Invalid Source IP Address %s\n", st_opt.sip);
			return false;
		} else if (inet_pton(AF_INET, st_opt.dip, &addr) != 1) {
			printf("Invalid Destination IP Address %s\n", st_opt.dip);
			return false;
		}

	} else if (st_opt.ip_version == 6) {
		if (inet_pton(AF_INET6, st_opt.sip, &addr6) != 1) {
			printf("Invalid Source IP Address %s\n", st_opt.sip);
			return false;
		} else if (inet_pton(AF_INET6, st_opt.dip, &addr6) != 1) {
			printf("Invalid Destination IP Address %s\n", st_opt.dip);
			return false;
		}

	} else {
		printf("Invalid ip version %d\n", st_opt.ip_version);
		return false;
	}

	if (st_opt.sport == 0) {
		printf("Rule create failure due to invalid source port\n");
		return false;
	} else if (st_opt.dport == 0) {
		printf("Exception case: dport set to 0\n");
	}

	return true;
}

/*
 * nss_udp_st_create()
 *	Configure NSS UDP speedtest rules
 */
static int nss_udp_st_create(void)
{
	FILE *fp = NULL;

	if (!nss_udp_st_rule_check()) {
		printf("Failed to add invalid rule\n");
		return -EINVAL;
	}

	fp = fopen(NSS_UDP_ST_RULES, "a+");
	if (!fp) {
		printf("create/open file error\n");
		return -EINVAL;
	}

	fprintf(fp, "sip=<%s> dip=<%s> sport=<%d> dport=<%d> version=<%d>\n",
	st_opt.sip, st_opt.dip, st_opt.sport, st_opt.dport, st_opt.ip_version);

	fclose(fp);
	return 0;
}

/*
 * nss_udp_st_list()
 *	List NSS UDP speedtest rules
 */
static int nss_udp_st_list(void)
{
	FILE *fp = NULL;
	char *buffer = NULL;
	size_t size = 0;

	fp = fopen(NSS_UDP_ST_RULES, "r");
	if (!fp) {
		printf("create/open file error\n");
		return -EINVAL;
	}

	while ((getline(&buffer, &size, fp)) != -1) {
		printf("\n%s", buffer);
	}

	free(buffer);
	fclose(fp);
	return 0;
}

/*
 * nss_udp_st_clear()
 *	Clear NSS UDP speedtest rules
 */
static int nss_udp_st_clear(void)
{
	FILE *fp = NULL;

	fp = fopen(NSS_UDP_ST_RULES, "w");
	if (!fp) {
		printf("create/open file error\n");
		return -EINVAL;
	}

	fclose(fp);
	return 0;
}

/*
 * nss_udp_st_push_rules()
 *	Push NSS UDP speedtest rules to driver
 */
static int nss_udp_st_push_rules(void)
{
	FILE *fp = NULL;
	char *buffer = NULL;
	char *token = NULL;
	char *saveptr = NULL;
	size_t size = 0;
	int count = 0;
	int ret = 0;
	bool flag = true;

	fp = fopen(NSS_UDP_ST_RULES, "r");
	if (!fp) {
		printf("create/open file error\n");
		return -EINVAL;
	}

	while ((getline(&buffer, &size, fp)) != -1) {
		flag = true;
		count = 0;
		token=strtok_r(buffer, "<>", &saveptr);
		memset(&st_opt, 0, sizeof(st_opt));
		while(token != NULL) {
			flag = !flag;
			if (flag) {
				if (count == NSS_UDP_ST_SIP)
					strlcpy(st_opt.sip, token, sizeof(st_opt.sip));
				if (count == NSS_UDP_ST_DIP)
					strlcpy(st_opt.dip, token, sizeof(st_opt.dip));
				if (count == NSS_UDP_ST_SPORT)
					st_opt.sport=atoi(token);
				if (count == NSS_UDP_ST_DPORT)
					st_opt.dport=atoi(token);
				if (count == NSS_UDP_ST_FLAGS)
					st_opt.ip_version=atoi(token);

				count ++;
			}
			token = strtok_r(NULL, "<>", &saveptr);
		}

		ret = write(st_cfg.handle, &st_opt, sizeof(st_opt));
		if (ret < 0) {
			printf("invalid rule config option\n");
			printf("write error %d\n",ret);
			free(buffer);
			fclose(fp);
			return -EINVAL;
		}
	}

	free(buffer);
	fclose(fp);
	return 0;
}

/*
 * nss_udp_st_start()
 *	Send ioctl to start NSS UDP speedtest
 */
static int nss_udp_st_start(void)
{
	int ret = nss_udp_st_push_rules();
	if (ret) {
		printf("invalid rule config option\n");
		return -EINVAL;
	}

	if (st_cfg.type == NSS_UDP_ST_TX) {
		ret = ioctl(st_cfg.handle, NSS_UDP_ST_IOCTL_START_TX, &st_cfg.time);
	} else if (st_cfg.type == NSS_UDP_ST_RX) {
		ret = ioctl(st_cfg.handle, NSS_UDP_ST_IOCTL_START_RX);
	} else {
		printf("invalid type option\n");
		return -EINVAL;
	}

	if (ret < 0) {
		printf("ioctl error %d\n", ret);
		return -EINVAL;
	}

	return 0;
}

/*
 * nss_udp_st_stop()
 *	Send ioctl to stop NSS USP speedtest
 */
static int nss_udp_st_stop(void)
{
	int ret = ioctl(st_cfg.handle, NSS_UDP_ST_IOCTL_STOP);
	if (ret < 0) {
		printf("ioctl error %d\n", ret);
		return -EINVAL;
	}

	return 0;
}

/*
 * nss_udp_st_rate_change()
 *	Send ioctl to change rate of NSS USP speedtest
 */
static int nss_udp_st_rate_change(void)
{
	int ret = ioctl(st_cfg.handle, NSS_UDP_ST_IOCTL_RATE_CHANGE, &st_param.rate);
	if (ret < 0) {
		printf("ioctl error %d\n", ret);
		return -EINVAL;
	}

	return 0;
}

/*
 * nss_udp_st_reset_stats()
 *	Send ioctl to reset stats of NSS UDP speedtest
 */
static int nss_udp_st_reset_stats(void)
{
	int ret = ioctl(st_cfg.handle, NSS_UDP_ST_IOCTL_RESET_STATS);
	if (ret < 0) {
		printf("ioctl error %d\n", ret);
		return -EINVAL;
	}

	return 0;
}

/*
 * nss_udp_st_stats()
 *	Read NSS UDP speedtest results
 */
static int nss_udp_st_stats(void)
{
	int ret = 0;
	FILE *fp = NULL;
	long long bytes = 0;

	ret = read(st_cfg.handle, &st_stat, sizeof(st_stat));
	if (ret < 0) {
		printf("read error %d\n",ret);
		return -EINVAL;
	}

	if (st_cfg.type == NSS_UDP_ST_TX) {
		fp = fopen(NSS_UDP_ST_TX_STATS, "w");
		if (!fp) {
			printf("create/open file error\n");
			return -EINVAL;
		}

		fprintf(fp, "\nPacket Stats\n");
		fprintf(fp, "\ttx_packets = %lld packets\n", st_stat.p_stats.tx_packets);
		fprintf(fp, "\ttx_bytes   = %lld bytes\n", st_stat.p_stats.tx_bytes);

		bytes = st_stat.p_stats.tx_bytes;
	} else if (st_cfg.type == NSS_UDP_ST_RX) {
		fp = fopen(NSS_UDP_ST_RX_STATS, "w");
		if (!fp) {
			printf("create/open file error\n");
			return -EINVAL;
		}

		fprintf(fp, "\nPacket Stats\n\n");
		fprintf(fp, "\trx_packets = %lld packets\n", st_stat.p_stats.rx_packets);
		fprintf(fp, "\trx_bytes   = %lld bytes\n", st_stat.p_stats.rx_bytes);

		bytes = st_stat.p_stats.rx_bytes;
	} else {
		printf("type error\n");
		return -EINVAL;
	}

	fprintf(fp, "\nTime Stats\n");
	fprintf(fp, "\tstart time = %lld ms\n",st_stat.timer_stats[NSS_UDP_ST_STATS_TIME_START]);
	fprintf(fp, "\tcapture time = %lld ms\n",
		st_stat.timer_stats[NSS_UDP_ST_STATS_TIME_CURRENT]);
	fprintf(fp, "\telapsed time = %lld ms\n",
		st_stat.timer_stats[NSS_UDP_ST_STATS_TIME_ELAPSED]);

	fprintf(fp, "\nError Stats\n");
	fprintf(fp, "\tincorrect rate = %lld\n",
		st_stat.errors[NSS_UDP_ST_ERROR_INCORRECT_RATE]);
	fprintf(fp, "\tincorrect buffer size = %lld\n",
		st_stat.errors[NSS_UDP_ST_ERROR_INCORRECT_BUFFER_SIZE]);
	fprintf(fp, "\tmemory failure = %lld\n",
		st_stat.errors[NSS_UDP_ST_ERROR_MEMORY_FAILURE]);
	fprintf(fp, "\tpacket drop count = %lld\n",
		st_stat.errors[NSS_UDP_ST_ERROR_PACKET_DROP]);
	fprintf(fp, "\tincorrect ip version = %lld\n",
		st_stat.errors[NSS_UDP_ST_ERROR_INCORRECT_IP_VERSION]);

	fprintf(fp, "\nThroughput Stats\n");
	fprintf(fp, "\taverage throughput  = %lld Mbps\n",
		(bytes * 8)/(st_stat.timer_stats[NSS_UDP_ST_STATS_TIME_ELAPSED] * 1000));

	if (st_cfg.type == NSS_UDP_ST_RX) {
		fprintf(fp, "\nLatency Stats\n");
		fprintf(fp, "\tdropped packets  = %lld pkts\n",
			st_stat.p_stats.dropped);
		fprintf(fp, "\tout of order packets  = %lld pkts\n",
			st_stat.p_stats.ooo);
		fprintf(fp, "\taverage latency  = %lld ms\n",
			(st_stat.total_latency) / st_stat.p_stats.rx_packets);
		fprintf(fp, "\ttotal latency  = %lld ms\n",
			st_stat.total_latency);
		fprintf(fp, "\tminimum latency  = %lld ms\n",
			st_stat.p_stats.min_latency);
		fprintf(fp, "\tmaximum latency  = %lld ms\n",
			st_stat.p_stats.max_latency);
	}

	fclose(fp);
	return 0;
}

/*
 * nss_udp_st_usage()
 *	Usage for command line arguments
 */
static void nss_udp_st_usage(void)
{
	printf("\nUsage:");
	printf("\n./nss_udp_st --mode <init> --rate <rate in Mbps> \
		--buffer_sz <buffer_size in bytes> --burst burst_number --dscp <dscp> --net_dev <net_dev> --timestamp");
	printf("\n./nss_udp_st --mode <create> --sip <sip> --dip <dip> \
		--sport <sport> --dport <dport> --version <4/6>");
	printf("\n./nss_udp_st --mode <start> --type <tx/rx> --time <time in seconds>");
	printf("\n./nss_udp_st --mode <stats> --type <tx/rx>");
	printf("\n./nss_udp_st --mode <reset_stats>");
	printf("\n./nss_udp_st --mode <rate_change> --rate <rate in Mbps>");
	printf("\n./nss_udp_st --mode <list/clear/final>");
}

/*
 * nss_udp_st_parse_uint32()
 *	Parse str into *value, rejecting empty, negative or non-numeric input
 */
static bool nss_udp_st_parse_uint32(const char *str, uint32_t *value)
{
	unsigned long val;
	char *endptr;

	if (!str || *str == '\0' || str[0] == '-') {
		return false;
	}

	val = strtoul(str, &endptr, 10);
	if (*endptr != '\0' || val > UINT32_MAX) {
		return false;
	}

	*value = (uint32_t)val;
	return true;
}

/*
 * nss_udp_st_max_cpu_bitmap()
 *	Return the max cpu_bitmap value allowed as per number of cores
 */
static uint32_t nss_udp_st_max_cpu_bitmap(void)
{
	long ncpus = sysconf(_SC_NPROCESSORS_ONLN);

	if (ncpus <= 0) {
		ncpus = 1;
	}

	return (uint32_t)((1UL << ncpus) - 1);
}

/*
 * nss_udp_st_get_opt()
 *	Parse command line arguments
 */
static int nss_udp_st_get_opt(int args, char **argv)
{
	int c = 0;
	int option_index = 0;
	uint64_t port_tmp;
	char *endptr;

	while (1) {
		c = getopt_long_only(args, argv, "m:x:s:d:y:z:n:f:t:r:b:c:0:u:g",
			long_options, &option_index);
		if (c == -1)
			break;

		switch (c) {
		case 'm':
			strlcpy(st_cfg.mode, optarg, sizeof(st_cfg.mode));
			break;

		case 'x':
			if (!strcmp(optarg, "tx")) {
				st_cfg.type = NSS_UDP_ST_TX;
			} else if (!strcmp(optarg, "rx")) {
				st_cfg.type = NSS_UDP_ST_RX;
			} else {
				printf("invalid option for type (tx/rx)\n");
				return -EINVAL;
			}
			break;

		case 's':
			strlcpy(st_opt.sip, optarg, sizeof(st_opt.sip));
			break;

		case 'd':
			strlcpy(st_opt.dip, optarg, sizeof(st_opt.dip));
			break;

		case 'y':
			/*
			 * First assign port to 64 bit variable,
			 * if initial assignment is done to 16 bit
			 * variable there may be wraparound if the
			 * inputted port is greater than UINT16_MAX,
			 * causing unexpected behavior
			 */
			port_tmp = strtol(optarg, &endptr, 10);
			if (*endptr != '\0') {
				printf("Invalid source port %s\n", optarg);
				st_opt.sport = 0;
				break;
			}

			if (port_tmp > UINT16_MAX) {
				printf("Invalid source port %"PRIu64"\n", port_tmp);
				st_opt.sport = 0;
				break;
			}

			st_opt.sport = port_tmp;
			break;

		case 'z':
			/*
			 * First assign port to 64 bit variable,
			 * if initial assignment is done to 16 bit
			 * variable there may be wraparound if the
			 * inputted port is greater than UINT16_MAX,
			 * causing unexpected behavior
			 */
			port_tmp = strtol(optarg, &endptr, 10);
			if (*endptr != '\0') {
				printf("Invalid destination port %s\n", optarg);
				st_opt.dport = 0;
				break;
			}
			if (port_tmp > UINT16_MAX) {
				printf("Invalid destination port %"PRIu64"\n", port_tmp);
				st_opt.dport = 0;
				break;
			}

			st_opt.dport = port_tmp;
			break;

		case 'n':
			strlcpy(st_param.net_dev, optarg, sizeof(st_param.net_dev));
			break;

		case 'f':
			st_opt.ip_version = atoi(optarg);
			break;

		case 't':
			if (st_cfg.type == NSS_UDP_ST_RX) {
				printf("Time duration cannot be configured for RX driver, this value will be igorned: %s\n", optarg);
				printf("Rx test can be stopped using nss-udp-st --mode stop\n");
				break;
			}
			st_cfg.time = atoi(optarg);
			break;

		case 'r':
			st_param.rate = atoi(optarg);
			break;

		case 'b':
			if (!nss_udp_st_parse_uint32(optarg, &st_param.buffer_sz)) {
				printf("Invalid buffer size %s\n", optarg);
				return -EINVAL;
			}
			break;

		case 'c':
			st_param.dscp = atoi(optarg);
			break;

		case '0':
			st_param.flags |= NSS_UDP_ST_FLAGS_TIMESTAMP;
			break;

		case 'v':
			st_param.flags |= NSS_UDP_ST_FLAGS_VP;
			break;

		case 'p':
			st_param.flags |= NSS_UDP_ST_FLAGS_HW_OFFLOAD;
			break;

		case 'h':
			nss_udp_st_usage();
			break;

		case 'u': {
			uint32_t max_bitmap = nss_udp_st_max_cpu_bitmap();

			if (!nss_udp_st_parse_uint32(optarg, &st_param.cpu_bitmap)) {
				printf("Invalid cpu bitmap %s\n", optarg);
				return -EINVAL;
			}

			if (st_param.cpu_bitmap > max_bitmap) {
				printf("Invalid cpu bitmap %u. Max allowed: %u\n", st_param.cpu_bitmap, max_bitmap);
				return -EINVAL;
			}
			break;
		}

		case 'g':
			st_param.burst_size = atoi(optarg);
			break;

		default:
			nss_udp_st_usage();
			return -EINVAL;
		}
	}
	return 0;
}

/*
 * main()
 *	Invoke helper function based on command line arguments
 */
int main(int args, char **argv)
{

	memset(&st_param, 0, sizeof(st_param));
	memset(&st_opt, 0, sizeof(st_opt));
	memset(&st_cfg, 0, sizeof(st_cfg));
	memset(&st_stat, 0, sizeof(st_stat));

	if (nss_udp_st_get_opt(args, argv)) {
		printf("invalid command line options\n");
		return -EINVAL;
	}

	if (!strcmp(st_cfg.mode,"init")) {
		system("insmod nss-udp-st.ko");

		/*
		 * Sleep prevents race condition between
		 * module load and ioctl call
		 */
		sleep(1);
		st_cfg.handle = open(NSS_UDP_ST_DEV, O_RDWR);
		nss_udp_st_init();
		close(st_cfg.handle);
	} else if (!strcmp(st_cfg.mode,"final")) {
		st_cfg.handle = open(NSS_UDP_ST_DEV, O_RDWR);
		nss_udp_st_final();
		close(st_cfg.handle);
		sleep(1);
		system("rmmod nss-udp-st.ko");
	} else if (!strcmp(st_cfg.mode,"create")) {
		nss_udp_st_create();
	} else if (!strcmp(st_cfg.mode,"list")) {
		nss_udp_st_list();
	} else if (!strcmp(st_cfg.mode,"clear")) {
		nss_udp_st_clear();
		/*
		 * Reset kernel stats so PPE stats do not carry over
		 * to the next test run.  Open the device only if the module
		 * is already loaded; ignore failure if it is not.
		 */
		st_cfg.handle = open(NSS_UDP_ST_DEV, O_RDWR);
		if (st_cfg.handle >= 0) {
			nss_udp_st_reset_stats();
			close(st_cfg.handle);
		}
	} else if (!strcmp(st_cfg.mode,"start")) {
		st_cfg.handle = open(NSS_UDP_ST_DEV, O_RDWR);
		nss_udp_st_start();
		close(st_cfg.handle);
	} else if (!strcmp(st_cfg.mode,"stop")) {
		st_cfg.handle = open(NSS_UDP_ST_DEV, O_RDWR);
		nss_udp_st_stop();
		close(st_cfg.handle);
	} else if (!strcmp(st_cfg.mode,"rate_change")) {
		st_cfg.handle = open(NSS_UDP_ST_DEV, O_RDWR);
		nss_udp_st_rate_change();
		close(st_cfg.handle);
	} else if (!strcmp(st_cfg.mode,"reset_stats")) {
		st_cfg.handle = open(NSS_UDP_ST_DEV, O_RDWR);
		nss_udp_st_reset_stats();
		close(st_cfg.handle);
	} else if (!strcmp(st_cfg.mode,"stats")) {
		st_cfg.handle = open(NSS_UDP_ST_DEV, O_RDWR);
		nss_udp_st_stats();
		close(st_cfg.handle);
	} else {
		printf("invalid command line options for mode\n");
		return -EINVAL;
	}
	return 0;
}
