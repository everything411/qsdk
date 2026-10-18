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

#include <linux/version.h>
#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/if.h>
#include <linux/list.h>
#include <linux/completion.h>
#include <linux/skbuff.h>

#include "netfn_pkt_steer_priv.h"

/*
 * netfn_pkt_steer_send_ipi()
 *	trigger IPI only if not in-progress.
 */
static inline void netfn_pkt_steer_send_ipi(int cpu, struct netfn_pkt_steer_pcpu *pcpu)
{
	int error;

	/*
	 * Send IPI to remote CPU only if,
	 *  1. NAPI is not scheduled (This should be true for only initial packets).
	 *  2. Already IPI has been sent by previous packet on current CPU.
	 *  3. IPI is in-progress by other CPU for same remote CPU.
	 *
	 * IPI send can only fail if remote CPU is not online or invalid.
	 */
	if (unlikely(!atomic_read(&pcpu->ipi_masked)) && atomic_read(&pcpu->queued)) {
		if (atomic_cmpxchg(&pcpu->ipi_queued, 0, 1) == 0) {
			error = smp_call_function_single_async(cpu, &pcpu->csd);
			if (error) {
				WARN_ON_ONCE(error);

				/*
				 * Clear the ipi_queued in case of error.
				 */
				atomic_set(&pcpu->ipi_queued, 0);
			}
		}
	}
}

/*
 * netfn_pkt_steer_add_stats()
 * 	Add stats from src to dst
 */
static inline void netfn_pkt_steer_add_stats(struct netfn_pkt_steer_stats *src, struct netfn_pkt_steer_stats *dst)
{
	uint64_t *src_p = (uint64_t *)src;
	uint64_t *dst_p = (uint64_t *)dst;
	int i;

	for(i = 0; i < sizeof(*dst) / sizeof(uint64_t); i++) {
		dst_p[i] += src_p[i];
	}
}

/*
 * netfn_pkt_steer_flush()
 *	Free SKB array of given size.
 */
static void netfn_pkt_steer_flush(struct netfn_pkt_steer_pcpu *pcpu)
{
	int i, cpu;

	for_each_online_cpu(cpu) {
		struct netfn_pkt_steer_fifo *fifo = &pcpu->fifo[cpu];

		for (i = 0; i < NETFN_PKT_STEER_QUEUE_DEPTH; i++) {
			struct sk_buff *skb = xchg(&fifo->skbs[i], NULL);
			if (skb)
				consume_skb(skb);
		}
	}
}

/*
 * netfn_pkt_steer_reap_fifo()
 * 	Process the pending skb for given FIFO
 */
static int netfn_pkt_steer_reap_fifo(struct netfn_pkt_steer_fifo *fifo, struct sk_buff_head *skb_head, int budget)
{
	int processed = 0, avail;
	struct sk_buff *skb;
	uint64_t prod;
	uint64_t cons;

	prod = atomic64_read(&fifo->prod);
	cons = atomic64_read(&fifo->cons);

	avail = NETFN_PKT_STEER_AVAIL_COUNT(prod, cons);
	if (!avail) {
		return 0;
	}

	avail = avail > budget ? budget : avail;
	processed = avail;

	while (avail--) {
		skb = xchg(&fifo->skbs[cons], NULL);
		cons = NETFN_PKT_STEER_INC(cons);
		if (likely(avail)) {
			struct sk_buff *nskb = fifo->skbs[cons];
			prefetch(nskb);
			prefetch((uint8_t *)nskb + 64);
			prefetch((uint8_t *)nskb + 128);
			prefetch((uint8_t *)nskb + 192);
		}

		__skb_queue_tail(skb_head, skb);
	}

	atomic64_set(&fifo->cons, cons);

	fifo->stats.fifo_dq += processed;
	return processed;
}

/*
 * netfn_pkt_steer_rcv_poll
 * 	NAPI poll function
 */
static int netfn_pkt_steer_rcv_poll(struct napi_struct *napi, int budget)
{
	struct netfn_pkt_steer_pcpu *pcpu = container_of(napi, struct netfn_pkt_steer_pcpu, napi);
	struct sk_buff_head skb_head;
	uint32_t processed = 0;
	int cpu;

	skb_queue_head_init(&skb_head);

	/*
	 * TODO: Implement Fair Reap budget.
	 */
	for_each_online_cpu(cpu) {
		struct netfn_pkt_steer_fifo *fifo = &pcpu->fifo[cpu];
		processed += netfn_pkt_steer_reap_fifo(fifo, &skb_head, budget - processed);
	}

	/*
	 * Callback to remote handler.
	 */
	pcpu->cb(pcpu->ps, &skb_head);
	WARN_ON(skb_queue_len(&skb_head));

	atomic_sub(processed, &pcpu->queued);
	if (processed < budget) {

		napi_complete(napi);
		atomic_set(&pcpu->ipi_masked, 0);

		/*
		 * Reschedule if packet got queued after last Reap.
		 * Otherwise those would have to wait for next packet interrupt.
		 */
		if (atomic_read(&pcpu->queued)) {
			atomic_set(&pcpu->ipi_masked, 1);
			napi_reschedule(napi);
			return processed;
		}
	}

	return processed;
}

