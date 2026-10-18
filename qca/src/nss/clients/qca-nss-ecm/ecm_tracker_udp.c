/*
 **************************************************************************
 * Copyright (c) 2014-2015, 2020, The Linux Foundation. All rights reserved.
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: ISC
 **************************************************************************
 */

#include <linux/version.h>
#include <linux/types.h>
#include <linux/ip.h>
#include <linux/tcp.h>
#include <linux/module.h>
#include <linux/skbuff.h>
#include <linux/icmp.h>
#include <linux/sysctl.h>
#include <linux/kthread.h>
#include <linux/device.h>
#include <linux/fs.h>
#include <linux/pkt_sched.h>
#include <linux/string.h>
#include <net/route.h>
#include <net/ip.h>
#include <net/tcp.h>
#include <asm/uaccess.h>	/* for put_user */
#include <net/ipv6.h>
#include <linux/inet.h>
#include <linux/in.h>
#include <linux/udp.h>
#include <linux/tcp.h>

#include <linux/netfilter_ipv4.h>
#include <linux/netfilter_bridge.h>
#include <net/netfilter/nf_conntrack.h>
#include <net/netfilter/nf_conntrack_helper.h>
#include <net/netfilter/nf_conntrack_l4proto.h>
#include <net/netfilter/nf_conntrack_core.h>
#include <net/netfilter/ipv4/nf_conntrack_ipv4.h>
#include <net/netfilter/ipv4/nf_defrag_ipv4.h>

/*
 * Debug output levels
 * 0 = OFF
 * 1 = ASSERTS / ERRORS
 * 2 = 1 + WARN
 * 3 = 2 + INFO
 * 4 = 3 + TRACE
 */
#define DEBUG_LEVEL ECM_TRACKER_UDP_DEBUG_LEVEL

#include "ecm_types.h"
#include "ecm_db_types.h"
#include "ecm_state.h"
#include "ecm_tracker.h"
#include "ecm_tracker_udp.h"

/*
 * Magic numbers
 */
#define ECM_TRACKER_UDP_INSTANCE_MAGIC 0x7765
#define ECM_TRACKER_UDP_SKB_CB_MAGIC 0xAAAB

/*
 * Sysctl table header
 */
static struct ctl_table_header *ecm_tracker_udp_ctl_tbl_hdr;

/*
 * Useful constants
 */
#define ECM_TRACKER_UDP_HEADER_SIZE 8			/* UDP header is always 8 bytes RFC 768 Page 1 */
#define ECM_TRACKER_UDP_RTP_HEADER_SIZE 12		/* RTP static header is 12 */
#define ECM_TRACKER_UDP_RTP_VERSION 2			/* RTP version */
#define ECM_TRACKER_UDP_RTP_CSRC_COUNT_MAX 4		/* RTP Max CSRC count */

#define ECM_TRACKER_UDP_RTP_VERSION_BITS_SHIFT 6	/* RTP version shift */
#define ECM_TRACKER_UDP_RTP_CSRC_COUNT_MASK 0x0F	/* CCRC count mask */
#define ECM_TRACKER_UDP_RTP_PAYLOAD_TYPE_MASK 0x7F	/* Payload type mask */

#define ECM_FRONT_END_UDP_RTP_PAYLOAD_TYPES_BUFFER_SIZE 64	/* Size of buffer to read payload type list */
#define ECM_TRACKER_UDP_RTP_PAYLOAD_TYPES_MIN 0			/* MIN RTP payload types */
#define ECM_TRACKER_UDP_RTP_PAYLOAD_TYPES_MAX 128		/* MAX RTP payload types */

/*
 * Supported payload types list
 */
static bool ecm_tracker_udp_rtp_payload_types[ECM_TRACKER_UDP_RTP_PAYLOAD_TYPES_MAX];

uint32_t ecm_tracker_udp_clf_enabled;	/* UDP classification enable flag */

#ifdef ECM_TRACKER_DPI_SUPPORT_ENABLE
/*
 * struct ecm_tracker_udp_skb_cb_format
 *	Map of the cb[] array within our cached buffers - we use that area for our tracking
 */
struct ecm_tracker_udp_skb_cb_format {
	uint32_t data_offset;			/* Offset in skb data to the actual datagram data - i.e. omitting headers */
	uint32_t data_size;			/* Size of data */
#if (DEBUG_LEVEL > 0)
	uint16_t magic;
#endif
};
#endif

/*
 * struct ecm_tracker_udp_internal_instance
 */
struct ecm_tracker_udp_internal_instance {
	struct ecm_tracker_udp_instance udp_base;			/* MUST BE FIRST FIELD */

#ifdef ECM_TRACKER_DPI_SUPPORT_ENABLE
	/*
	 * skb-next and skb->prev pointers are leveraged for these lists
	 */
	struct sk_buff *src_recvd_order;	/* sk buff list as sent by src */
	struct sk_buff *src_recvd_order_last;	/* Last skb send - for fast appending of new buffers */
	int32_t src_count;			/* Count of datagrams in list */
	int32_t src_bytes_total;		/* Total bytes in all received datagrams */

	struct sk_buff *dest_recvd_order;	/* sk buff list as sent by dest */
	struct sk_buff *dest_recvd_order_last;	/* Last skb send - for fast appending of new buffers */
	int32_t dest_count;			/* Count of datagrams in list */
	int32_t dest_bytes_total;		/* Total bytes in all received datagrams */

	int32_t data_limit;			/* Limit for tracked data */
#endif

	ecm_tracker_sender_state_t sender_state[ECM_TRACKER_SENDER_MAX];
						/* State of each sender */
	ecm_db_timer_group_t timer_group;	/* Recommended timer group for connection that is using this tracker */

	spinlock_t lock;			/* lock */

	int refs;				/* Integer to trap we never go negative */
#if (DEBUG_LEVEL > 0)
	uint16_t magic;
#endif
};

int ecm_tracker_udp_count = 0;		/* Counts the number of UDP data trackers right now */
static DEFINE_SPINLOCK(ecm_tracker_udp_lock);		/* Global lock for the tracker globals */

/*
 * ecm_trracker_udp_connection_state_matrix[][]
 *	Matrix to convert from/to states to connection state
 */
