/*
 * Copyright (c) 2022, Qualcomm Innovation Center, Inc. All rights reserved.
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

#ifndef _DP_SAWF_H__
#define _DP_SAWF_H__

#include <linux/in6.h>

#define QCA_SAWF_SVID_VALID 0x1
#define QCA_SAWF_DSCP_VALID 0x2
#define QCA_SAWF_PCP_VALID  0x4
#define QCA_SAWF_AC_VALID   0x8
#define QCA_SAWF_SVC_ID_INVALID 0xFF
#define FLOW_START 1
#define FLOW_STOP  2
#define FLOW_DEPRIORITIZE  3
#define QCA_SAWF_MCAST_IF_MAX 16
#define MAC_ADDR_SIZE 6
#define MAX_SSID_LEN 32
#define MAX_NUM_MLO_VDEV 7

enum QCA_VDEV_BAND {
	QCA_VDEV_BAND_INVALID,	/* Invalid radio band */
	QCA_VDEV_BAND_2G,	/* 2G Radio Band */
	QCA_VDEV_BAND_5G,	/* 5G Radio Band */
	QCA_VDEV_BAND_5GH,	/* 5GH RADIO BAND */
	QCA_VDEV_BAND_5GL,	/* 5GL RADIO BAND */
	QCA_VDEV_BAND_6G,	/* 6G RADIO BAND */
	QCA_VDEV_BAND_6GH,	/* 6GH RADIO BAND */
	QCA_VDEV_BAND_6GL,	/* 6GL RADIO BAND */
	QCA_MAX_VDEV_PER_ML,	/* MAX VDEV */
};

enum QCA_VDEV_BW {
	SAWF_BW_BW_INVALID,	/* Invalid radio band */
	SAWF_BW_20MHZ,
	SAWF_BW_40MHZ,
	SAWF_BW_80MHZ,
	SAWF_BW_160MHZ,
	SAWF_BW_80_80MHZ,
	SAWF_BW_320MHZ,
	SAWF_BW_MAX,		/* Maximum bandwidth */
};

/* qca_sawf_metadata_param
 *
 * @netdev : Netdevice
 * @peer_mac : Destination peer mac address
 * @service_id : Service class id
 * @dscp : Differentiated Services Code Point
 * @rule_id : Rule id
 * @sawf_rule_type: Rule type
 * @pcp: pcp value
 * @valid_flag: flag to indicate if pcp is valid or not
 * @mcast_flag: flag to indicate if query is for multicast
 */
struct qca_sawf_metadata_param {
	struct net_device *netdev;
	uint8_t *peer_mac;
	uint32_t service_id;
	uint32_t dscp;
	uint32_t rule_id;
	uint8_t sawf_rule_type;
	uint32_t pcp;
	uint32_t valid_flag;
	uint8_t mcast_flag:1;
};

/*
 * qca_sawf_connection_sync_param
 *
 * @dst_dev: Destination netdev
 * @src_dev: Source netdev
 * @dst_mac: Destination MAC address
 * @src_mac: Source MAC address
 * @fw_service_id: Service ID matching in forward direction
 * @rv_service_id: Service ID matching in reverse direction
 * @start_or_stop: Connection stat or stop indication 1:start, 2:stop
 * @fw_mark_metadata: forward metadata
 * @rv_mark_metadata: reverse metadata
 */
struct qca_sawf_connection_sync_param {
	struct net_device *dst_dev;
	struct net_device *src_dev;
	uint8_t *dst_mac;
	uint8_t *src_mac;
	uint8_t fw_service_id;
	uint8_t rv_service_id;
	uint8_t start_or_stop;
	uint32_t fw_mark_metadata;
	uint32_t rv_mark_metadata;
};

/*
 * qca_sawf_ip_addr
 *
 * @v4_addr: ipv4 IP address
 * @v6_addr: ipv6 IP addr
 */
union qca_sawf_ip_addr {
		u_int32_t v4_addr;
		struct in6_addr v6_addr;
};

/*
 * qca_sawf_mcast_connection_sync_param
 *
 * @src: src IP addr
 * @dest: src IP addr
 * @src_dev_name: source netdev name
 * @dest_dev_name: destination netdev names
 * @dest_dev_count: destination netdev count
 * @add_or_sub: 1:start, 2:stop
 * @ip_version: ipv6/ipv4
 */
