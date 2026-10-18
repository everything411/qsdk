/*
 * Copyright (c) 2014, 2016-2017, 2021, The Linux Foundation. All rights reserved.
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: ISC
 */


/**
 * @defgroup fal_type FAL_TYPE
 * @{
 */
#ifndef _FAL_TYPE_H_
#define _FAL_TYPE_H_

#ifdef __cplusplus
extern "C" {
#endif                          /* __cplusplus */

    typedef a_uint32_t fal_port_t;

/*fal_port_t definition,
	bit31-bit24: port_type, 0-physical port, 1-trunk port, 2-virtual port, 3-virtual port group,
		4-gem port
	bit23-bit0: physical port id or trunk id or virtual port id*/
#define FAL_PORT_TYPE_PPORT 0
#define FAL_PORT_TYPE_TRUNK 1
#define FAL_PORT_TYPE_VPORT 2
#define FAL_PORT_TYPE_VP_GROUP 3
#define FAL_PORT_TYPE_GEM_PORT 4
#define FAL_PORT_TYPE_INVALID  0xff

#define FAL_PORT_ID_TYPE(port_id) (((port_id)>>24)&0xff)
#define FAL_PORT_ID_VALUE(port_id) ((port_id)&0xffffff)
#define FAL_PORT_ID(type, value) (((type)<<24)|(value))

#define FAL_IS_PPORT(port_id) (((FAL_PORT_ID_TYPE(port_id))==FAL_PORT_TYPE_PPORT)?1:0)
#define FAL_IS_TRUNK(port_id) (((FAL_PORT_ID_TYPE(port_id))==FAL_PORT_TYPE_TRUNK)?1:0)
#define FAL_IS_VPORT(port_id) (((FAL_PORT_ID_TYPE(port_id))==FAL_PORT_TYPE_VPORT)?1:0)
#define FAL_IS_VP_GROUP(port_id) (((FAL_PORT_ID_TYPE(port_id))==FAL_PORT_TYPE_VP_GROUP)?1:0)
#define FAL_IS_GEM_PORT(port_id) (((FAL_PORT_ID_TYPE(port_id))==FAL_PORT_TYPE_GEM_PORT)?1:0)

/*
* Source info change types for vlan translation/tunnel decap
*/
#define FAL_CHG_SRC_TYPE_VP            0x0
#define FAL_CHG_SRC_L3_IF_TUNNEL       0x1

/* fal_pbmp_t definition,
 * bit31-bit24: port_type, 0-physical port bitmap, 1-trunk port, 2-virtual port, 3-vport group, 4-Gem port
 * bit23-bit0: physical port bitmap or trunk id or virtual port id or vp group id
 */
#if (SW_MAX_NR_PORT <= 32)
    typedef a_uint32_t fal_pbmp_t;
#else
    typedef a_uint64_t fal_pbmp_t;
#endif

    typedef struct
    {
        a_uint8_t uc[6];
    } fal_mac_addr_t;

    typedef a_uint32_t fal_ip4_addr_t;

    typedef struct
    {
        a_uint32_t ul[4];
    } fal_ip6_addr_t;

    typedef enum
    {
        FAL_L3_TYPE_OTHERS = 0,
        FAL_L3_TYPE_IPV4,
        FAL_L3_TYPE_ARP,
        FAL_L3_TYPE_IPV6,
        FAL_L3_TYPE_BUTT,
    } fal_l3_type_t;

    typedef enum
    {
        FAL_L4_TYPE_OTHERS = 0,
        FAL_L4_TYPE_TCP,
        FAL_L4_TYPE_UDP,
        FAL_L4_TYPE_UDP_LITE,
        FAL_L4_TYPE_ICMP,
        FAL_L4_TYPE_GRE,
        FAL_L4_TYPE_BUTT,
    } fal_l4_type_t;

    typedef enum
    {
        FAL_ETHERNET_HDR =0,
        FAL_ETHERNET_TAG_HDR,
        FAL_IPV4_HDR,
        FAL_IPV6_HDR,
        FAL_UDP_HDR,
        FAL_UDP_LITE_HDR,
        FAL_TCP_HDR,
        FAL_GRE_HDR,
        FAL_VXLAN_HDR,
        FAL_VXLAN_GPE_HDR,
        FAL_GENEVE_HDR,
        FAL_HDR_BUTT,
    } fal_hdr_type_t;

    /**
    @brief This enum defines several forwarding command type.
    * Field description:
        FAL_MAC_FRWRD      - packets are normally forwarded
        FAL_MAC_DROP       - packets are dropped
        FAL_MAC_CPY_TO_CPU - packets are copyed to cpu
        FAL_MAC_RDT_TO_CPU - packets are redirected to cpu
    */
    typedef enum
    {
        FAL_MAC_FRWRD = 0,      /**<   packets are normally forwarded */
        FAL_MAC_DROP,           /**<   packets are dropped */
        FAL_MAC_CPY_TO_CPU,     /**<   packets are copyed to cpu */
        FAL_MAC_RDT_TO_CPU      /**<   packets are redirected to cpu */
    } fal_fwd_cmd_t;

    typedef enum
    {
        FAL_BYTE_BASED = 0,
        FAL_FRAME_BASED,
        FAL_RATE_MODE_BUTT
    } fal_traffic_unit_t;

    typedef a_uint32_t fal_queue_t;

#define FAL_SVL_FID   0xffff


    /**
    @brief This enum defines packets transmitted out vlan tagged mode.
    */
    typedef enum
    {
        FAL_EG_UNMODIFIED = 0,  /**<  egress transmit packets unmodified */
        FAL_EG_UNTAGGED,        /**<   egress transmit packets without vlan tag*/
        FAL_EG_TAGGED,          /**<  egress transmit packets with vlan tag     */
        FAL_EG_HYBRID,          /**<  egress transmit packets in hybrid tag mode     */
        FAL_EG_UNTOUCHED,
        FAL_EG_MODE_BUTT
    } fal_pt_1q_egmode_t;

#define FAL_NEXT_ENTRY_FIRST_ID 0xffffffff

	typedef struct{
		a_uint32_t reg_count;
		a_uint32_t reg_base;
		a_uint32_t reg_end;
		a_uint32_t reg_value[256];
		a_int8_t   reg_name[32];
	}fal_reg_dump_t;

	typedef struct{
		a_uint32_t reg_count;
		a_uint32_t reg_addr[32];
		a_uint32_t reg_value[32];
		a_int8_t   reg_name[32];
	}fal_debug_reg_dump_t;

	typedef struct{
		a_uint32_t phy_count;
		a_uint32_t phy_base;
		a_uint32_t phy_end;
		a_uint16_t phy_value[256];
		a_int8_t   phy_name[32];
	}fal_phy_dump_t;

	typedef enum
	{
		FAL_DEST_INFO_PORT_BMP = 0,
		FAL_DEST_INFO_PORT_ID,
		FAL_DEST_INFO_INVALID
	} fal_dest_info_type_t;

	typedef struct
	{
		fal_dest_info_type_t dest_info_type;
		a_uint32_t dest_info_value;
	}fal_dest_info_t;

	typedef struct {
		a_uint32_t matched_pkts; /* entry packet counter */
		a_uint64_t matched_bytes; /* entry byte counter */
	} fal_entry_counter_t;

	typedef struct {
		a_bool_t l3_if_valid; /* 0 for disable and 1 for enable */
		a_uint32_t l3_if_index; /* index for interface table */
	} fal_intf_id_t;

	typedef enum {
		FAL_INTF_TYPE_TUNNEL,
		FAL_INTF_TYPE_NORMAL,
	} fal_intf_type_t;

	typedef enum {
		FAL_DIR_BOTH = 0,
		FAL_DIR_INGRESS = 1,
		FAL_DIR_EGRESS = 2,
	} fal_direction_t;

	typedef enum
	{
		FAL_VLAN_MATCH_VID = 0,
		FAL_VLAN_MATCH_VSI = 1,
	} fal_vlan_match_mode_t;

	typedef enum
	{
		FAL_PCP_MATCH_PCP_DEI = 0,
		FAL_PCP_MATCH_INT_PRI = 1,
	} fal_pcp_match_mode_t;

#ifdef __cplusplus
}
#endif                          /* __cplusplus */
#endif                          /* _FAL_TYPE_H_ */
/**
 * @}
 */