static ecm_tracker_connection_state_t ecm_tracker_udp_connection_state_matrix[ECM_TRACKER_SENDER_STATE_MAX][ECM_TRACKER_SENDER_STATE_MAX] =
{	/* 			Unknown						Establishing					Established					Closing					Closed					Fault */
	/* Unknown */		{ECM_TRACKER_CONNECTION_STATE_ESTABLISHING,	ECM_TRACKER_CONNECTION_STATE_ESTABLISHING,	ECM_TRACKER_CONNECTION_STATE_ESTABLISHED,	ECM_TRACKER_CONNECTION_STATE_FAULT,	ECM_TRACKER_CONNECTION_STATE_FAULT,	ECM_TRACKER_CONNECTION_STATE_FAULT},
	/* Establishing */	{ECM_TRACKER_CONNECTION_STATE_ESTABLISHING,	ECM_TRACKER_CONNECTION_STATE_ESTABLISHING,	ECM_TRACKER_CONNECTION_STATE_ESTABLISHED,	ECM_TRACKER_CONNECTION_STATE_FAULT,	ECM_TRACKER_CONNECTION_STATE_FAULT,	ECM_TRACKER_CONNECTION_STATE_FAULT},
	/* Established */	{ECM_TRACKER_CONNECTION_STATE_ESTABLISHED,	ECM_TRACKER_CONNECTION_STATE_ESTABLISHED,	ECM_TRACKER_CONNECTION_STATE_ESTABLISHED,	ECM_TRACKER_CONNECTION_STATE_CLOSING,	ECM_TRACKER_CONNECTION_STATE_CLOSING,	ECM_TRACKER_CONNECTION_STATE_FAULT},
	/* Closing */		{ECM_TRACKER_CONNECTION_STATE_FAULT,		ECM_TRACKER_CONNECTION_STATE_FAULT,		ECM_TRACKER_CONNECTION_STATE_CLOSING,		ECM_TRACKER_CONNECTION_STATE_CLOSING,	ECM_TRACKER_CONNECTION_STATE_CLOSING,	ECM_TRACKER_CONNECTION_STATE_FAULT},
	/* Closed */		{ECM_TRACKER_CONNECTION_STATE_FAULT,		ECM_TRACKER_CONNECTION_STATE_FAULT,		ECM_TRACKER_CONNECTION_STATE_CLOSING,		ECM_TRACKER_CONNECTION_STATE_CLOSING,	ECM_TRACKER_CONNECTION_STATE_CLOSED, 	ECM_TRACKER_CONNECTION_STATE_FAULT},
	/* Fault */		{ECM_TRACKER_CONNECTION_STATE_FAULT,		ECM_TRACKER_CONNECTION_STATE_FAULT,		ECM_TRACKER_CONNECTION_STATE_FAULT,		ECM_TRACKER_CONNECTION_STATE_FAULT,	ECM_TRACKER_CONNECTION_STATE_FAULT,	ECM_TRACKER_CONNECTION_STATE_FAULT},
};

/*
 * ecm_tracker_udp_check_header_and_read()
 *	Check for validly sized header and read the udp protocol header
 */
struct udphdr *ecm_tracker_udp_check_header_and_read(struct sk_buff *skb, struct ecm_tracker_ip_header *ip_hdr, struct udphdr *port_buffer)
{
	struct ecm_tracker_ip_protocol_header *header;

	/*
	 * Is there a UDP header?
	 */
	header = &ip_hdr->headers[ECM_TRACKER_IP_PROTOCOL_TYPE_UDP];
	if (header->header_size != ECM_TRACKER_UDP_HEADER_SIZE) {
		DEBUG_WARN("Skb: %px, UDP header size bad %u\n", skb, header->header_size);
		return NULL;
	}

	return skb_header_pointer(skb, header->offset, sizeof(*port_buffer), port_buffer);
}
EXPORT_SYMBOL(ecm_tracker_udp_check_header_and_read);

/*
 * ecm_tracker_is_payload_type_in_supported_list()
 *	Checks if given payload type is in the supported payload type list.
 */
static bool ecm_tracker_is_payload_type_in_supported_list(int payload_type)
{
	return (ecm_tracker_udp_rtp_payload_types[payload_type]);
}

/*
 * ecm_tracker_udp_check_is_rtp()
 *	Check if this is RTP packet
 *
 * 0                   1                   2                   3
 *    0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
 *   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 *   |V=2|P|X|  CC   |M|     PT      |       sequence number         |
 *   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 *   |                           timestamp                           |
 *   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 *   |           synchronization source (SSRC) identifier            |
 *   +=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+
 *   |            contributing source (CSRC) identifiers             |
 *   |                             ....                              |
 *   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 */
bool ecm_tracker_udp_check_is_rtp(struct sk_buff *skb, struct udphdr *udp_hdr)
{
	uint8_t *payload = (void *)udp_hdr + sizeof(*udp_hdr);
	int payload_size = ntohs(udp_hdr->len) - sizeof(*udp_hdr);
	int version, cc, pt;

	/*
	 * RTP detection is not done if UDP classification is not enabled
	 */
	if (!ecm_tracker_udp_clf_enabled) {
		DEBUG_TRACE("Skb: %px, UDP classification is not enabled\n", skb);
		return false;
	}

	if (!payload_size) {
		DEBUG_TRACE("Skb: %px, bad UDP payload size, udp_hdr len %d payload size %d\n",
				skb, ntohs(udp_hdr->len), payload_size);
		return false;
	}

	/*
	 * Version should be 2
	 */
	version = *payload >> ECM_TRACKER_UDP_RTP_VERSION_BITS_SHIFT;
	if (version != ECM_TRACKER_UDP_RTP_VERSION) {
		DEBUG_TRACE("Skb: %px, RTP version %d\n", skb, version);
		return false;
	}

	/*
	 * CSRC count should be <= 4
	 */
	cc = *payload & ECM_TRACKER_UDP_RTP_CSRC_COUNT_MASK;
	if (cc > ECM_TRACKER_UDP_RTP_CSRC_COUNT_MAX) {
		DEBUG_TRACE("Skb: %px, RTP CSRC count %d\n", skb, cc);
		return false;
	}

	/*
	 * Check for only supported static payload types
	 */
	payload++;
	pt = *payload & ECM_TRACKER_UDP_RTP_PAYLOAD_TYPE_MASK;
	if (!ecm_tracker_is_payload_type_in_supported_list(pt)) {
		DEBUG_TRACE("Skb: %px, RTP payload type %d\n", skb, pt);
		return false;
	}

	DEBUG_TRACE("Skb: %px, RTP version %d CSRC count %d payload type %d\n", skb, version, cc, pt);
	return true;
}

#ifdef ECM_TRACKER_DPI_SUPPORT_ENABLE
/*
 * ecm_tracker_udp_datagram_discard()
 *	Discard n number of datagrams at the head of the datagram list that were sent to the target
 */
