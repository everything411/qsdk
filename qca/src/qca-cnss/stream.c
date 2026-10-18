/* Copyright (c) 2022, Qualcomm Innovation Center, Inc. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */
#include <linux/module.h>
#include <linux/errno.h>
#include <linux/socket.h>
#include <linux/udp.h>
#include <linux/types.h>
#include <linux/kernel.h>
#include <net/net_namespace.h>
#include <net/udp.h>
#include <net/udp_tunnel.h>
#include <linux/net.h>
#include <linux/inet.h>

#include "../main.h"
#ifdef CNSS_DEBUG_SUPPORT
#include "debug/debug.h"
#endif
#if defined CNSS_PCI_SUPPORT
#include "pci/pci.h"
#endif
#include "cnss_common/cnss_common.h"
#include "bus/bus.h"

extern struct cnss_plat_data *cnss_get_plat_priv_by_instance_id(
			int instance_id);
struct udp_port_cfg udp_conf;
#define INT_MODULE_PARM(n, v) static int n = v; \
				module_param(n, int, 0444)
#define STRING_MODULE_PARM(s, v) static char *s = v; \
				module_param(s, charp, 0000)

#define Q6STREAM_SIZE   (1024 * 1024)
#define TOTAL_PAGES_PER_DATA    (Q6STREAM_SIZE / PAGE_SIZE)
#define PAGES_PER_DATA  8
#define COMP_PAGES_PER_DATA (TOTAL_PAGES_PER_DATA - PAGES_PER_DATA)


/* insmod ipq_cnss2_stream.ko stream_dst_port=5004
 * stream_dst_addr="X.X.X.X" stream_src_port=5006
 * stream_src_addr="X.X.X.X instance_id=0x3a
 * stream_board_id="epryLd_IWCiRu-h9Aq9Yqg"
 */
INT_MODULE_PARM(stream_src_port, 5005);
MODULE_PARM_DESC(stream_src_port, "UDP src port");
INT_MODULE_PARM(stream_dst_port, 5004);
MODULE_PARM_DESC(stream_dst_port, "UDP dst port");
STRING_MODULE_PARM(stream_src_addr, "127.0.0.1");
MODULE_PARM_DESC(stream_src_addr, "IPv4 src address");
STRING_MODULE_PARM(stream_dst_addr, "127.0.0.1");
MODULE_PARM_DESC(stream_dst_addr, "IPv4 src address");
static char stream_board_id[32];
module_param_string(stream_board_id, stream_board_id,
		 sizeof(stream_board_id), 0);
MODULE_PARM_DESC(stream_board_id, "Boardid");
static unsigned int instance_id = 0x3a;
module_param(instance_id, uint, 0600);
MODULE_PARM_DESC(instance_id, "wlfw service instance id");

unsigned int udp_packet_no;
#define QLD_STREAM_PORT 5004
#define SEQ_NO_SIZE 4

