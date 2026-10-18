/*
 * netfn_tcpst.c
 *	Netfn TCP Speedtest Driver file
 *
 * Copyright (c) 2024-2025 Qualcomm Innovation Center, Inc. All rights reserved.
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
#include <linux/version.h>
#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/ip.h>
#include <linux/in6.h>
#include <linux/debugfs.h>
#include <linux/if_arp.h>
#include <net/inet_sock.h>
#include <net/protocol.h>
#include <net/gro.h>
#include <ppe_drv.h>

#include "netfn_tcpst_priv.h"
#include "netfn_tcpst_ppe.h"
#include "netfn_tcpst_tx.h"
#include "netfn_tcpst_rx.h"
#include "netfn_tcpst_nl.h"

/*
 * global TCP speedtest instance.
 */
struct netfn_tcpst g_tcpst;

static struct packet_offload gro_offload = {
	.type = cpu_to_be16(ETH_P_MAP),
	.priority = 1,
	.callbacks = {
		.gro_receive = NULL,
		.gro_complete = netfn_tcpst_rx_gro_comp,
	},
};

/*
 * Statistics name for netfn_tcpst_stats
 */
static int8_t *netfn_tcpst_stats_name[] = {
	"VP rx packets",
	"VP rx bytes",
	"Dst miss packets",
	"coalesce",
	"start_coalesce",
	"fail_coalesce",
	"fail_start_coalesce",
	"napi_flush",
	"Test rx packets",
	"Test rx bytes",
	"Test tx packets",
	"Test tx bytes",
};

/*
 * netfn_tcpst_stats_show()
 */
static int netfn_tcpst_stats_show(struct seq_file *m, void __attribute__((unused))*ptr)
{
	struct netfn_tcpst *tcpst = &g_tcpst;
	struct netfn_tcpst_ctx *ctx;
	int cpu;

	rcu_read_lock_bh();
	ctx = rcu_dereference_bh(tcpst->ctx);
	if (!ctx) {
		pr_warn("%px: Test is not running\n", &g_tcpst);
		rcu_read_unlock_bh();
		return 0;
	}

	seq_printf(m, "NSS TCP SpeedTest stats:\n");

	for_each_cpu(cpu, cpu_all_mask) {
		struct netfn_tcpst_conn *conn = &ctx->conn[cpu];
		struct netfn_tcpst_stats_ext *stats;
		uint64_t *stats_word;
		int i;

		if (!READ_ONCE(conn->sock)) {
			continue;
		}

		stats = &conn->ext_stats;
		stats_word = (uint64_t *)stats;

		seq_printf(m, "\t CPU %u Flow:\n", cpu);
		for (i = 0; i < sizeof(*stats) / sizeof(uint64_t); i++) {
			seq_printf(m, "\t\t [%s]:  %llu\n", netfn_tcpst_stats_name[i], stats_word[i]);
		}
	}

	rcu_read_unlock_bh();
	return 0;
}

/*
 * netfn_flowmgr_stats_open()
 *	Netfn flowmgr stats open callback
 */
static int netfn_tcpst_stats_open(struct inode *inode, struct file *file)
{
	return single_open(file, netfn_tcpst_stats_show, inode->i_private);
}

/*
 * stats callback.
 */
const struct file_operations netfn_tcpst_file_ops = {
	.open = netfn_tcpst_stats_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = seq_release,
};

/*
 * netfn_tcpst_get_result()
 *	Fetch the result.
 */
static struct netfn_tcpst_result *netfn_tcpst_get_result(struct netfn_tcpst_ctx *ctx, struct netfn_tcpst_result *result)
{
	struct netfn_tcpst_stats *stats = &result->stats;
	uint64_t min_rtt = U64_MAX;
	int cpu;

	result->error = NETFN_TCPST_ERR_NONE;
	result->state = netfn_tcpst_state_get(ctx);

	stats->test.bom_time = U64_MAX;
	stats->test.rom_time = U64_MAX;