static void ecm_tracker_udp_datagram_discard(struct ecm_tracker_udp_internal_instance *utii, ecm_tracker_sender_type_t sender, int32_t n)
{
	DEBUG_CHECK_MAGIC(utii, ECM_TRACKER_UDP_INSTANCE_MAGIC, "%px: magic failed", utii);

	DEBUG_TRACE("%px: discard %u datagrams for %d\n", utii, n, sender);

	/*
	 * Which list?
	 */
	if (sender == ECM_TRACKER_SENDER_TYPE_SRC) {
		/*
		 * iterate n times and discard the buffers
		 */
		while (n) {
			struct sk_buff *skb;

			spin_lock_bh(&utii->lock);

			skb = utii->src_recvd_order;
			DEBUG_ASSERT(skb, "%px: bad list\n", utii);

			utii->src_recvd_order = skb->next;
			if (!utii->src_recvd_order) {
				DEBUG_ASSERT(utii->src_recvd_order_last == skb, "%px: bad list\n", utii);
				utii->src_recvd_order_last = NULL;
			} else {
				utii->src_recvd_order->prev = NULL;
			}

			utii->src_count--;
			DEBUG_ASSERT(utii->src_count >= 0, "%px: bad total\n", utii);
			utii->src_bytes_total -= skb->truesize;
			ecm_tracker_data_total_decrease(skb->len, skb->truesize);
			DEBUG_ASSERT(utii->src_bytes_total >= 0, "%px: bad bytes total\n", utii);

			spin_unlock_bh(&utii->lock);
			kfree_skb(skb);

			n--;
		}
		return;
	}

	/*
	 * iterate n times and discard the buffers
	 */
	while (n) {
		struct sk_buff *skb;

		spin_lock_bh(&utii->lock);

		skb = utii->dest_recvd_order;
		DEBUG_ASSERT(skb, "%px: bad list\n", utii);

		utii->dest_recvd_order = skb->next;
		if (!utii->dest_recvd_order) {
			DEBUG_ASSERT(utii->dest_recvd_order_last == skb, "%px: bad list\n", utii);
			utii->dest_recvd_order_last = NULL;
		} else {
			utii->dest_recvd_order->prev = NULL;
		}

		utii->dest_count--;
		DEBUG_ASSERT(utii->dest_count >= 0, "%px: bad total\n", utii);
		utii->dest_bytes_total -= skb->truesize;
		ecm_tracker_data_total_decrease(skb->len, skb->truesize);
		DEBUG_ASSERT(utii->dest_bytes_total >= 0, "%px: bad bytes total\n", utii);

		spin_unlock_bh(&utii->lock);
		kfree_skb(skb);

		n--;
	}
}

/*
 * ecm_tracker_udp_discard_all()
 *	Discard all tracked data
 */
static void ecm_tracker_udp_discard_all(struct ecm_tracker_udp_internal_instance *utii)
{
	DEBUG_CHECK_MAGIC(utii, ECM_TRACKER_UDP_INSTANCE_MAGIC, "%px: magic failed", utii);

	/*
	 * Destroy all datagrams
	 */
	DEBUG_TRACE("%px: destroy all\n", utii);
	ecm_tracker_udp_datagram_discard(utii, ECM_TRACKER_SENDER_TYPE_SRC, utii->src_count);
	ecm_tracker_udp_datagram_discard(utii, ECM_TRACKER_SENDER_TYPE_DEST, utii->dest_count);
}

/*
 * ecm_tracker_udp_discard_all_callback()
 *	Discard all tracked data
 */
static void ecm_tracker_udp_discard_all_callback(struct ecm_tracker_instance *ti)
{
	struct ecm_tracker_udp_internal_instance *utii = (struct ecm_tracker_udp_internal_instance *)ti;
	ecm_tracker_udp_discard_all(utii);
}
#endif

/*
 * ecm_tracker_udp_ref()
 */
static void ecm_tracker_udp_ref(struct ecm_tracker_udp_internal_instance *utii)
{
	DEBUG_CHECK_MAGIC(utii, ECM_TRACKER_UDP_INSTANCE_MAGIC, "%px: magic failed", utii);

	spin_lock_bh(&utii->lock);

	utii->refs++;
	DEBUG_ASSERT(utii->refs > 0, "%px: ref wrap", utii);
	DEBUG_TRACE("%px: ref %d\n", utii, utii->refs);

	spin_unlock_bh(&utii->lock);
}

/*
 * ecm_tracker_udp_ref_callback()
 */
static void ecm_tracker_udp_ref_callback(struct ecm_tracker_instance *ti)
{
	struct ecm_tracker_udp_internal_instance *utii = (struct ecm_tracker_udp_internal_instance *)ti;
	ecm_tracker_udp_ref(utii);
}

/*
 * ecm_tracker_udp_deref()
 */
static int ecm_tracker_udp_deref(struct ecm_tracker_udp_internal_instance *utii)
{
	int refs;
	DEBUG_CHECK_MAGIC(utii, ECM_TRACKER_UDP_INSTANCE_MAGIC, "%px: magic failed", utii);

	spin_lock_bh(&utii->lock);
	utii->refs--;
	refs = utii->refs;
	DEBUG_ASSERT(utii->refs >= 0, "%px: ref wrap", utii);
	DEBUG_TRACE("%px: deref %d\n", utii, utii->refs);

	if (utii->refs > 0) {
		spin_unlock_bh(&utii->lock);
		return refs;
	}
	spin_unlock_bh(&utii->lock);

	DEBUG_TRACE("%px: final\n", utii);

#ifdef ECM_TRACKER_DPI_SUPPORT_ENABLE
	/*
	 * Destroy all datagrams
	 */
	ecm_tracker_udp_datagram_discard(utii, ECM_TRACKER_SENDER_TYPE_SRC, utii->src_count);
	ecm_tracker_udp_datagram_discard(utii, ECM_TRACKER_SENDER_TYPE_DEST, utii->dest_count);
#endif

	spin_lock_bh(&ecm_tracker_udp_lock);
	ecm_tracker_udp_count--;
	DEBUG_ASSERT(ecm_tracker_udp_count >= 0, "%px: tracker count wrap", utii);
	spin_unlock_bh(&ecm_tracker_udp_lock);

	DEBUG_INFO("%px: Udp tracker final\n", utii);
	DEBUG_CLEAR_MAGIC(utii);
	kfree(utii);

	return 0;
}

/*
 * _ecm_tracker_udp_deref_callback()
 */
static int ecm_tracker_udp_deref_callback(struct ecm_tracker_instance *ti)
{
	struct ecm_tracker_udp_internal_instance *utii = (struct ecm_tracker_udp_internal_instance *)ti;
	return ecm_tracker_udp_deref(utii);
}

#ifdef ECM_TRACKER_DPI_SUPPORT_ENABLE
/*
 * ecm_tracker_udp_datagram_count_get()
 *	Return number of available datagrams sent to the specified target
 */
static int32_t ecm_tracker_udp_datagram_count_get(struct ecm_tracker_udp_internal_instance *utii, ecm_tracker_sender_type_t sender)
{
	int32_t count;

	DEBUG_CHECK_MAGIC(utii, ECM_TRACKER_UDP_INSTANCE_MAGIC, "%px: magic failed", utii);

	/*
	 * Which list?
	 */
	spin_lock_bh(&utii->lock);
	if (sender == ECM_TRACKER_SENDER_TYPE_SRC) {
		count = utii->src_count;
	} else {
		count = utii->dest_count;
	}
	spin_unlock_bh(&utii->lock);

	DEBUG_TRACE("%px: datagram count get for %d is %d\n", utii, sender, count);

	return count;
}

/*
 * ecm_tracker_udp_datagram_count_get_callback()
 *	Return number of available datagrams sent to the specified target
 */
static int32_t ecm_tracker_udp_datagram_count_get_callback(struct ecm_tracker_instance *ti, ecm_tracker_sender_type_t sender)
{
	struct ecm_tracker_udp_internal_instance *utii = (struct ecm_tracker_udp_internal_instance *)ti;
	return ecm_tracker_udp_datagram_count_get(utii, sender);
}

