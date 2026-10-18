/*
 * Copyright (c) 2022-2023, Qualcomm Innovation Center, Inc. All rights reserved.
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

#ifndef __EIP_FLOW_H
#define __EIP_FLOW_H

#define EIP_FLOW_MAX (1U << (EIP_HW_FLUE_CONFIG_TABLE_SZ + 5))
#define EIP_FLOW_MASK (EIP_FLOW_MAX - 1)
#define EIP_FLOW_HASH_IDX(hash) ((hash.words[0] >> 6) & EIP_FLOW_MASK)
#define EIP_FLOW_HASH_DATA_SZ 13U /* Iterations for hash function */
#define EIP_FLOW_MAX_COLLISION 64U
#define EIP_HASH_EQUAL(H1,H2) ((H1.words[0] == H2.words[0]) && (H1.words[1] == H2.words[1]) && (H1.words[2] == H2.words[2]) && (H1.words[3] == H2.words[3]))
#define EIP_FLOW_TR_DISABLE 0x0
#define EIP_FLOW_VALID_BIT 0x1U

/*
 * eip_flow_hash_t
 * 	Structure to store 128bit hash
 */
typedef struct {
	uint32_t words[4];
} eip_flow_hash_t;

/*
 * eip_flow_tuple
 *      Filled by caller; Allocate on stack.
 */
struct eip_flow_tuple {
	__be32 src_ip[4];
	__be32 dst_ip[4];
	__be32 spi;           /* For non-IPsec flow this must be zero */
	__be16 src_port;
	__be16 dst_port;
	__be16 epoch;         /* For non-DTLS flow this must be zero */
	uint8_t ip_ver;
	uint8_t ip_proto;
};

/*
 * eip_flow_hw
 *      Hw flow
 */
struct eip_flow_hw {
	eip_flow_hash_t hashid_1;	/* For storing the hash value */
	eip_flow_hash_t reserved_0;
	eip_flow_hash_t reserved_1;
	uint32_t tr_addr_type_1;	/* Record offset for transform record */
	uint32_t reserved_addr_0;
	uint32_t reserved_addr_1;
	uint32_t next_flow_offset;	/* Bucket offset in the presence of collision */
}__attribute((__packed__));

/*
 * eip_flow
 *      SW flow object
 */
struct eip_flow {
	struct hlist_node node;         /* Hlist node to traverse the collision list */
	struct eip_flow_hw *hflow;      /* Pointer to HW flow */
	dma_addr_t hflow_paddr;         /* Physical address of hardware flow */
	eip_flow_hash_t hash;           /* Hash value calculated from flow tuple */
	struct eip_flow_tuple tuple;    /* Flow tuple */
	bool sentinel;                  /* Sentinel to check if a node is the head node */
};

/*
 * eip_flow_tbl
 * 	Flow table
 */
struct eip_flow_tbl {
	struct hlist_head sw_head[EIP_FLOW_MAX];        /* Pointer to SW flow object in the flow table */
	struct eip_flow_hw *hw_head;                    /* Pointer to HW flow in the flow table */
	dma_addr_t hw_head_paddr;                       /* Physical address of the flow table */
	spinlock_t lock;                                /* Spinlock */
	struct eip_flow_tbl_stats {
		uint64_t alloc;                         /* Counter for number of flows allocated */
		uint64_t free;                          /* Counter for number of flows freed */
		uint64_t collision;                     /* Counter for total number of collisions */
		uint64_t active_collision;              /* Counter for total collisions that are active */
	} stats;
} ;

struct eip_flow *eip_flow_add(struct eip_flow_tuple *flow_tuple, struct eip_tr *tr);
void eip_flow_del(struct eip_flow_tuple *flow_tuple, struct eip_tr *tr);
bool eip_flow_table_init(struct platform_device *pdev);
void eip_flow_table_deinit(struct platform_device *pdev);

#endif /* __EIP_FLOW_H */