	for_each_cpu(cpu, cpu_all_mask) {
		struct netfn_tcpst_conn *conn = &ctx->conn[cpu];
		struct netfn_tcpst_stats *conn_stats;
		uint64_t rtt;

		if (!READ_ONCE(conn->sock)) {
			continue;
		}

		conn_stats = &conn->stats;

		/*
		 * Fill values only for connection with Minimum RTT.
		 */
		rtt = (conn_stats->tcp_open.response_time - conn_stats->tcp_open.request_time);
		if (min_rtt > rtt) {
			stats->tcp_open.request_time = conn_stats->tcp_open.request_time;
			stats->tcp_open.response_time = conn_stats->tcp_open.response_time;
			min_rtt = rtt;
		}

		/*
		 * bom_time & rom_time should be of first connection connected.
		 * eom_time should be of last connection completed.
		 */
		stats->test.bom_time = min(stats->test.bom_time, conn_stats->test.bom_time);
		stats->test.rom_time = min(stats->test.rom_time, conn_stats->test.rom_time);
		stats->test.eom_time = max(stats->test.eom_time, conn_stats->test.eom_time);

		stats->test_bytes.sent += conn_stats->test_bytes.sent;
		stats->test_bytes.rcvd += conn_stats->test_bytes.rcvd;
		stats->eth_bytes.sent += conn_stats->eth_bytes.sent;
		stats->eth_bytes.rcvd += conn_stats->eth_bytes.rcvd;
	}

	/*
	 * Average out the ethernet stats
	 */
	stats->eth_bytes.sent = div_u64(stats->eth_bytes.sent, ctx->num_conns);
	stats->eth_bytes.rcvd = div_u64(stats->eth_bytes.rcvd, ctx->num_conns);

	return result;
}


/*
 * netfn_tcpst_netdev_setup()
 *	Netdev setup function.
 */
static void netfn_tcpst_netdev_setup(struct net_device *dev)
{
	dev->addr_len = ETH_ALEN;
	dev->mtu = ETH_DATA_LEN;
	dev->type = ARPHRD_VOID;
	dev->ethtool_ops = NULL;
	dev->header_ops = NULL;
	dev->netdev_ops = NULL;
	dev->priv_destructor = NULL;

	memcpy((void *)dev->dev_addr, "\x00\x00\x00\x00\x00\x00", dev->addr_len);
	memset(dev->broadcast, 0xff, dev->addr_len);
	memcpy(dev->perm_addr, dev->dev_addr, dev->addr_len);

	/*
	 * Mark the device UP for VP path.
	 * Device will not participate in any Linux datapath and hence We dont want to register with Linux using dev_open.
	 */
	dev->flags |= IFF_UP;
}

/*
 * netfn_tcpst_conn_deinit_all()
 */
static void netfn_tcpst_conn_deinit_all(struct netfn_tcpst_ctx *ctx)
{
	int cpu;

	for_each_cpu(cpu, cpu_all_mask) {
		struct netfn_tcpst_conn *conn = &ctx->conn[cpu];

		if (!READ_ONCE(conn->sock)) {
			continue;
		}

		hrtimer_cancel(&conn->hrtimer);
		netfn_tcpst_ppe_decel(conn->sock->sk, conn->eth_dev);
		kernel_sock_shutdown(conn->sock, SHUT_RDWR);
		sock_release(conn->sock);
		vfree(conn->buf);
		WRITE_ONCE(conn->sock, NULL);
	}
}

/*
 * netfn_tcpst_conn_init()
 */
static enum netfn_tcpst_err netfn_tcpst_conn_init(struct netfn_tcpst_ctx *ctx, struct netfn_tcpst_cfg *cfg, uint8_t cpu)
{
	struct sockaddr_in6 local_in6 = {0}, remote_in6 = {0};
	struct sockaddr_in local_in = {0}, remote_in = {0};
	struct netfn_tcpst_server *remote = &cfg->remote;
	struct netfn_tcpst_conn *conn = &ctx->conn[cpu];
	struct netfn_tcpst_stats *stats = &conn->stats;
	struct sockaddr *local_sa, *remote_sa;
	enum netfn_tcpst_err err;
	struct socket *sock;
	int status = 0;
	int addrlen;