/*
 * ecm_tracker_udp_datagram_discard_callback()
 *	Discard n number of datagrams at the head of the datagram list that were sent to the target
 */
static void ecm_tracker_udp_datagram_discard_callback(struct ecm_tracker_instance *ti, ecm_tracker_sender_type_t sender, int32_t n)
{
	struct ecm_tracker_udp_internal_instance *utii = (struct ecm_tracker_udp_internal_instance *)ti;

	ecm_tracker_udp_datagram_discard(utii, sender, n);
}

/*
 * ecm_tracker_udp_datagram_size_get()
 *	Return size in bytes of datagram at index i that was sent to the target
 */
static int32_t ecm_tracker_udp_datagram_size_get(struct ecm_tracker_udp_instance *uti, ecm_tracker_sender_type_t sender, int32_t i)
{
	struct ecm_tracker_udp_internal_instance *utii = (struct ecm_tracker_udp_internal_instance *)uti;
	int32_t size;
	struct sk_buff *skb;
	DEBUG_CHECK_MAGIC(utii, ECM_TRACKER_UDP_INSTANCE_MAGIC, "%px: magic failed", utii);

	/*
	 * Which list?
	 */
	spin_lock_bh(&utii->lock);
	if (sender == ECM_TRACKER_SENDER_TYPE_SRC) {
		skb = utii->src_recvd_order;
	} else {
		skb = utii->dest_recvd_order;
	}

	/*
	 * Iterate to the i'th datagram
	 */
	while (i) {
		DEBUG_ASSERT(skb, "%px: index bad\n", utii);
		skb = skb->next;
		i--;
	}
	DEBUG_ASSERT(skb, "%px: index bad\n", utii);

	size = skb->len;

	spin_unlock_bh(&utii->lock);
	return size;
}

/*
 * ecm_tracker_udp_datagram_size_get_callback()
 *	Return size in bytes of datagram at index i that was sent to the target
 */
static int32_t ecm_tracker_udp_datagram_size_get_callback(struct ecm_tracker_instance *ti, ecm_tracker_sender_type_t sender, int32_t i)
{
	struct ecm_tracker_udp_instance *uti = (struct ecm_tracker_udp_instance *)ti;

	return ecm_tracker_udp_datagram_size_get(uti, sender, i);
}

/*
 * ecm_tracker_udp_datagram_read()
 *	Read size bytes from datagram at index i into the buffer
 */
static int ecm_tracker_udp_datagram_read(struct ecm_tracker_udp_instance *uti, ecm_tracker_sender_type_t sender, int32_t i, int32_t offset, int32_t size, void *buffer)
{
	struct ecm_tracker_udp_internal_instance *utii = (struct ecm_tracker_udp_internal_instance *)uti;
	int res;
	struct sk_buff *skb;
	DEBUG_CHECK_MAGIC(utii, ECM_TRACKER_UDP_INSTANCE_MAGIC, "%px: magic failed", utii);
	DEBUG_TRACE("%px: datagram %d read at offset %d for %d bytes for %d\n", utii, i, offset, size, sender);

	/*
	 * Which list?
	 */
	spin_lock_bh(&utii->lock);
	if (sender == ECM_TRACKER_SENDER_TYPE_SRC) {
		skb = utii->src_recvd_order;
	} else {
		skb = utii->dest_recvd_order;
	}

	/*
	 * Iterate to the i'th datagram
	 */
	while (i) {
		DEBUG_ASSERT(skb, "%px: index bad\n", utii);
		skb = skb->next;
		i--;
	}
	DEBUG_ASSERT(skb, "%px: index bad\n", utii);

	/*
	 * Perform read
	 */
	res = skb_copy_bits(skb, offset, buffer, (unsigned int)size);

	spin_unlock_bh(&utii->lock);

	return res;
}

/*
 * ecm_tracker_udp_datagram_read_callback()
 *	Read size bytes from datagram at index i into the buffer
 */
static int ecm_tracker_udp_datagram_read_callback(struct ecm_tracker_instance *ti, ecm_tracker_sender_type_t sender, int32_t i, int32_t offset, int32_t size, void *buffer)
{
	struct ecm_tracker_udp_instance *uti = (struct ecm_tracker_udp_instance *)ti;

	return ecm_tracker_udp_datagram_read(uti, sender, i, offset, size, buffer);
}

/*
 * ecm_tracker_udp_data_total_get_callback()
 *	Return total tracked data
 */
static int32_t ecm_tracker_udp_data_total_get_callback(struct ecm_tracker_instance *ti)
{
	struct ecm_tracker_udp_internal_instance *utii = (struct ecm_tracker_udp_internal_instance *)ti;
	int32_t data_total;

	DEBUG_CHECK_MAGIC(utii, ECM_TRACKER_UDP_INSTANCE_MAGIC, "%px: magic failed", utii);

	spin_lock_bh(&utii->lock);
	data_total = utii->src_bytes_total + utii->dest_bytes_total;
	spin_unlock_bh(&utii->lock);

	return data_total;
}

/*
 * ecm_tracker_udp_data_limit_get_callback()
 *	Return tracked data limit
 */
static int32_t ecm_tracker_udp_data_limit_get_callback(struct ecm_tracker_instance *ti)
{
	struct ecm_tracker_udp_internal_instance *utii = (struct ecm_tracker_udp_internal_instance *)ti;
	int32_t data_limit;

	DEBUG_CHECK_MAGIC(utii, ECM_TRACKER_UDP_INSTANCE_MAGIC, "%px: magic failed", utii);

	spin_lock_bh(&utii->lock);
	data_limit = utii->data_limit;
	spin_unlock_bh(&utii->lock);

	return data_limit;
}

/*
 * ecm_tracker_udp_data_limit_set_callback()
 *	Set tracked data limit
 */
static void ecm_tracker_udp_data_limit_set_callback(struct ecm_tracker_instance *ti, int32_t data_limit)
{
	struct ecm_tracker_udp_internal_instance *utii = (struct ecm_tracker_udp_internal_instance *)ti;

	DEBUG_CHECK_MAGIC(utii, ECM_TRACKER_UDP_INSTANCE_MAGIC, "%px: magic failed", utii);

	spin_lock_bh(&utii->lock);
	utii->data_limit = data_limit;
	spin_unlock_bh(&utii->lock);
}

/*
 * ecm_tracker_udp_datagram_add()
 *	Append the datagram onto the tracker queue for the given target
 */
static bool ecm_tracker_udp_datagram_add(struct ecm_tracker_udp_internal_instance *utii, ecm_tracker_sender_type_t sender,
								struct ecm_tracker_ip_header *ip_hdr, struct ecm_tracker_ip_protocol_header *ecm_udp_header,
								struct udphdr *udp_header, struct sk_buff *skb)
{
	struct sk_buff *skbc;
	struct ecm_tracker_udp_skb_cb_format *skbc_cb;

	DEBUG_TRACE("%px: datagram %px add for %d, ip_hdr_len %u, total len: %u, offset: %u, size: %u\n",
			utii, skb, sender, ip_hdr->ip_header_length, ip_hdr->total_length,
			ecm_udp_header->offset, ecm_udp_header->size);