static DEFINE_SPINLOCK(qdss_work_lock);
void qld_stream_work_hdlr(struct work_struct *work)
{
	struct qdss_stream_data *qdss_stream  = container_of(work,
							struct qdss_stream_data,
							qld_stream_work);
	struct cnss_plat_data *plat_priv = NULL;
	int ret = 0;
	int i = 0;
	struct page *transfer_page;
	void *vaddr, *next_page;
	unsigned int need_to_enable;
	unsigned long flags;
	int wrapped_around_seq_no;

	plat_priv = cnss_get_plat_priv_by_instance_id(instance_id);
	need_to_enable = 0;
	spin_lock_irqsave(&qdss_work_lock, flags);
	if (atomic_read(&qdss_stream->seq_no) >= COMP_PAGES_PER_DATA) {
		wrapped_around_seq_no = (atomic_read(&qdss_stream->seq_no) %
					TOTAL_PAGES_PER_DATA);
		need_to_enable = 1;
	}
	spin_unlock_irqrestore(&qdss_work_lock, flags);

	for (i = atomic_read(&qdss_stream->completed_seq_no);
		(i < atomic_read(&qdss_stream->seq_no))
		&& (i < TOTAL_PAGES_PER_DATA) ; i++) {

		vaddr = phys_to_virt(CNSS_ETR_SG_ENT_TO_BLK(((uint32_t *)
					qdss_stream->qdss_vaddr)[i]));
		next_page = vaddr + PAGE_SIZE;
		udp_packet_no++;
		*(unsigned int *)next_page = udp_packet_no;
		memcpy(next_page + SEQ_NO_SIZE, stream_board_id, sizeof(stream_board_id));
		dmac_flush_range((void *)next_page, (void *)next_page
					+ SEQ_NO_SIZE + sizeof(stream_board_id));
		transfer_page = virt_to_page(vaddr);
		ret = kernel_sendpage(qdss_stream->qld_stream_sock,
				transfer_page,
				0, PAGE_SIZE + SEQ_NO_SIZE + sizeof(stream_board_id),
				MSG_DONTWAIT);
		if (ret < PAGE_SIZE + SEQ_NO_SIZE)
			cnss_pr_err("ERROR: Can't send 4096 bytes %d\n", ret);
		atomic_inc(&qdss_stream->completed_seq_no);
	}

	if (need_to_enable) {
		spin_lock_irqsave(&qdss_work_lock, flags);
		if (atomic_read(&qdss_stream->completed_seq_no)
				== TOTAL_PAGES_PER_DATA) {
			atomic_set(&qdss_stream->completed_seq_no, 0);
			atomic_set(&qdss_stream->seq_no, wrapped_around_seq_no);
		}
		spin_unlock_irqrestore(&qdss_work_lock, flags);
	}
}
EXPORT_SYMBOL(qld_stream_work_hdlr);

int setup_kernel_qld_socket(struct cnss_plat_data *plat_priv)
{
	struct socket *sock;
	struct qdss_stream_data *qdss_stream;
	int err = 0;

	memset(&udp_conf, 0, sizeof(udp_conf));
	udp_conf.family = AF_INET;
	udp_conf.local_ip.s_addr = in_aton(stream_src_addr);
	udp_conf.local_udp_port = htons(stream_src_port);
	udp_conf.peer_udp_port = htons(stream_dst_port);
	udp_conf.use_udp_checksums = 1;
	udp_conf.peer_ip.s_addr = in_aton(stream_dst_addr);

	/* Open QLD stream socket */
	err = udp_sock_create(&init_net, &udp_conf, &sock);
	if (err < 0) {
		cnss_pr_err("ERROR: Could not create QLD STREAM socket\n");
		return -err;
	}
	qdss_stream = &plat_priv->qdss_stream;
	qdss_stream->qld_stream_sock = sock;
	INIT_WORK(&qdss_stream->qld_stream_work, qld_stream_work_hdlr);

	return 0;
}

static void __exit cnss_stream_fini(void)
{
	struct cnss_plat_data *plat_priv = NULL;
	struct qdss_stream_data *qdss_stream;

	plat_priv = cnss_get_plat_priv_by_instance_id(instance_id);
	if (!plat_priv) {
		cnss_pr_err("plat_priv is NULL\n");
		return;
	}
	qdss_stream = &plat_priv->qdss_stream;
	flush_work(&qdss_stream->qld_stream_work);
	kernel_sock_shutdown(qdss_stream->qld_stream_sock, SHUT_RDWR);
	sock_release(qdss_stream->qld_stream_sock);
	return;
}

static int __init cnss_stream_init(void)
{
	struct cnss_plat_data *plat_priv = NULL;

	plat_priv = cnss_get_plat_priv_by_instance_id(instance_id);

	if (!plat_priv) {
		cnss_pr_err("plat_priv is NULL\n");
		return 0;
	}
	setup_kernel_qld_socket(plat_priv);
	return 0;
}
module_init(cnss_stream_init);
module_exit(cnss_stream_fini);

MODULE_LICENSE("GPL v2");