	/*
	 * Allocate memory for dummy buffer used for tx/rx
	 */
	cfg->buf_len = !cfg->buf_len ? NETFN_TCPST_BUF_LEN : cfg->buf_len;
	conn->buf_len = cfg->buf_len;

	conn->buf = vzalloc(conn->buf_len);
	if (!conn->buf) {
		pr_err("%px: Failed to allocate dummy buffer\n", conn);
		return NETFN_TCPST_ERR_NOMEM;
	}

	/*
	 * Set buf memory to dummy data
	 */
	memset(conn->buf, 'c', sizeof(conn->buf));

	/*
	 * Initialize test specific parameters used in scheduled work.
	 */
	switch (cfg->test) {
	case NETFN_TCPST_TEST_HTTP_UPLOAD:
	case NETFN_TCPST_TEST_HTTP_DOWNLOAD:
		conn->http = cfg->http;
		break;

	case NETFN_TCPST_TEST_RAW_UPLOAD:
	case NETFN_TCPST_TEST_RAW_DOWNLOAD:
		conn->raw = cfg->raw;
		break;

	default:
		pr_warn("%px: Unhandled Test type (%u)", ctx, cfg->test);
		return NETFN_TCPST_ERR_PARAM;
	}

	if (remote->ip_version == IPVERSION) {
		local_in.sin_family = AF_INET;
		local_in.sin_addr.s_addr = htonl(INADDR_ANY);
		local_in.sin_port = 0;
		local_sa = (struct sockaddr *)&local_in;
		addrlen = sizeof(local_in);

		remote_in.sin_family = AF_INET;
		remote_in.sin_addr = remote->ip.v4;
		remote_in.sin_port = remote->port;
		remote_sa = (struct sockaddr *)&remote_in;
	} else {
		local_in6.sin6_family = AF_INET6;
		local_in6.sin6_addr = in6addr_any;
		local_in6.sin6_port = 0;
		local_sa = (struct sockaddr *)&local_in6;
		addrlen = sizeof(local_in6);

		remote_in6.sin6_family = AF_INET6;
		remote_in6.sin6_addr = remote->ip.v6;
		remote_in6.sin6_port = remote->port;
		remote_sa = (struct sockaddr *)&remote_in6;
	}

	/*
	 * Allocate Socket with Local address (any)
	 */
	status = sock_create_kern(&init_net, local_sa->sa_family, SOCK_STREAM, IPPROTO_TCP, &sock);
	if (status) {
		pr_err("%px: Failed to create socket(%d)\n", conn, status);
		return NETFN_TCPST_ERR_SOCK;
	}

	status = kernel_bind(sock, local_sa, addrlen);
	if (status) {
		pr_err("%px: Failed to bind socket(%d)\n", conn, status);
		err = NETFN_TCPST_ERR_BIND;
		goto free_sock;
	}

	/*
	 * Record the TCP request time
	 */
	stats->tcp_open.request_time = ktime_get_real_ns();

	/*
	 * Connect Socket to remote ip.
	 */
	status = kernel_connect(sock, remote_sa, addrlen, 0);
	if (status) {
		pr_err("%px: Failed to connect socket(%d)\n", conn, status);
		err = NETFN_TCPST_ERR_CONNECT;
		goto free_sock;
	}

	/*
	 * Record the TCP response time
	 */
	stats->tcp_open.response_time = ktime_get_real_ns();

	conn->eth_dev = netfn_tcpst_ppe_accel(sock->sk, ctx->vp_dev);
	if (!conn->eth_dev) {
		pr_err("%px: Failed to accelerate Rule", conn);
		status = NETFN_TCPST_ERR_FLOW;
		goto disconnect_sock;
	}

	WRITE_ONCE(conn->sock, sock);
	conn->duration = cfg->duration;
	conn->offset = cfg->offset;
	conn->timer_state = 0;
	conn->ctx = ctx;

	/*
	 * Connection uses timer for statistics offset and test duration control.
	 */
	hrtimer_init(&conn->hrtimer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);

	return NETFN_TCPST_ERR_NONE;

disconnect_sock:
	kernel_sock_shutdown(sock, SHUT_RDWR);
free_sock:
	sock_release(sock);
	return status;
}