	/*
	 * Clone the packet
	 */
	skbc = skb_clone(skb, GFP_ATOMIC | __GFP_NOWARN);
	if (!skbc) {
		DEBUG_WARN("%px: Failed to clone packet %px\n", utii, skb);
		return false;
	}

	DEBUG_TRACE("%px: cloned %px to %px\n", utii, skb, skbc);

	/*
	 * Get the private cb area and initialise it.
	 * ALL DATAGRAMS HAVE TO HAVE ONE.
	 */
	skbc_cb = (struct ecm_tracker_udp_skb_cb_format *)skbc->cb;
	DEBUG_SET_MAGIC(skbc_cb, ECM_TRACKER_UDP_SKB_CB_MAGIC);
	skbc_cb->data_offset = ecm_udp_header->offset + ecm_udp_header->header_size;
	if (ip_hdr->total_length < (ecm_udp_header->offset + ecm_udp_header->size)) {
		DEBUG_WARN("%px: Invalid headers\n", utii);
		kfree_skb(skbc);
		return false;
	}
	skbc_cb->data_size = ecm_udp_header->size - ecm_udp_header->header_size;

	/*
	 * Are we within instance limit?
	 */
	spin_lock_bh(&utii->lock);
	DEBUG_ASSERT((utii->src_bytes_total + utii->dest_bytes_total + skbc->truesize) > 0, "%px: bad total\n", utii);
	if ((utii->src_bytes_total + utii->dest_bytes_total + skbc->truesize) > utii->data_limit) {
		DEBUG_TRACE("%px: over limit\n", utii);
		spin_unlock_bh(&utii->lock);
		kfree_skb(skbc);
		return false;
	}

	/*
	 * Within global limit?
	 */
	if (!ecm_tracker_data_total_increase(skbc->len, skbc->truesize)) {
		DEBUG_TRACE("%px: over global limit\n", utii);
		spin_unlock_bh(&utii->lock);
		kfree_skb(skbc);
		return false;
	}

	/*
	 * Which list to insert the datagram in to?
	 */
	if (sender == ECM_TRACKER_SENDER_TYPE_SRC) {
		skbc->next = NULL;
		skbc->prev = utii->src_recvd_order_last;
		utii->src_recvd_order_last = skbc;
		if (skbc->prev) {
			skbc->prev->next = skbc;
		} else {
			DEBUG_ASSERT(utii->src_recvd_order == NULL, "%px: bad list\n", utii);
			utii->src_recvd_order = skbc;
		}

		utii->src_count++;
		DEBUG_ASSERT(utii->src_count > 0, "%px: bad total\n", utii);
		utii->src_bytes_total += skbc->truesize;
		spin_unlock_bh(&utii->lock);
		return true;
	}

	skbc->next = NULL;
	skbc->prev = utii->dest_recvd_order_last;
	utii->dest_recvd_order_last = skbc;
	if (skbc->prev) {
		skbc->prev->next = skbc;
	} else {
		DEBUG_ASSERT(utii->dest_recvd_order == NULL, "%px: bad list\n", utii);
		utii->dest_recvd_order = skbc;
	}

	utii->dest_count++;
	DEBUG_ASSERT(utii->dest_count > 0, "%px: bad total\n", utii);
	utii->dest_bytes_total += skbc->truesize;
	spin_unlock_bh(&utii->lock);
	return true;
}

/*
 * _ecm_tracker_udp_datagram_add_callback()
 *	Append the datagram onto the tracker queue for the given target
 */
static bool ecm_tracker_udp_datagram_add_callback(struct ecm_tracker_instance *ti, ecm_tracker_sender_type_t sender, struct sk_buff *skb)
{
	struct ecm_tracker_udp_internal_instance *utii = (struct ecm_tracker_udp_internal_instance *)ti;
	struct ecm_tracker_ip_header ip_hdr;
	struct ecm_tracker_ip_protocol_header *ecm_udp_header;
	struct udphdr *udp_header;
	struct udphdr udp_header_buffer;

	DEBUG_CHECK_MAGIC(utii, ECM_TRACKER_UDP_INSTANCE_MAGIC, "%px: magic failed", utii);

	/*
	 * Obtain the IP header from the skb
	 */
	if (!ecm_tracker_ip_check_header_and_read(&ip_hdr, skb)) {
		DEBUG_WARN("%px: no ip_hdr for %px\n", utii, skb);
		return false;
	}

	/*
	 * Get UDP header
	 */
	udp_header = ecm_tracker_udp_check_header_and_read(skb, &ip_hdr, &udp_header_buffer);
	if (!udp_header) {
		DEBUG_WARN("%px: not/invalid udp %d\n", utii, ip_hdr.protocol);
		return false;
	}
	ecm_udp_header = &ip_hdr.headers[ECM_TRACKER_IP_PROTOCOL_TYPE_UDP];

	return ecm_tracker_udp_datagram_add(utii, sender, &ip_hdr, ecm_udp_header, udp_header, skb);
}

/*
 * ecm_tracker_udp_datagram_add_checked_callback()
 *	Add a pre-checked datagram
 */
static bool ecm_tracker_udp_datagram_add_checked_callback(struct ecm_tracker_udp_instance *uti, ecm_tracker_sender_type_t sender,
								struct ecm_tracker_ip_header *ip_hdr, struct ecm_tracker_ip_protocol_header *ecm_udp_header,
								struct udphdr *udp_header, struct sk_buff *skb)
{
	struct ecm_tracker_udp_internal_instance *utii = (struct ecm_tracker_udp_internal_instance *)uti;
	DEBUG_CHECK_MAGIC(utii, ECM_TRACKER_UDP_INSTANCE_MAGIC, "%px: magic failed", utii);
	return ecm_tracker_udp_datagram_add(utii, sender, ip_hdr, ecm_udp_header, udp_header, skb);
}

/*
 * ecm_tracker_udp_data_read_callback()
 *	Return size bytes of datagram data at index i that was sent to the target
 */
static int ecm_tracker_udp_data_read_callback(struct ecm_tracker_udp_instance *uti, ecm_tracker_sender_type_t sender, int32_t i,
								int32_t offset, int32_t size, void *buffer)

{
	struct ecm_tracker_udp_internal_instance *utii = (struct ecm_tracker_udp_internal_instance *)uti;
	int res;
	struct sk_buff *skb;
	struct ecm_tracker_udp_skb_cb_format *skb_cb;

	DEBUG_CHECK_MAGIC(utii, ECM_TRACKER_UDP_INSTANCE_MAGIC, "%px: magic failed", utii);
	DEBUG_TRACE("%px: datagram data %d read at offset %d for %d bytes for %d\n", utii, i, offset, size, sender);

	/*
	 * Which list?
	 */
	spin_lock_bh(&utii->lock);
	if (sender == ECM_TRACKER_SENDER_TYPE_SRC) {
		skb = utii->src_recvd_order;
	} else {
		skb = utii->dest_recvd_order;
	}

	/*
	 * Iterate to the i'th datagram
	 */
	while (i) {
		DEBUG_ASSERT(skb, "%px: index bad\n", utii);
		skb = skb->next;
		i--;
	}
	DEBUG_ASSERT(skb, "%px: index bad\n", utii);

	/*
	 * Perform read of data excluding headers
	 */
	skb_cb = (struct ecm_tracker_udp_skb_cb_format *)skb->cb;
	DEBUG_CHECK_MAGIC(skb_cb, ECM_TRACKER_UDP_SKB_CB_MAGIC, "%px: invalid cb magic %px\n", utii, skb);
	offset += skb_cb->data_offset;
	DEBUG_ASSERT(size <= skb_cb->data_size, "%px: size %d too large for skb %px at %u\n", utii, size, skb, skb_cb->data_size);
	res = skb_copy_bits(skb, offset, buffer, (unsigned int)size);

	spin_unlock_bh(&utii->lock);

	return res;
}