typedef struct qca_sawf_mcast_connection_sync_param {
	union qca_sawf_ip_addr src;
	union qca_sawf_ip_addr dest;
	int32_t src_ifindex;
	int32_t dest_ifindex[QCA_SAWF_MCAST_IF_MAX];
	uint32_t dest_dev_count;
	uint8_t add_or_sub;
	uint16_t ip_version;
} qca_sawf_mcast_sync_param_t;

/*
 * qca_sawf_flow_deprioritize_params
 *
 * @peer_mac: peer mac address
 * @mark_metadata: mark metadata
 * @netdev_info_valid: flag to indicate if netdev info is valid
 * @netdev_ifindex: netdev interface index
 * @netdev_mac: netdev mac address
 */
struct qca_sawf_flow_deprioritize_params {
	uint8_t peer_mac[MAC_ADDR_SIZE];
	uint32_t mark_metadata;
	bool netdev_info_valid;
	uint32_t netdev_ifindex;
	uint8_t netdev_mac[MAC_ADDR_SIZE];
};

/*
 * qca_sawf_flow_deprioritize_resp_params
 *
 * @netdev: netdev pointer
 * @mac_addr: peer mac addr
 * @service_id: service id
 * @success_count: no. of flows successfully deprioritized
 * @fail_count: no. of flows which failed to deprioritize
 * @mark_metadata: mark metadata
 */
struct qca_sawf_flow_deprioritize_resp_params {
	struct net_device *netdev;
	uint8_t *mac_addr;
	uint8_t service_id;
	uint16_t success_count;
	uint16_t fail_count;
	uint32_t mark_metadata;
};

/*
 * qca_sawf_radio_params
 *
 * @flag: OR/AND flag
 * @value: bandwidth/band/channel value
 */
struct qca_sawf_radio_params {
	bool flag;
	uint8_t value[MAX_NUM_MLO_VDEV];
};

/*
 * qca_sawf_wifi_port_params
 *
 * @ssid: AP ssid
 * @ssid_len: ssid length
 * @bssid: Basic service set identifier
 * @ra_mac: Receiver mac address
 * @ta_mac: Transmitter mac address
 * @ac: access category
 * @valid_flag: flag to indicate if pcp is valid or not
 * @priority: Traffic priority set by user
 * @band: Band 2G/5G/6G
 * @channel: channel options
 * @bw: bandwidth options
 * @dscp: Differentiated Services Code Point
 * @pcp: pcp value
 */
struct qca_sawf_wifi_port_params {
	uint8_t *ssid;
	uint8_t ssid_len;
	uint8_t *bssid;
	uint8_t *ra_mac;
	uint8_t *ta_mac;
	uint8_t ac;
	uint8_t valid_flags;
	uint8_t priority;
	struct qca_sawf_radio_params band;
	struct qca_sawf_radio_params channel;
	struct qca_sawf_radio_params bw;
	uint8_t dscp;
	uint8_t pcp;
};

struct qca_sawf_wifi_port_params_v2 {
	uint8_t ssid[MAX_SSID_LEN + 1];
	uint8_t bssid[MAC_ADDR_SIZE];
	uint8_t ra_mac[MAC_ADDR_SIZE];
	uint8_t ta_mac[MAC_ADDR_SIZE];
	struct qca_sawf_radio_params band;
	struct qca_sawf_radio_params channel;
	struct qca_sawf_radio_params bw;
	uint8_t ssid_len;
	uint8_t dscp;
	uint8_t pcp;
	uint8_t ac;
	uint8_t valid_flags;
	uint8_t priority;
};

uint16_t qca_sawf_get_msduq(struct net_device *netdev,
			    uint8_t *peer_mac, uint32_t service_id);
uint16_t qca_sawf_get_msduq_v2(struct net_device *netdev, uint8_t *peer_mac,
			       uint32_t service_id, uint32_t dscp_pcp,
			       uint32_t rule_id, uint8_t sawf_rule_type,
			       bool pcp);
uint32_t qca_sawf_get_mark_metadata(
		struct qca_sawf_metadata_param *params);
uint16_t qca_sawf_get_msdu_queue(struct net_device *netdev,
				 uint8_t *peer_mac, uint32_t service_id,
				 uint32_t dscp, uint32_t rule_id);