/*
 * netfn_tcpst_conn_init_all()
 */
static enum netfn_tcpst_err netfn_tcpst_conn_init_all(struct netfn_tcpst_ctx *ctx, struct netfn_tcpst_cfg *cfg)
{
	cpumask_t mask = { {cfg->core_mask} };
	enum netfn_tcpst_err err;
	int cpu;

	for_each_cpu(cpu, &mask) {
		err = netfn_tcpst_conn_init(ctx, cfg, cpu);
		if (err != NETFN_TCPST_ERR_NONE) {
			pr_err("%px: Failed to alloc connection for CPU(%u), err(%d).\n", ctx, cpu, err);
			netfn_tcpst_conn_deinit_all(ctx);
			return err;
		}

		ctx->num_conns++;
		pr_debug("%px: Allocated connection for CPU(%u)\n", ctx, cpu);
	}

	return NETFN_TCPST_ERR_NONE;
}

/*
 * netfn_tcpst_ctx_alloc()
 *	Allocate the New Test Context.
 */
static struct netfn_tcpst_ctx *netfn_tcpst_ctx_alloc(struct netfn_tcpst_cfg *cfg)
{
	struct netfn_tcpst *tcpst = &g_tcpst;
	struct netfn_tcpst_ctx *ctx;
	struct ppe_vp_ai vpai = {0};
	enum netfn_tcpst_err err;

	/*
	 * Allocate a new context
	 */
	ctx = kzalloc(sizeof(*ctx), GFP_KERNEL);
	if (!ctx) {
		pr_err("%px: Failed to allocated new context\n", tcpst);
		return NULL;
	}

	/*
	 * Set the state to not running
	 */
	netfn_tcpst_state_set(ctx, NETFN_TCPST_STATE_NOT_RUNNING, netfn_tcpst_state_get(ctx));

	ctx->vp_dev = alloc_netdev(0, "tcpst", NET_NAME_ENUM, netfn_tcpst_netdev_setup);
	if (!ctx->vp_dev) {
		pr_err("%p:Failed to allocate netdevice\n", ctx);
		goto free_ctx;
	}

	/*
	 * Allocate PPE VP
	 */
	vpai.type = PPE_VP_TYPE_SW_L3;
	vpai.usr_type = PPE_VP_USER_TYPE_ACTIVE;
	vpai.core_mask = cfg->core_mask;
	vpai.dst_cb_data = ctx;
	vpai.src_cb = NULL;
	vpai.src_cb_data = NULL;
	vpai.dst_cb = (cfg->remote.ip_version == IPVERSION) ? netfn_tcpst_rx_vp4 : netfn_tcpst_rx_vp6;

	ctx->vp_num = ppe_vp_alloc(ctx->vp_dev, &vpai);
	if (ctx->vp_num < 0) {
		pr_err("%px: Failed to allocate VP, err(%d).\n", ctx, vpai.status);
		goto free_netdev;
	}

	err = netfn_tcpst_conn_init_all(ctx, cfg);
	if (err != NETFN_TCPST_ERR_NONE) {
		pr_err("%px: Failed to Initialize socket, err(%d).\n", ctx, err);
		goto free_vp;
	}

	/*
	 * Work queue for HTTP work.
	 */
	ctx->wq = alloc_workqueue("netfn_tcpst", WQ_HIGHPRI | WQ_CPU_INTENSIVE, 0);
	if (!ctx->wq) {
		pr_err("%p:Failed to allocate workqueue\n", tcpst);
		goto deinit_all;
	}

	return ctx;

deinit_all:
	netfn_tcpst_conn_deinit_all(ctx);
free_vp:
	ppe_vp_free(ctx->vp_num);
free_netdev:
	free_netdev(ctx->vp_dev);
free_ctx:
	kfree(ctx);
	return NULL;
}

/*
 * netfn_tcpst_ctx_free()
 *	Free the test context
 */
static void netfn_tcpst_ctx_free(struct netfn_tcpst_ctx *ctx)
{
	destroy_workqueue(ctx->wq);
	netfn_tcpst_conn_deinit_all(ctx);
	ppe_vp_free(ctx->vp_num);
	dev_remove_offload(&gro_offload);
	free_netdev(ctx->vp_dev);
	kfree(ctx);
}