/*
 * ecm_tracker_udp_data_size_get_callback()
 *	Read size in bytes of data at index i into the buffer
 */
static int32_t ecm_tracker_udp_data_size_get_callback(struct ecm_tracker_udp_instance *uti, ecm_tracker_sender_type_t sender, int32_t i)
{
	struct ecm_tracker_udp_internal_instance *utii = (struct ecm_tracker_udp_internal_instance *)uti;
	int32_t size;
	struct sk_buff *skb;
	struct ecm_tracker_udp_skb_cb_format *skb_cb;

	DEBUG_CHECK_MAGIC(utii, ECM_TRACKER_UDP_INSTANCE_MAGIC, "%px: magic failed", utii);
	DEBUG_TRACE("%px: datagram %u data size get for %d\n", utii, i, sender);

	/*
	 * Which list?
	 */
	spin_lock_bh(&utii->lock);
	if (sender == ECM_TRACKER_SENDER_TYPE_SRC) {
		skb = utii->src_recvd_order;
	} else {
		skb = utii->dest_recvd_order;
	}

	/*
	 * Iterate to the i'th datagram
	 */
	while (i) {
		DEBUG_ASSERT(skb, "%px: index bad\n", utii);
		skb = skb->next;
		i--;
	}
	DEBUG_ASSERT(skb, "%px: index bad\n", utii);

	/*
	 * Perform read of data excluding headers
	 */
	skb_cb = (struct ecm_tracker_udp_skb_cb_format *)skb->cb;
	DEBUG_CHECK_MAGIC(skb_cb, ECM_TRACKER_UDP_SKB_CB_MAGIC, "%px: invalid cb magic %px\n", utii, skb);
	size = skb_cb->data_size;
	spin_unlock_bh(&utii->lock);

	return size;
}
#endif

/*
 * ecm_tracker_udp_state_update_callback()
 * 	Update connection state based on the knowledge we have and the skb given
 */
static void ecm_tracker_udp_state_update_callback(struct ecm_tracker_instance *ti, ecm_tracker_sender_type_t sender, struct ecm_tracker_ip_header *ip_hdr, struct sk_buff *skb)
{
	struct ecm_tracker_udp_internal_instance *utii = (struct ecm_tracker_udp_internal_instance *)ti;
	DEBUG_CHECK_MAGIC(utii, ECM_TRACKER_UDP_INSTANCE_MAGIC, "%px: magic failed", utii);

	/*
	 * As long as a sender has seen data then we consider the sender established
	 */
	spin_lock_bh(&utii->lock);
	utii->sender_state[sender] = ECM_TRACKER_SENDER_STATE_ESTABLISHED;
	spin_unlock_bh(&utii->lock);
}

/*
 * ecm_tracker_udp_state_get_callback()
 * 	Get state
 */
static void ecm_tracker_udp_state_get_callback(struct ecm_tracker_instance *ti, ecm_tracker_sender_state_t *src_state,
					ecm_tracker_sender_state_t *dest_state, ecm_tracker_connection_state_t *state, ecm_db_timer_group_t *tg)
{
	struct ecm_tracker_udp_internal_instance *utii = (struct ecm_tracker_udp_internal_instance *)ti;
	DEBUG_CHECK_MAGIC(utii, ECM_TRACKER_UDP_INSTANCE_MAGIC, "%px: magic failed", utii);
	spin_lock_bh(&utii->lock);
	*src_state = utii->sender_state[ECM_TRACKER_SENDER_TYPE_SRC];
	*dest_state = utii->sender_state[ECM_TRACKER_SENDER_TYPE_DEST];
	*tg = utii->timer_group;
	spin_unlock_bh(&utii->lock);
	*state = ecm_tracker_udp_connection_state_matrix[*src_state][*dest_state];
}

#ifdef ECM_STATE_OUTPUT_ENABLE
/*
 * ecm_tracker_udp_state_text_get_callback()
 *	Return state
 */
static int ecm_tracker_udp_state_text_get_callback(struct ecm_tracker_instance *ti, struct ecm_state_file_instance *sfi)
{
	int result;
	struct ecm_tracker_udp_internal_instance *utii = (struct ecm_tracker_udp_internal_instance *)ti;
#ifdef ECM_TRACKER_DPI_SUPPORT_ENABLE
	int32_t src_count;
	int32_t src_bytes_total;
	int32_t dest_count;
	int32_t dest_bytes_total;
	int32_t data_limit;
#endif
	ecm_db_timer_group_t timer_group;
	ecm_tracker_sender_state_t sender_state[ECM_TRACKER_SENDER_MAX];
	ecm_tracker_connection_state_t connection_state;
	DEBUG_CHECK_MAGIC(utii, ECM_TRACKER_UDP_INSTANCE_MAGIC, "%px: magic failed", utii);

	if ((result = ecm_state_prefix_add(sfi, "tracker_udp"))) {
		return result;
	}

	/*
	 * Capture state
	 */
	spin_lock_bh(&utii->lock);
#ifdef ECM_TRACKER_DPI_SUPPORT_ENABLE
	src_count = utii->src_count;
	src_bytes_total = utii->src_bytes_total;
	dest_count = utii->dest_count;
	dest_bytes_total = utii->dest_bytes_total;
	data_limit = utii->data_limit;
#endif
	sender_state[ECM_TRACKER_SENDER_TYPE_SRC] = utii->sender_state[ECM_TRACKER_SENDER_TYPE_SRC];
	sender_state[ECM_TRACKER_SENDER_TYPE_DEST] = utii->sender_state[ECM_TRACKER_SENDER_TYPE_DEST];
	timer_group = utii->timer_group;
	spin_unlock_bh(&utii->lock);
	connection_state = ecm_tracker_udp_connection_state_matrix[sender_state[ECM_TRACKER_SENDER_TYPE_SRC]][sender_state[ECM_TRACKER_SENDER_TYPE_DEST]];

#ifdef ECM_TRACKER_DPI_SUPPORT_ENABLE
	if ((result = ecm_state_write(sfi, "src_count", "%d", src_count))) {
		return result;
	}
	if ((result = ecm_state_write(sfi, "src_bytes_total", "%d", src_bytes_total))) {
		return result;
	}
	if ((result = ecm_state_write(sfi, "dest_count", "%d", dest_count))) {
		return result;
	}
	if ((result = ecm_state_write(sfi, "dest_bytes_total", "%d", dest_bytes_total))) {
		return result;
	}
	if ((result = ecm_state_write(sfi, "data_limit", "%d", data_limit))) {
		return result;
	}
#endif

	connection_state = ecm_tracker_udp_connection_state_matrix[sender_state[ECM_TRACKER_SENDER_TYPE_SRC]][sender_state[ECM_TRACKER_SENDER_TYPE_DEST]];
	if ((result = ecm_state_write(sfi, "timer_group", "%d", ECM_DB_TIMER_GROUPS_CONNECTION_GENERIC_TIMEOUT))) {
		return result;
	}
	if ((result = ecm_state_write(sfi, "src_sender_state", "%s", ecm_tracker_sender_state_to_string(sender_state[ECM_TRACKER_SENDER_TYPE_SRC])))) {
		return result;
	}
	if ((result = ecm_state_write(sfi, "dest_sender_state", "%s", ecm_tracker_sender_state_to_string(sender_state[ECM_TRACKER_SENDER_TYPE_DEST])))) {
		return result;
	}
	if ((result = ecm_state_write(sfi, "connection_state", "%s", ecm_tracker_connection_state_to_string(connection_state)))) {
		return result;
	}

 	return ecm_state_prefix_remove(sfi);
}
#endif

