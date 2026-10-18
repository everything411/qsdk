/*
 * Copyright (c) 2024, Qualcomm Innovation Center, Inc. All rights reserved.
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

#ifndef __NETFN_PKT_STEER_H
#define __NETFN_PKT_STEER_H

/*
 * Packet steer per PCU handle
 */
struct netfn_pkt_steer_pcpu;
struct netfn_pkt_steer;
typedef void (*netfn_pkt_steer_recv_t)(struct netfn_pkt_steer *ps, struct sk_buff_head *q_head);

/*
 * netfn_pkt_steer_info
 */
struct netfn_pkt_steer_info {
	netfn_pkt_steer_recv_t cb;	/* Callback to execute on remote CPU */
	struct net_device *dev;		/* Netdevice of the caller real or dummy */
	uint16_t budget;		/* Budget for remote CPU process */
	uint16_t weight;		/* weight for each queue */
};

/*
 * netfn_pkt_steer_stats
 */
struct netfn_pkt_steer_stats {
	uint64_t fifo_full;		/* Enqueue failed due to FIFO full */
	uint64_t fifo_enq;		/* Total packets enqueued */
	uint64_t fifo_dq;		/* Total packets dequeued */
	uint64_t spare[2];		/* Reserved stats */
};

/*
 * netfn_pkt_steer
 * 	Per Core NAPI structure.
 */
struct netfn_pkt_steer {
	struct netfn_pkt_steer_info info;	/* Info passed during init */
	struct netfn_pkt_steer_pcpu __rcu __percpu *pcpu;	/* Per CPU NAPI entry */
};

/*
 * netfn_pkt_steer_init
 *	Allocate packet steer object.
 *
 * @datatypes
 * struct netfn_pkt_steer
 * netfn_pkt_steer_info
 *
 * @param[in] ps	Object to initialize
 * @param[in] info	Packet Steering information.
 *
 * @return
 * void
 */
void netfn_pkt_steer_init(struct netfn_pkt_steer *ps, struct netfn_pkt_steer_info *info);

/*
 * netfn_pkt_steer_deinit
 *	Deinitialize packet steer object.
 *
 * @datatypes
 * struct netfn_pkt_steer
 *
 * @param[in] ps	Object to initialize.
 *
 * @return
 * void
 */
void netfn_pkt_steer_deinit(struct netfn_pkt_steer *ps);

/*
 * netfn_pkt_steer_enable
 *	Enable steer object.
 *
 * @datatypes
 * struct netfn_pkt_steer
 *
 * @param[in] ps	Object to enable.
 *
 * @return
 * True if succesfully enabled.
 */
bool netfn_pkt_steer_enable(struct netfn_pkt_steer *ps);

/*
 * netfn_pkt_steer_disable()
 *	Disable packet steer object.
 *
 * @datatypes
 * struct netfn_pkt_steer
 *
 * @param[in] ps	Object to disable.
 *
 * @return
 * void
 */
void netfn_pkt_steer_disable(struct netfn_pkt_steer *ps);

/*
 * netfn_pkt_steer_send
 *	Send SKB to remote cpu. Caller needs to free any non-send SKBs.
 *
 * @datatypes
 * struct netfn_pkt_steer
 *
 * @param[in] ps	Packet steer object on which skb will be queued.
 * @param[in] skb	SKB to send.
 * @param[in] cpu	Packet will be steer to this CPU.
 *
 * @return
 * 1 if packets enqueued.
 */
int netfn_pkt_steer_send(struct netfn_pkt_steer *ps, struct sk_buff *skb, int cpu);

/*
 * netfn_pkt_steer_send_list
 *	Send list of SKB to remote cpu. Caller needs to free any non-send SKBs.
 *
 * @datatypes
 * struct netfn_pkt_steer
 * sk_buff_head
 *
 * @param[in] ps	Packet steer object on which skb_head will be queued.
 * @param[in] head	SKB list head.
 * @param[in] cpu	Packet will be steer to this CPU.
 * @param[out] tx_bytes	Enqueued bytes.
 *
 * @return
 * Number of packets enqueued/consumed. Balance SKBs in the queue should be freed by caller.
 */
int netfn_pkt_steer_send_list(struct netfn_pkt_steer *ps, struct sk_buff_head *head, int cpu, uint64_t *tx_bytes);

/*
 * netfn_pkt_steer_get_stats
 *	Get Queue statistic for given handle.
 *
 * @datatypes
 * netfn_pkt_steer_t
 * netfn_pkt_steer_stats
 *
 * @param[in] ps	Packet steer obdject.
 * @param[out] stats 	Memory to dump statistics.
 */
void netfn_pkt_steer_get_stats(struct netfn_pkt_steer *ps, struct netfn_pkt_steer_stats *stats);

#endif /* __NETFN_PKT_STEER_H */