/*
 * netfn_tcpst_get_stats()
 *	Fetch the current statistics.
 */
enum netfn_tcpst_state netfn_tcpst_get_stats(struct netfn_tcpst_stats *stats)
{
	struct netfn_tcpst *tcpst = &g_tcpst;
	enum netfn_tcpst_state state;
	struct netfn_tcpst_ctx *ctx;
	int cpu;

	rcu_read_lock_bh();
	ctx = rcu_dereference_bh(tcpst->ctx);
	if (!ctx) {
		rcu_read_unlock_bh();
		return NETFN_TCPST_STATE_NOT_RUNNING;
	}

	for_each_cpu(cpu, cpu_all_mask) {
		struct netfn_tcpst_conn *conn = &ctx->conn[cpu];
		struct netfn_tcpst_stats_ext *conn_stats;
		if (!READ_ONCE(conn->sock)) {
			continue;
		}

		conn_stats = &conn->ext_stats;
		stats->test_bytes.sent += conn_stats->test_tx_bytes;
		stats->test_bytes.rcvd += conn_stats->test_rx_bytes;
	}

	state = netfn_tcpst_state_get(ctx);
	rcu_read_unlock_bh();

	return state;
}
EXPORT_SYMBOL(netfn_tcpst_get_stats);

/*
 * netfn_tcpst_start()
 *	Start the Test.
 */
enum netfn_tcpst_state netfn_tcpst_start(struct netfn_tcpst_cfg *cfg)
{
	enum netfn_tcpst_state state = NETFN_TCPST_STATE_NOT_RUNNING;
	void (*work_cb)(struct work_struct *w);
	struct netfn_tcpst *tcpst = &g_tcpst;
	cpumask_t mask = { {cfg->core_mask} };
	struct netfn_tcpst_ctx *ctx;
	int cpu;

	/*
	 * Check if we already have a test running
	 */
	mutex_lock(&tcpst->lock);
	if (rcu_dereference_protected(tcpst->ctx, 1)) {
		pr_err("%px: Test is already running, Stop the running test before starting new\n", tcpst);
		goto unlock;
	}

	/*
	 * Input Sanity check.
	 */
	if (!cfg->conn || !cfg->core_mask) {
		pr_err("%px: minimum one Connection(%u) and  CPU(%u) required.\n", tcpst, cfg->conn, cfg->core_mask);
		goto unlock;
	}

	switch (cfg->test) {
	case NETFN_TCPST_TEST_HTTP_UPLOAD:
	case NETFN_TCPST_TEST_RAW_UPLOAD:
		work_cb = netfn_tcpst_tx_work;
		break;

	case NETFN_TCPST_TEST_HTTP_DOWNLOAD:
	case NETFN_TCPST_TEST_RAW_DOWNLOAD:
		work_cb = netfn_tcpst_rx_work;
		break;

	default:
		pr_warn("%px: Unhandled Test type (%u)", ctx, cfg->test);
		return NETFN_TCPST_STATE_NOT_RUNNING;
	}

	if (cfg->conn > hweight32(cfg->core_mask)) {
		pr_warn("%px: Only one connection per CPU is supported\n", tcpst);
		cfg->conn = hweight32(cfg->core_mask);
	}

	ctx = netfn_tcpst_ctx_alloc(cfg);
	if (!ctx) {
		pr_err("%px: Test Context allocation failed\n", tcpst);
		goto unlock;
	}

	ctx->test = cfg->test;
	rcu_assign_pointer(tcpst->ctx, ctx);

	/*
	 * Register GRO offload handler.
	 */
	dev_add_offload(&gro_offload);

	/*
	 * Mark test in progress only after everything initialized
	 */
	netfn_tcpst_state_set(ctx, NETFN_TCPST_STATE_INPROGRESS, NETFN_TCPST_STATE_NOT_RUNNING);
	state = netfn_tcpst_state_get(ctx);