/*
 * ecm_tracker_payload_types_read()
 *	Reads the payload types from the payload types array and prints.
 */
static void ecm_tracker_payload_types_read(void *buffer, size_t *lenp, loff_t *ppos, bool *payload_types)
{
	char *read_buf;
	int i, len;
	size_t bytes = 0;

	/*
	 * (64 * 8) bytes for the buffer size is sufficient to write
	 * the array including the spaces and new line characters.
	 */
	read_buf = kzalloc(ECM_FRONT_END_UDP_RTP_PAYLOAD_TYPES_BUFFER_SIZE * 8 * sizeof(char), GFP_KERNEL);
	if (!read_buf) {
		DEBUG_ERROR("Failed to alloc buffer to print payload types array\n");
		return;
	}

	for (i = 0; i < ECM_TRACKER_UDP_RTP_PAYLOAD_TYPES_MAX; i++) {
		if(ecm_tracker_udp_rtp_payload_types[i]) {
			len = scnprintf(read_buf + bytes, 8, "%d ", i);
			if (!len) {
				DEBUG_ERROR("failed to read from buffer %d\n", i);
				kfree(read_buf);
				return;
			}
			bytes += len;
		}
	}

	/*
	 * Add new line character at the end.
	 */
	len = scnprintf(read_buf + bytes, 4, "\n");
	bytes += len;

	bytes = memory_read_from_buffer(buffer, *lenp, ppos, read_buf, bytes);
	*lenp = bytes;
	kfree(read_buf);
}

/*
 * ecm_tracker_rtp_payload_types_handler()
 *	Proc handler function for RTP payload types read/write operation.
 */
static int ecm_tracker_rtp_payload_types_handler(int write, void *buffer, size_t *lenp, loff_t *ppos, bool *payload_types)
{

	char *buf;
	char *pfree;
	char *token;
	int count, payload_type;
	long int val;

	if (!write) {
		ecm_tracker_payload_types_read(buffer, lenp, ppos, payload_types);
		return 0;
	}

	buf = kzalloc((ECM_FRONT_END_UDP_RTP_PAYLOAD_TYPES_BUFFER_SIZE * 8) * sizeof(char), GFP_KERNEL);
	if (!buf) {
		return -ENOMEM;
	}

	pfree = buf;
	count = *lenp;
	if (count > (ECM_FRONT_END_UDP_RTP_PAYLOAD_TYPES_BUFFER_SIZE * 8 * sizeof(char))) {
		count = ECM_FRONT_END_UDP_RTP_PAYLOAD_TYPES_BUFFER_SIZE * 8 * sizeof(char);
	}

	memcpy(buf, buffer, count);
	*lenp = count;
	*ppos += count;

	token = strsep(&buf, " ");
	if (strlen(token) != 3) {
		DEBUG_ERROR("cmd should be add or del\n");
		kfree(pfree);
		return -EINVAL;
	}

	if (!strncmp(token, "add", 3)) {
		while (buf) {
			token = strsep(&buf, " ");
			if (!token) {
				DEBUG_ERROR("token is empty\n");
				kfree(pfree);
				return -EINVAL;
			}

			if (kstrtol(token, 10, &val)) {
				DEBUG_ERROR("%s is not a number\n", token);
				kfree(pfree);
				return -EINVAL;
			}

			if (sscanf(token, "%d", &payload_type)) {
				if (payload_type < ECM_TRACKER_UDP_RTP_PAYLOAD_TYPES_MIN
					|| payload_type >= ECM_TRACKER_UDP_RTP_PAYLOAD_TYPES_MAX) {
					DEBUG_ERROR("payload type %d is not between (0-127)\n", payload_type);
					kfree(pfree);
					return -EINVAL;
				}

				if (ecm_tracker_is_payload_type_in_supported_list(payload_type)) {
					DEBUG_WARN("payload type: %d is already in the list\n", payload_type);
					continue;
				}

				ecm_tracker_udp_rtp_payload_types[payload_type] = true;
			}
		}
	} else if (!strncmp(token, "del", 3)) {
		while (buf) {
			token = strsep(&buf, " ");
			if (!token) {
				DEBUG_ERROR("token is empty\n");
				kfree(pfree);
				return -EINVAL;
			}

			if (kstrtol(token, 10, &val)) {
				DEBUG_ERROR("%s is not a number\n", token);
				kfree(pfree);
				return -EINVAL;
			}

			if (sscanf(token, "%d", &payload_type)) {
				if (payload_type < ECM_TRACKER_UDP_RTP_PAYLOAD_TYPES_MIN
					|| payload_type >= ECM_TRACKER_UDP_RTP_PAYLOAD_TYPES_MAX) {
					DEBUG_ERROR("payload type %d is not between (0-127)\n", payload_type);
					kfree(pfree);
					return -EINVAL;
				}

				if (!ecm_tracker_is_payload_type_in_supported_list(payload_type)) {
					DEBUG_WARN("payload type: %d is not in the list\n", payload_type);
					continue;
				}

				ecm_tracker_udp_rtp_payload_types[payload_type] = false;
			}
		}
	} else {
		DEBUG_ERROR("invalid command: %s\n", token);
	}

	kfree(pfree);
	return count;
}

/*
 * ecm_tracker_udp_rtp_payload_types_handler()
 *	Proc handler function for RTP payload types read/write operation.
 */
static int ecm_tracker_udp_rtp_payload_types_handler(ECM_CTL_TABLE_CONST struct ctl_table *ctl, int write, void *buffer, size_t *lenp, loff_t *ppos)
{
	/*
	 * Usage:
	 *	Add payload types to the list:
	 *	echo add 14 18 32 > /proc/sys/net/ecm/udp_rtp_payload_types
	 *
	 *	Delete payload types from the list:
	 *	echo del 14 > /proc/sys/net/ecm/udp_rtp_payload_types
	 *
	 *	Dump the list to the console:
	 *	cat /proc/sys/net/ecm/udp_rtp_payload_types
	 */
	return ecm_tracker_rtp_payload_types_handler(write, buffer, lenp, ppos, ecm_tracker_udp_rtp_payload_types);
}

