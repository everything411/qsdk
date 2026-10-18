/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: ISC
 */

#ifndef NFT_H
#define NFT_H

struct hostapd_scs_req_desc_data;

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>

struct hostapd_data;

/* NFT support requires Linux kernel 5.16+ for NF_NETDEV_EGRESS */
#define NFT_MIN_KERNEL_VERSION KERNEL_VERSION(5, 16, 0)

#define NFT_ETH_HDR_DEST_MAC_OFFSET 0
#define BATCH_BUF_SIZE		8192
#define NFNL_SUBSYS_RES_ID	10  /* Resource ID for nfnetlink subsystem */
#define NFT_RECEIVE_BUF_SIZE		4096

/* UDP encapsulated IPsec (UDP-encapsulated ESP) port numbers */
#define NFT_UDP_ENCAP_IPSEC_PORT_STANDARD	4500
#define NFT_UDP_ENCAP_IPSEC_PORT_ALT		5200

/**
 * struct nft_global - Global NFT netlink socket management
 */
struct nft_global {
	struct mnl_socket *nl;
	u32 seq;
	unsigned int portid;
};


/**
 * nft_init - Initialize NFT netlink socket
 * Returns: 0 on success, -1 on failure
 *
 * This function initializes the global NFT netlink socket that will be
 * used for all nftables operations.
 */
int nft_init(void);

/**
 * nft_deinit - Deinitialize NFT netlink socket
 *
 * This function closes the global NFT netlink socket and frees associated
 * resources.
 */
void nft_deinit(void);

int hostapd_config_nft_table(char *table, bool add);

int hostapd_config_nft_chain(struct hostapd_data *hapd,
			     char *table, char *chain,
			     bool add);
int hostapd_config_nft_chain_for_dev(char *table, char *chain,
				     char *dev_name, bool add);

/* Rule parameter flags */
#define NFT_RULE_PARAM_SADDR	(1 << 0)
#define NFT_RULE_PARAM_DADDR	(1 << 1)
#define NFT_RULE_PARAM_SPORT	(1 << 2)
#define NFT_RULE_PARAM_DPORT	(1 << 3)
#define NFT_RULE_PARAM_PROTO	(1 << 4)
#define NFT_RULE_PARAM_MARK	(1 << 5)
#define NFT_RULE_PARAM_HANDLE	(1 << 6)
#define NFT_RULE_PARAM_DMAC	(1 << 7)
#define NFT_RULE_PARAM_ESP	(1 << 8)
#define NFT_RULE_PARAM_SPI	(1 << 9)
#define NFT_RULE_PARAM_DSCP	(1 << 10)

struct hostapd_nft_rule_params {
	u8 ip_family;
	u8 nf_family;
	char table[32];
	char chain[32];
	u32 valid_flags;
	u8 saddr6[16];
	u8 daddr6[16];
	u32 saddr4;
	u32 daddr4;
	u16 sport;
	u16 dport;
	u8 proto;
	u32 mark;
	u64 rule_pos;
	u8 dmac[ETH_ALEN];
	u32 esp_spi;
	u8 dscp;
	u8 weight;
	u64 handle;
	u8 flow_idx;
	struct hostapd_scs_req_desc_data *scs_desc;
	int tclas_ele_idx;
	u32 set_id;
};

int hostapd_config_nft_rule(struct hostapd_nft_rule_params *rparams, bool add);
#endif /* NFT_H */