	/*
	 * Schedule the Tx/Rx work.
	 */
	for_each_cpu(cpu, &mask) {
		struct work_struct *work = &ctx->conn[cpu].work;
		INIT_WORK(work, work_cb);
		queue_work_on(cpu, ctx->wq, work);
	}

unlock:
	mutex_unlock(&tcpst->lock);
	return state;
}
EXPORT_SYMBOL(netfn_tcpst_start);

/*
 * netfn_tcpst_stop()
 *	Stop the Test.
 */
enum netfn_tcpst_state netfn_tcpst_stop(netfn_tcpst_comp_t cb, void *app_data)
{
	struct netfn_tcpst *tcpst = &g_tcpst;
	struct netfn_tcpst_result res = {0};
	struct netfn_tcpst_ctx *ctx;
	int cpu;

	mutex_lock(&tcpst->lock);
	ctx = rcu_dereference_protected(tcpst->ctx, 1);
	if (!ctx) {
		pr_info("%px: Test is not running\n", tcpst);

		mutex_unlock(&tcpst->lock);
		return NETFN_TCPST_STATE_NOT_RUNNING;
	}

	ctx->vp_dev->flags &= ~IFF_UP;
	RCU_INIT_POINTER(tcpst->ctx, NULL);
	mutex_unlock(&tcpst->lock);
	synchronize_net();

	/*
	 * Wait for all test work to finish.
	 * It waits for the workers to complete incase of chaining as well.
	 */
	for_each_cpu(cpu, cpu_all_mask) {
		struct netfn_tcpst_conn *conn = &ctx->conn[cpu];

		if (!READ_ONCE(conn->sock)) {
			continue;
		}

		kernel_sock_shutdown(conn->sock, SHUT_RDWR);
		cancel_work_sync(&conn->work);
	}

	/*
	 * Send the results back to userspace
	 */
	if (cb)
		cb(app_data, netfn_tcpst_get_result(ctx, &res));

	/*
	 * Set state to stopped
	 */
	netfn_tcpst_state_set(ctx, NETFN_TCPST_STATE_STOPPED, netfn_tcpst_state_get(ctx));

	/*
	 * We are done with the test, Free the context.
	 */
	netfn_tcpst_ctx_free(ctx);

	return NETFN_TCPST_STATE_STOPPED;
}
EXPORT_SYMBOL(netfn_tcpst_stop);

/*
 * netfn_tcpst_init_module()
 *	Module initialization
 */
int __init netfn_tcpst_init_module(void)
{
	struct netfn_tcpst *tcpst = &g_tcpst;

	tcpst->dentry = debugfs_create_dir("qca-nss-netfn-tcpst", NULL);
	if (!tcpst->dentry) {
		pr_err("%px: Failed to create debugfs\n", tcpst);
		return -1;
	}

	debugfs_create_file("stats", S_IRUGO, tcpst->dentry, tcpst, &netfn_tcpst_file_ops);
	mutex_init(&tcpst->lock);

	if (!netfn_tcpst_nl_register()) {
                pr_err("%px: Failed to initialize TCP ST netlink family\n", tcpst);
		debugfs_remove(tcpst->dentry);
                return -1;
        }

	pr_info("NSS TCP Speedtest loaded: (%s)\n", NSS_NETFN_BUILD_ID);
	return 0;
}

/*
 * netfn_tcpst_exit_module()
 *	Module exit cleanup
 */
void __exit netfn_tcpst_exit_module(void)
{
	struct netfn_tcpst *tcpst = &g_tcpst;
	struct netfn_tcpst_ctx *ctx;

	ctx = rcu_dereference_protected(tcpst->ctx, 1);
	if (ctx) {
		pr_warn("%px: Module unloaded while test is running\n", ctx);
		netfn_tcpst_stop(NULL, NULL);
	}

	netfn_tcpst_nl_unregister();

	/*
	 * Remove debug entry
	 */
	debugfs_remove(tcpst->dentry);
	pr_info("NSS TCP Speedtest module unloaded %s\n", NSS_NETFN_BUILD_ID);
}

MODULE_LICENSE("Dual BSD/GPL");
MODULE_DESCRIPTION("NSS NETFN TCPST client");

module_init(netfn_tcpst_init_module);
module_exit(netfn_tcpst_exit_module);