/*
 * netfn_pkt_steer_rcv_ipi()
 * 	IPI interrupt function
 */
static void netfn_pkt_steer_rcv_ipi(void *info)
{
	struct netfn_pkt_steer_pcpu *pcpu = info;

	/*
	 * Mask the ipi and enable NAPI.
	 */
	atomic_set(&pcpu->ipi_masked, 1);
	napi_schedule(&pcpu->napi);

	/*
	 * Mark IPI as completed.
	 */
	atomic_set(&pcpu->ipi_queued, 0);
}

/*
 * netfn_pkt_steer_send_list()
 *	Send packet to remote CPU.
 */
int netfn_pkt_steer_send_list(struct netfn_pkt_steer *ps, struct sk_buff_head *head, int cpu, uint64_t *tx_bytes)
{
	struct netfn_pkt_steer_pcpu *ps_pcpu;
	struct netfn_pkt_steer_pcpu *pcpu;
	uint32_t qlen = skb_queue_len(head);
	struct netfn_pkt_steer_fifo *fifo;
	int cur_cpu = smp_processor_id();
	uint32_t processed = 0, avail;
	uint64_t prod, cons;

	rcu_read_lock_bh();
	ps_pcpu = rcu_dereference(ps->pcpu);
	if (unlikely(!ps_pcpu)) {
		rcu_read_unlock_bh();
		return 0;
	}

	pcpu = per_cpu_ptr(ps_pcpu, cpu);

	fifo = &pcpu->fifo[cur_cpu];
	prod = atomic64_read(&fifo->prod);
	cons = atomic64_read(&fifo->cons);
	*tx_bytes = 0;

	/*
	 * Fetch available length.
	 */
	avail = NETFN_PKT_STEER_AVAIL_COUNT(cons, prod + 1);
	if (!avail) {
		goto done;
	}

	avail = qlen > avail ? avail : qlen;
	processed = avail;

	while (avail--) {
		struct sk_buff *skb = __skb_dequeue(head);
		*tx_bytes += skb->len;

		xchg(&fifo->skbs[prod], skb);
		prod = NETFN_PKT_STEER_INC(prod);
	}

	atomic64_set(&fifo->prod, prod);
	atomic_add(processed, &pcpu->queued);
	fifo->stats.fifo_enq += processed;
done:
	fifo->stats.fifo_full += skb_queue_len(head);
	netfn_pkt_steer_send_ipi(cpu, pcpu);

	rcu_read_unlock_bh();
	return processed;
}
EXPORT_SYMBOL(netfn_pkt_steer_send_list);

/*
 * netfn_pkt_steer_send()
 *	Send single packet to remote CPU.
 */
int netfn_pkt_steer_send(struct netfn_pkt_steer *ps, struct sk_buff *skb, int cpu)
{
	struct netfn_pkt_steer_pcpu *ps_pcpu;
	struct netfn_pkt_steer_pcpu *pcpu;
	struct netfn_pkt_steer_fifo *fifo;
	int cur_cpu = smp_processor_id();
	int ret = 0;
	uint64_t prod, cons;

	rcu_read_lock_bh();
	ps_pcpu = rcu_dereference(ps->pcpu);
	if (unlikely(!ps_pcpu)) {
		rcu_read_unlock_bh();
		return 0;
	}

	pcpu = per_cpu_ptr(ps_pcpu, cpu);

	fifo = &pcpu->fifo[cur_cpu];
	prod = atomic64_read(&fifo->prod);
	cons = atomic64_read(&fifo->cons);

	/*
	 * Fetch available length.
	 */
	if (!NETFN_PKT_STEER_AVAIL_COUNT(cons, prod + 1)) {
		fifo->stats.fifo_full++;
		goto done;
	}

	xchg(&fifo->skbs[prod], skb);
	prod = NETFN_PKT_STEER_INC(prod);

	atomic64_set(&fifo->prod, prod);

	fifo->stats.fifo_enq++;
	atomic_inc(&pcpu->queued);
	ret = 1;

done:
	netfn_pkt_steer_send_ipi(cpu, pcpu);
	rcu_read_unlock_bh();
	return ret;
}
EXPORT_SYMBOL(netfn_pkt_steer_send);

/*
 * netfn_pkt_steer_get_stats()
 * 	Get statistics of Packet steer context.
 */
void netfn_pkt_steer_get_stats(struct netfn_pkt_steer *ps, struct netfn_pkt_steer_stats *stats)
{
	struct netfn_pkt_steer_pcpu *ps_pcpu;
	int cpu, fifo_idx;

	memset(stats, 0, sizeof(*stats));

	rcu_read_lock_bh();
	ps_pcpu = rcu_dereference(ps->pcpu);
	if (!ps_pcpu) {
		goto done;
	}

	for_each_online_cpu(cpu) {
		struct netfn_pkt_steer_pcpu *pcpu = per_cpu_ptr(ps_pcpu, cpu);

		for_each_online_cpu(fifo_idx) {
			struct netfn_pkt_steer_fifo *fifo = &pcpu->fifo[fifo_idx];
			netfn_pkt_steer_add_stats(&fifo->stats, stats);
		}
	}

done:
	rcu_read_unlock_bh();
}
EXPORT_SYMBOL(netfn_pkt_steer_get_stats);