/*
 * ecm_tracker_udp_clf_enabled_handler()
 *	Sysctl to enable/disable UDP classifier flag.
 */
static int ecm_tracker_udp_clf_enabled_handler(ECM_CTL_TABLE_CONST struct ctl_table *ctl, int write, void *buffer, size_t *lenp, loff_t *ppos)
{
	int ret;

	/*
	 * Write the variable with user input
	 */
	ret = proc_dointvec(ctl, write, buffer, lenp, ppos);
	if (ret || (!write)) {
		/*
		 * Return failure.
		 */
		return ret;
	}

	if ((ecm_tracker_udp_clf_enabled != 0) &&
		(ecm_tracker_udp_clf_enabled != 1)) {
		DEBUG_WARN("Invalid input. Valid values 0/1\n");
		return -EINVAL;
	}

	return ret;
}

static struct ctl_table ecm_tracker_udp_sysctl_tbl[] = {
	{
		.procname	= "udp_classification_enabled",
		.data		= &ecm_tracker_udp_clf_enabled,
		.maxlen		= sizeof(int),
		.mode		= 0644,
		.proc_handler	= &ecm_tracker_udp_clf_enabled_handler,
	},
	{
		.procname	= "udp_rtp_payload_types",
		.data		= &ecm_tracker_udp_rtp_payload_types,
		.maxlen		= sizeof(int) * ECM_FRONT_END_UDP_RTP_PAYLOAD_TYPES_BUFFER_SIZE,
		.mode		= 0644,
		.proc_handler	= &ecm_tracker_udp_rtp_payload_types_handler,
	},
};

/*
 * ecm_tracker_udp_init()
 *	Initialise the two host addresses that define the two directions we track data for
 */
void ecm_tracker_udp_init(struct ecm_tracker_udp_instance *uti, int32_t data_limit, int src_port, int dest_port)
{
	struct ecm_tracker_udp_internal_instance *utii = (struct ecm_tracker_udp_internal_instance *)uti;
	DEBUG_CHECK_MAGIC(utii, ECM_TRACKER_UDP_INSTANCE_MAGIC, "%px: magic failed", utii);
	DEBUG_TRACE("%px: init udp tracker\n", utii);

	spin_lock_bh(&utii->lock);
#ifdef ECM_TRACKER_DPI_SUPPORT_ENABLE
	utii->data_limit = data_limit;
#endif
	if ((src_port < 1024) || (dest_port < 1024)) {
		/*
		 * Because UDP connections can be reaped we assign well known ports to the WKP timer group.
		 * The WKP group is not reaped thus preserving connections involving known services.
		 * NOTE: Classifiers are still free to change the group as they see fit.
		 */
		utii->timer_group = ECM_DB_TIMER_GROUPS_CONNECTION_UDP_WKP_TIMEOUT;
		spin_unlock_bh(&utii->lock);
		return;
	}
	utii->timer_group = ECM_DB_TIMER_GROUPS_CONNECTION_UDP_GENERIC_TIMEOUT;
	spin_unlock_bh(&utii->lock);
}
EXPORT_SYMBOL(ecm_tracker_udp_init);

/*
 * ecm_tracker_udp_alloc()
 */
struct ecm_tracker_udp_instance *ecm_tracker_udp_alloc(void)
{
	struct ecm_tracker_udp_internal_instance *utii;

	utii = (struct ecm_tracker_udp_internal_instance *)kzalloc(sizeof(struct ecm_tracker_udp_internal_instance), GFP_ATOMIC | __GFP_NOWARN);
	if (!utii) {
		DEBUG_WARN("Failed to allocate udp tracker instance\n");
		return NULL;
	}

	utii->udp_base.base.ref = ecm_tracker_udp_ref_callback;
	utii->udp_base.base.deref = ecm_tracker_udp_deref_callback;
	utii->udp_base.base.state_update = ecm_tracker_udp_state_update_callback;
	utii->udp_base.base.state_get = ecm_tracker_udp_state_get_callback;
#ifdef ECM_TRACKER_DPI_SUPPORT_ENABLE
	utii->udp_base.base.datagram_count_get = ecm_tracker_udp_datagram_count_get_callback;
	utii->udp_base.base.datagram_discard = ecm_tracker_udp_datagram_discard_callback;
	utii->udp_base.base.datagram_read = ecm_tracker_udp_datagram_read_callback;
	utii->udp_base.base.datagram_size_get = ecm_tracker_udp_datagram_size_get_callback;
	utii->udp_base.base.datagram_add = ecm_tracker_udp_datagram_add_callback;
	utii->udp_base.base.discard_all = ecm_tracker_udp_discard_all_callback;
	utii->udp_base.base.data_total_get = ecm_tracker_udp_data_total_get_callback;
	utii->udp_base.base.data_limit_get = ecm_tracker_udp_data_limit_get_callback;
	utii->udp_base.base.data_limit_set = ecm_tracker_udp_data_limit_set_callback;

	utii->udp_base.data_read = ecm_tracker_udp_data_read_callback;
	utii->udp_base.data_size_get = ecm_tracker_udp_data_size_get_callback;
	utii->udp_base.datagram_add = ecm_tracker_udp_datagram_add_checked_callback;
#endif
#ifdef ECM_STATE_OUTPUT_ENABLE
	utii->udp_base.base.state_text_get = ecm_tracker_udp_state_text_get_callback;
#endif

	spin_lock_init(&utii->lock);

	utii->refs = 1;
	DEBUG_SET_MAGIC(utii, ECM_TRACKER_UDP_INSTANCE_MAGIC);

	spin_lock_bh(&ecm_tracker_udp_lock);
	ecm_tracker_udp_count++;
	DEBUG_ASSERT(ecm_tracker_udp_count > 0, "%px: udp tracker count wrap\n", utii);
	spin_unlock_bh(&ecm_tracker_udp_lock);

	DEBUG_TRACE("UDP tracker created %px\n", utii);
	return (struct ecm_tracker_udp_instance *)utii;
}
EXPORT_SYMBOL(ecm_tracker_udp_alloc);

/*
 * ecm_tracker_udp_sysctl_register()
 *	Function to register sysctl node during ecm init
 */
void ecm_tracker_udp_sysctl_register(void)
{
	ecm_tracker_udp_ctl_tbl_hdr = register_sysctl("net/ecm", ecm_tracker_udp_sysctl_tbl);
}

/*
 * ecm_tracker_udp_sysctl_unregister()
 *	Function to unregister sysctl node during ecm exit
 */
void ecm_tracker_udp_sysctl_unregister(void)
{
	if (ecm_tracker_udp_ctl_tbl_hdr) {
		unregister_sysctl_table(ecm_tracker_udp_ctl_tbl_hdr);
	}
}