/*
 * qca_sawf_config_ul() - Set Uplink QoS parameters
 *
 * @dst_dev: Destination netdev
 * @src_dev: Source netdev
 * @dst_mac: Destination MAC address
 * @src_mac: Source MAC address
 * @fw_service_id: Service class ID in forward direction
 * @rv_service_id: Service class ID in reverse direction
 * @add_or_sub: Add or Sub param
 * @fw_mark_metadata: Forward direction metadata
 * @rv_mark_metadata: Reverse direction metadata
 *
 * Return: void
 */
void qca_sawf_config_ul(struct net_device *dst_dev, struct net_device *src_dev,
			uint8_t *dst_mac, uint8_t *src_mac,
			uint8_t fw_service_id, uint8_t rv_service_id,
			uint8_t add_or_sub, uint32_t fw_mark_metadata,
			uint32_t rv_mark_metadata);

/*
 * qca_sawf_connection_sync() - Update connection status
 *
 * @param: connection sync parameters
 *
 * Return: void
 */
void qca_sawf_connection_sync(struct qca_sawf_connection_sync_param *param);

/*
 * qca_sawf_mcast_connection_sync() - Update connection status
 *
 * @param: connection sync parameters
 *
 * Return: void
 */
void qca_sawf_mcast_connection_sync(qca_sawf_mcast_sync_param_t *params);

/*
 * qca_sawf_register_flow_deprioritize_callback() - Flow deprioritization
 * registration function
 *
 * sawf_flow_deprioritize_callback: callback function address
 *
 * Return: bool
 */
bool qca_sawf_register_flow_deprioritize_callback(void (*sawf_flow_deprioritize_callback)(struct qca_sawf_flow_deprioritize_params *params));

/*
 * qca_sawf_unregister_flow_deprioritize_callback() - Flow deprioritization
 * unregistration function
 *
 * Return: None
 */
void qca_sawf_unregister_flow_deprioritize_callback(void);

/*
 * qca_sawf_flow_deprioritize_response() - Flow deprioritization response
 * handler
 *
 * @params: flow deprioritization response params
 * Return: None
 */
void qca_sawf_flow_deprioritize_response(struct qca_sawf_flow_deprioritize_resp_params *params);

/*
 * qca_sdwf_match_wifi_port_params() - Check if wifi port params match
 *
 * @netdev: Netdevice
 * @dest_mac: Destination mac address
 * @priority: Traffic priority set by user
 * @wp: SWDF wifi parameters parsed and sent by SPM
 *
 * Return: true if rule match found else false
 */
bool qca_sdwf_match_wifi_port_params(struct net_device *netdev,
				     uint8_t *dest_mac,
				     struct net_device *src_dev,
				     uint8_t *src_mac,
				     uint8_t priority,
				     struct qca_sawf_wifi_port_params_v2 *wp);

/*
 * qca_sdwf_match_wifi_port_params_v2() - Check if wifi port params match
 *
 * @dst_dev: Destination netdevice
 * @dst_mac: Destination mac address
 * @src_dev: Source netdevice
 * @src_mac: Source mac address
 * @priority: Traffic priority set by user
 * @wp: SWDF wifi parameters parsed and sent by SPM
 *
 * Return: true if rule match found else false
 */
bool qca_sdwf_match_wifi_port_params_v2(struct net_device *dst_dev,
					uint8_t *dest_mac,
					struct net_device *src_dev,
					uint8_t *src_mac,
					uint8_t priority,
					struct qca_sawf_wifi_port_params *wp);


/*
 * qca_sdwf_validate_wifi_port_params() - Check if wifi port params are valid
 *
 * @wp: SWDF wifi parameters parsed and sent by SPM
 *
 * Return: true if parameters are valid, false if invalid
 */
bool qca_sdwf_validate_wifi_port_params(struct qca_sawf_wifi_port_params *wp);

/*
 * qca_sdwf_validate_wifi_port_params() - Check if wifi port params are valid
 *
 * @wp: SWDF wifi parameters parsed and sent by SPM
 *
 * Return: true if parameters are valid, false if invalid
 */
bool qca_sdwf_validate_wifi_port_params_v2(struct qca_sawf_wifi_port_params_v2 *wp);
#endif