/*
 * netfn_pkt_steer_enable()
 *     Free packet steering object
 */
bool netfn_pkt_steer_enable(struct netfn_pkt_steer *ps)
{
	struct netfn_pkt_steer_info *info = &ps->info;
	struct netfn_pkt_steer_pcpu *ps_pcpu;
	int cpu;

	/*
	 * Allocated steer context for each CPU.
	 */
	ps_pcpu = alloc_percpu_gfp(struct netfn_pkt_steer_pcpu, GFP_KERNEL | __GFP_ZERO);
	if (!ps_pcpu) {
		pr_err("%p: failed to allocate context for each CPU\n", ps);
		return false;
	}

	for_each_online_cpu(cpu) {
		struct netfn_pkt_steer_pcpu *pcpu = per_cpu_ptr(ps_pcpu, cpu);

		pcpu->ps = ps;
		pcpu->cb = info->cb;
#if (LINUX_VERSION_CODE < KERNEL_VERSION(6, 6, 0))
		pcpu->csd.flags = 0;
#endif
		pcpu->csd.info = pcpu;
		pcpu->csd.func = netfn_pkt_steer_rcv_ipi;
		atomic_set(&pcpu->ipi_queued, 0);
		atomic_set(&pcpu->ipi_masked, 0);
		atomic_set(&pcpu->queued, 0);

#if (LINUX_VERSION_CODE < KERNEL_VERSION(6, 6, 0))
		netif_napi_add(info->dev, &pcpu->napi, netfn_pkt_steer_rcv_poll, info->budget);
#else
		netif_napi_add_weight(info->dev, &pcpu->napi, netfn_pkt_steer_rcv_poll, info->budget);
#endif
		napi_enable(&pcpu->napi);

		pr_info("%s:Enabled pkt_steer for cpu(%u)\n", info->dev->name, cpu);
	}

	rcu_assign_pointer(ps->pcpu, ps_pcpu);
	return true;
}
EXPORT_SYMBOL(netfn_pkt_steer_enable);

/*
 * netfn_pkt_steer_disable()
 *     Free packet steering object
 */
void netfn_pkt_steer_disable(struct netfn_pkt_steer *ps)
{
	struct netfn_pkt_steer_pcpu *ps_pcpu;
	int cpu;

	/*
	 * NOTE: Caller should not call multiple Packet Steer API(s) at same time.
	 */
	ps_pcpu = rcu_dereference_protected(ps->pcpu, 1);
	RCU_INIT_POINTER(ps->pcpu, NULL);
	synchronize_rcu();

	for_each_online_cpu(cpu) {
		struct netfn_pkt_steer_pcpu *pcpu = per_cpu_ptr(ps_pcpu, cpu);
		napi_disable(&pcpu->napi);
		netif_napi_del(&pcpu->napi);
		netfn_pkt_steer_flush(pcpu);
	}

	free_percpu(ps_pcpu);
}
EXPORT_SYMBOL(netfn_pkt_steer_disable);

/*
 * netfn_pkt_steer_init()
 *	Allocate Packet steering object.
 */
void netfn_pkt_steer_init(struct netfn_pkt_steer *ps, struct netfn_pkt_steer_info *info)
{
	memset(ps, 0, sizeof(*ps));
	ps->info = *info;
}
EXPORT_SYMBOL(netfn_pkt_steer_init);

/*
 * netfn_pkt_steer_free()
 *	Free packet steering object
 */
void netfn_pkt_steer_deinit(struct netfn_pkt_steer *ps)
{
	memset(ps, 0, sizeof(*ps));
}
EXPORT_SYMBOL(netfn_pkt_steer_deinit);

/*
 * netfn_pkt_steer_init_module()
 *	Module initialization
 */
int __init netfn_pkt_steer_init_module(void)
{
	pr_info("NETFN packet steer module loaded with Queue size(%u) %s\n", NETFN_PKT_STEER_QUEUE_DEPTH, NSS_NETFN_BUILD_ID);
	return 0;
}

/*
 * netfn_pkt_steer_exit_module()
 *	Module exit cleanup
 */
void __exit netfn_pkt_steer_exit_module(void)
{
	pr_info("NETFN packet steer module unloaded %s\n", NSS_NETFN_BUILD_ID);
}

module_init(netfn_pkt_steer_init_module);
module_exit(netfn_pkt_steer_exit_module);

MODULE_AUTHOR("Qualcomm Technologies");
MODULE_DESCRIPTION("NETFN Packet Steer");
MODULE_LICENSE("Dual BSD/GPL");
