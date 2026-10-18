/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: ISC
 */

#ifndef _HSL_MISC_H_
#define _HSL_MISC_H_

enum {
	OP_ADD = 0,
	OP_DEL,
	OP_GET,
	OP_FLUSH,
};

#define HASH_MODE	0
#define INDEX_MODE	1

/*[register] TOEPLITZ_SECRET_KEY*/
#define TOEPLITZ_SECRET_KEY
#define TOEPLITZ_SECRET_KEY_ADDRESS 0x4420
#define TOEPLITZ_SECRET_KEY_NUM     11
#define TOEPLITZ_SECRET_KEY_INC     0x4
#define TOEPLITZ_SECRET_KEY_TYPE    REG_TYPE_RW

/*[register]*/
#define IPO_MASK_REG
#define IPO_MASK_REG_ADDRESS 0x2000
#if defined(HMSPPE)
#define IPO_MASK_REG_NUM     128
#else
#define IPO_MASK_REG_NUM     512
#endif
#define IPO_MASK_REG_INC     0x10
#define IPO_MASK_REG_TYPE    REG_TYPE_RW
#define IPO_MASK_REG_DEFAULT 0x0

#define IPO_RULE_REG
#define IPO_RULE_REG_ADDRESS 0x0
#if defined(HMSPPE)
#define IPO_RULE_REG_NUM     128
#else
#define IPO_RULE_REG_NUM     512
#endif
#define IPO_RULE_REG_INC     0x10
#define IPO_RULE_REG_TYPE    REG_TYPE_RW
#define IPO_RULE_REG_DEFAULT 0x0

/*[table] */
#define FLOW_HOST_TBL_RD_RSLT_DATA
#define FLOW_HOST_TBL_RD_RSLT_DATA_ADDRESS 0x6f8
#define FLOW_HOST_TBL_RD_RSLT_DATA_NUM     10
#define FLOW_HOST_TBL_RD_RSLT_DATA_INC     0x4
#define FLOW_HOST_TBL_RD_RSLT_DATA_TYPE    REG_TYPE_RO
#define FLOW_HOST_TBL_RD_RSLT_DATA_DEFAULT 0x0

/*[table] */
#define IN_FLOW_TBL_OP_DATA
#define IN_FLOW_TBL_OP_DATA_ADDRESS 0x550
#define IN_FLOW_TBL_OP_DATA_NUM     12
#define IN_FLOW_TBL_OP_DATA_INC     0x4
#define IN_FLOW_TBL_OP_DATA_TYPE    REG_TYPE_RW
#define IN_FLOW_TBL_OP_DATA_DEFAULT 0x0

/*[register] HOST_TBL_OP_DATA0*/
#define HOST_TBL_OP_DATA
#define HOST_TBL_OP_DATA_ADDRESS 0x744
#define HOST_TBL_OP_DATA_NUM     10
#define HOST_TBL_OP_DATA_INC     0x4
#define HOST_TBL_OP_DATA_TYPE    REG_TYPE_RW
#define HOST_TBL_OP_DATA_DEFAULT 0x0

/*[table] */
#define FLOW_HOST_TBL_OP_DATA
#define FLOW_HOST_TBL_OP_DATA_ADDRESS 0x590
#define FLOW_HOST_TBL_OP_DATA_NUM     10
#define FLOW_HOST_TBL_OP_DATA_INC     0x4
#define FLOW_HOST_TBL_OP_DATA_TYPE    REG_TYPE_RW
#define FLOW_HOST_TBL_OP_DATA_DEFAULT 0x0

/*[table] */
#define IN_FLOW_TBL_RD_OP_DATA
#define IN_FLOW_TBL_RD_OP_DATA_ADDRESS 0x618
#define IN_FLOW_TBL_RD_OP_DATA_NUM     12
#define IN_FLOW_TBL_RD_OP_DATA_INC     0x4
#define IN_FLOW_TBL_RD_OP_DATA_TYPE    REG_TYPE_RW
#define IN_FLOW_TBL_RD_OP_DATA_DEFAULT 0x0

#define IN_FLOW_TBL_RD_RSLT_DATA
#define IN_FLOW_TBL_RD_RSLT_DATA_ADDRESS 0x6b8
#define IN_FLOW_TBL_RD_RSLT_DATA_NUM     12
#define IN_FLOW_TBL_RD_RSLT_DATA_INC     0x4
#define IN_FLOW_TBL_RD_RSLT_DATA_TYPE    REG_TYPE_RO
#define IN_FLOW_TBL_RD_RSLT_DATA_DEFAULT 0x0

/*[table] */
#define HOST_TBL_RD_OP_DATA
#define HOST_TBL_RD_OP_DATA_ADDRESS 0x7a4
#define HOST_TBL_RD_OP_DATA_NUM     10
#define HOST_TBL_RD_OP_DATA_INC     0x4
#define HOST_TBL_RD_OP_DATA_TYPE    REG_TYPE_RW
#define HOST_TBL_RD_OP_DATA_DEFAULT 0x0

/*[register]*/
#define HOST_TBL_RD_RSLT_DATA
#define HOST_TBL_RD_RSLT_DATA_ADDRESS 0x7f0
#define HOST_TBL_RD_RSLT_DATA_NUM     10
#define HOST_TBL_RD_RSLT_DATA_INC     0x4
#define HOST_TBL_RD_RSLT_DATA_TYPE    REG_TYPE_RO
#define HOST_TBL_RD_RSLT_DATA_DEFAULT 0x0

/*[register] FLOW_HOST_TBL_RD_OP_DATA*/
#define FLOW_HOST_TBL_RD_OP_DATA
#define FLOW_HOST_TBL_RD_OP_DATA_ADDRESS 0x658
#define FLOW_HOST_TBL_RD_OP_DATA_NUM     10
#define FLOW_HOST_TBL_RD_OP_DATA_INC     0x4
#define FLOW_HOST_TBL_RD_OP_DATA_TYPE    REG_TYPE_RW
#define FLOW_HOST_TBL_RD_OP_DATA_DEFAULT 0x0

/*[table] PRE_IPO_MASK_REG*/
#define PRE_IPO_MASK_REG
#define PRE_IPO_MASK_REG_ADDRESS 0x2000
#if defined(HMSPPE)
#define PRE_IPO_MASK_REG_NUM     128
#else
#define PRE_IPO_MASK_REG_NUM     512
#endif
#define PRE_IPO_MASK_REG_INC     0x10
#define PRE_IPO_MASK_REG_TYPE    REG_TYPE_RW
#define PRE_IPO_MASK_REG_DEFAULT 0x0

/*[table] PRE_IPO_RULE_REG*/
#define PRE_IPO_RULE_REG
#define PRE_IPO_RULE_REG_ADDRESS 0x0
#if defined(HMSPPE)
#define PRE_IPO_RULE_REG_NUM     128
#else
#define PRE_IPO_RULE_REG_NUM     512
#endif
#define PRE_IPO_RULE_REG_INC     0x10
#define PRE_IPO_RULE_REG_TYPE    REG_TYPE_RW
#define PRE_IPO_RULE_REG_DEFAULT 0x0

/*[register] PORT_MUX_CTRL*/
#define SWITCH_PORT_MUX_CTRL
#define SWITCH_PORT_MUX_CTRL_ADDRESS 0x10
#define SWITCH_PORT_MUX_CTRL_NUM     1
#define SWITCH_PORT_MUX_CTRL_INC     0x4
#define SWITCH_PORT_MUX_CTRL_TYPE    REG_TYPE_RW
#define SWITCH_PORT_MUX_CTRL_DEFAULT 0x0

/* [register] PON_MODE */
#define SWITCH_PON_MODE
#define SWITCH_PON_MODE_ADDRESS 0x54
#define SWITCH_PON_MODE_NUM     1
#define SWITCH_PON_MODE_INC     0x4
#define SWITCH_PON_MODE_TYPE    REG_TYPE_RW
#define SWITCH_PON_MODE_DEFAULT 0x0

struct hmsppe_pon_mode {
	a_uint32_t  pon_mode:1;
	a_uint32_t  _reserved1:31;
};

union hmsppe_pon_mode_u {
	a_uint32_t val;
	struct hmsppe_pon_mode bf;
};

/*[register] TDM_CTRL*/
#define TDM_CTRL
#define TDM_CTRL_ADDRESS 0x0
#define TDM_CTRL_NUM     1
#define TDM_CTRL_INC     0x4
#define TDM_CTRL_TYPE    REG_TYPE_RW
#define TDM_CTRL_DEFAULT 0x80000050

/*[table] TDM_CFG*/
#define TDM_CFG_MAX_ENTRY	128
#define TDM_CFG
#define TDM_CFG_ADDRESS 0x1000
#define TDM_CFG_NUM     128
#if defined(HMSPPE)
#define TDM_CFG_INC     0x4
#else
#define TDM_CFG_INC     0x10
#endif
#define TDM_CFG_TYPE    REG_TYPE_RW
#define TDM_CFG_DEFAULT 0x0

/*[register] RX_FIFO_CFG*/
#define RX_FIFO_CFG_MAX_ENTRY	8
#define RX_FIFO_CFG
#define RX_FIFO_CFG_ADDRESS 0x4
#define RX_FIFO_CFG_NUM     8
#define RX_FIFO_CFG_INC     0x4
#define RX_FIFO_CFG_TYPE    REG_TYPE_RW
#define RX_FIFO_CFG_DEFAULT 0x4

/*[table] DROP_STAT*/
#define DROP_STAT
#define DROP_STAT_ADDRESS 0x3000
#if defined(HMSPPE)
#define DROP_STAT_NUM     78
#elif defined(JHPPE)
#define DROP_STAT_NUM     80
#elif defined(MRPPE)
#define DROP_STAT_NUM     38
#else
#define DROP_STAT_NUM     30
#endif
#define DROP_STAT_INC     0x10
#define DROP_STAT_TYPE    REG_TYPE_RW
#define DROP_STAT_DEFAULT 0x0

/*[register] PORT_FC_STATUS*/
#define PORT_FC_STATUS
#define PORT_FC_STATUS_ADDRESS 0x1a0
#define PORT_FC_STATUS_NUM     40
#define PORT_FC_STATUS_INC     0x4
#define PORT_FC_STATUS_TYPE    REG_TYPE_RO
#define PORT_FC_STATUS_DEFAULT 0x0
	/*[field] PORT_XON_TH*/
	#define PORT_FC_STATUS_PORT_XON_TH
	#define PORT_FC_STATUS_PORT_XON_TH_OFFSET  0
	#define PORT_FC_STATUS_PORT_XON_TH_LEN     12
	#define PORT_FC_STATUS_PORT_XON_TH_DEFAULT 0x0
	/*[field] PORT_FC_STATUS*/
	#define PORT_FC_STATUS_PORT_FC_STATUS
	#define PORT_FC_STATUS_PORT_FC_STATUS_OFFSET  17
	#define PORT_FC_STATUS_PORT_FC_STATUS_LEN     2
	#define PORT_FC_STATUS_PORT_FC_STATUS_DEFAULT 0x0

struct port_fc_status {
	a_uint32_t  port_xon_th:12;
	a_uint32_t  _reserved0:5;
	a_uint32_t  port_fc_status:2;
	a_uint32_t  _reserved1:13;
};

union port_fc_status_u {
	a_uint32_t val;
	struct port_fc_status bf;
};

/*[register] LPI_ENABLE*/
#define LPI_PORT_ENABLE
#define LPI_PORT_ENABLE_ADDRESS 0xf4
#define LPI_PORT_ENABLE_NUM     3
#define LPI_PORT_ENABLE_INC     0x200
#define LPI_PORT_ENABLE_TYPE    REG_TYPE_RW
#define LPI_PORT_ENABLE_DEFAULT 0x0

/*[register] LPI_PORT_WAKEUP_TIMER*/
#define LPI_PORT_WAKEUP_TIMER_ENABLE
#define LPI_PORT_WAKEUP_TIMER_ADDRESS 0xf8
#define LPI_PORT_WAKEUP_TIMER_NUM     3
#define LPI_PORT_WAKEUP_TIMER_INC     0x200
#define LPI_PORT_WAKEUP_TIMER_TYPE    REG_TYPE_RW
#define LPI_PORT_WAKEUP_TIMER_DEFAULT 0x20

/*[register] LPI_PORT_SLEEP_TIMER*/
#define LPI_PORT_SLEEP_TIMER_ENABLE
#define LPI_PORT_SLEEP_TIMER_ADDRESS 0xfc
#define LPI_PORT_SLEEP_TIMER_NUM     3
#define LPI_PORT_SLEEP_TIMER_INC     0x200
#define LPI_PORT_SLEEP_TIMER_TYPE    REG_TYPE_RW
#define LPI_PORT_SLEEP_TIMER_DEFAULT 0x100

/*[register] LPI_1US_CNT*/
#define LPI_1US_CNT
#define LPI_1US_CNT_ADDRESS 0x104
#define LPI_1US_CNT_NUM     3
#define LPI_1US_CNT_INC     0x200
#define LPI_1US_CNT_TYPE    REG_TYPE_RW
#define LPI_1US_CNT_DEFAULT 0x177

/*[register] LPI_PORT_TIMER*/
#define LPI_PORT_TIMER_ENABLE
#define LPI_PORT_TIMER_ADDRESS 0x0
#define LPI_PORT_TIMER_NUM     6
#define LPI_PORT_TIMER_INC     0x4
#define LPI_PORT_TIMER_TYPE    REG_TYPE_RW
#define LPI_PORT_TIMER_DEFAULT 0x0

/*[register] LPI_ENABLE*/
#define LPI_ENABLE
#define LPI_ENABLE_ADDRESS 0x0
#define LPI_ENABLE_NUM     1
#define LPI_ENABLE_INC     0x0
#define LPI_ENABLE_TYPE    REG_TYPE_RW
#define LPI_ENABLE_DEFAULT 0x0

/*[register] LPI_CNT*/
#define LPI_CNT
#define LPI_CNT_ADDRESS 0x30
#define LPI_CNT_NUM     1
#define LPI_CNT_INC     0x1
#define LPI_CNT_TYPE    REG_TYPE_RW
#define LPI_CNT_DEFAULT 0x0

/*[register] DROP_CNT*/
#define DROP_CNT
#define DROP_CNT_ADDRESS 0x24
#if defined(JHPPE)
#define DROP_CNT_NUM     9
#elif defined(MRPPE)
#define DROP_CNT_NUM     4
#elif defined(MPPE)
#define DROP_CNT_NUM     3
#else
#define DROP_CNT_NUM     8
#endif
#define DROP_CNT_INC     0x4
#define DROP_CNT_TYPE    REG_TYPE_RW
#define DROP_CNT_DEFAULT 0x0


struct pre_ipo_mask_reg {
	a_uint32_t  maskfield_0:32;
	a_uint32_t  maskfield_1:21;
	a_uint32_t  _reserved0:11;
};

union pre_ipo_mask_reg_u {
	a_uint32_t val[2];
	struct pre_ipo_mask_reg bf;
};

struct pre_ipo_rule_reg {
	a_uint32_t rule_field_0:32;
	a_uint32_t rule_field_1:20;
	a_uint32_t fake_mac_header:1;
	a_uint32_t range_en:1;
	a_uint32_t inverse_en:1;
	a_uint32_t rule_type:5;
	a_uint32_t src_type:3;
	a_uint32_t src_0:1;
	a_uint32_t src_1:8;
	a_uint32_t pri:9;
	a_uint32_t res_chain:1;
	a_uint32_t inner_outer_sel:1;
	a_uint32_t _reserved1:13;
};

union pre_ipo_rule_reg_u {
	a_uint32_t val[3];
	struct pre_ipo_rule_reg bf;
};

struct ipo_mask_reg {
	a_uint32_t  maskfield_0:32;
	a_uint32_t  maskfield_1:21;
	a_uint32_t  _reserved0:11;
};

union ipo_mask_reg_u {
	a_uint32_t val[2];
	struct ipo_mask_reg bf;
};

struct ipo_rule_reg {
	a_uint32_t rule_field_0:32;
	a_uint32_t rule_field_1:20;
	a_uint32_t fake_mac_header:1;
	a_uint32_t range_en:1;
	a_uint32_t inverse_en:1;
	a_uint32_t rule_type:5;
	a_uint32_t src_type:3;
	a_uint32_t src_0:1;
	a_uint32_t src_1:8;
	a_uint32_t pri:9;
	a_uint32_t res_chain:1;
	a_uint32_t post_routing_en:1;
	a_uint32_t _reserved0:13;
};

union ipo_rule_reg_u {
	a_uint32_t val[3];
	struct ipo_rule_reg bf;
};

struct drop_stat {
	a_uint32_t  pkts:32;
	a_uint32_t  bytes_0:32;
	a_uint32_t  bytes_1:8;
	a_uint32_t  _reserved0:24;
};

union drop_stat_u {
	a_uint32_t val[3];
	struct drop_stat bf;
};

struct lpi_cnt {
	a_uint32_t  lpi_cnt_val:9;
	a_uint32_t  _reserved0:23;
};

union lpi_cnt_u {
	a_uint32_t val;
	struct lpi_cnt bf;
};

struct lpi_enable {
	a_uint32_t  lpi_port1_en:1;
	a_uint32_t  lpi_port2_en:1;
	a_uint32_t  lpi_port3_en:1;
	a_uint32_t  lpi_port4_en:1;
	a_uint32_t  lpi_port5_en:1;
	a_uint32_t  lpi_port6_en:1;
	a_uint32_t  _reserved0:26;
};

union lpi_enable_u {
	a_uint32_t val;
	struct lpi_enable bf;
};

struct lpi_port_timer {
	a_uint32_t  lpi_port_wakeup_timer:16;
	a_uint32_t  lpi_port_sleep_timer:16;
};

union lpi_port_timer_u {
	a_uint32_t val;
	struct lpi_port_timer bf;
};

struct appe_port_mux_ctrl {
	a_uint32_t  port1_pcs_sel:1;
	a_uint32_t  port2_pcs_sel:1;
	a_uint32_t  port3_pcs_sel:1;
	a_uint32_t  port4_pcs_sel:1;
	a_uint32_t  port5_pcs_sel:1;
	a_uint32_t  port6_pcs_sel:1;
	a_uint32_t  _reserved0:2;
	a_uint32_t  port1_mac_sel:1;
	a_uint32_t  port2_mac_sel:1;
	a_uint32_t  port3_mac_sel:1;
	a_uint32_t  port4_mac_sel:1;
	a_uint32_t  port5_mac_sel:1;
	a_uint32_t  port6_mac_sel:1;
	a_uint32_t  _reserved1:18;
};

union appe_port_mux_ctrl_u {
	a_uint32_t val;
	struct appe_port_mux_ctrl bf;
};

struct rx_fifo_cfg {
	a_uint32_t  rx_fifo_thres:3;
	a_uint32_t  _reserved0:29;
};

union rx_fifo_cfg_u {
	a_uint32_t val;
	struct rx_fifo_cfg bf;
};

struct tdm_ctrl {
	a_uint32_t  tdm_depth:8;
	a_uint32_t  tdm_offset:7;
	a_uint32_t  _reserved0:16;
	a_uint32_t  tdm_en:1;
};

union tdm_ctrl_u {
	a_uint32_t val;
	struct tdm_ctrl bf;
};

struct tdm_cfg {
	a_uint32_t  port_num:4;
	a_uint32_t  dir:1;
	a_uint32_t  valid:1;
	a_uint32_t  sec_valid:1;
	a_uint32_t  resv:1;
	a_uint32_t  sec_port_num:4;
	a_uint32_t  _reserved0:20;
};

union tdm_cfg_u {
	a_uint32_t val;
	struct tdm_cfg bf;
};

struct drop_cnt {
	a_uint32_t  drop_cnt:32;
};

union drop_cnt_u {
	a_uint32_t val;
	struct drop_cnt bf;
};

struct lpi_port_sleep_timer {
	a_uint32_t  lpi_port_sleep_timer:16;
	a_uint32_t  _reserved0:16;
};

union lpi_port_sleep_timer_u {
	a_uint32_t val;
	struct lpi_port_sleep_timer bf;
};

struct lpi_port_enable {
	a_uint32_t  lpi_port_en:1;
	a_uint32_t  _reserved0:31;
};

union lpi_port_enable_u {
	a_uint32_t val;
	struct lpi_port_enable bf;
};

struct lpi_1us_cnt {
	a_uint32_t  lpi_1us_cnt_val:9;
	a_uint32_t  _reserved0:23;
};

union lpi_1us_cnt_u {
	a_uint32_t val;
	struct lpi_1us_cnt bf;
};

struct lpi_port_wakeup_timer {
	a_uint32_t  lpi_port_wakeup_timer:16;
	a_uint32_t  _reserved0:16;
};

union lpi_port_wakeup_timer_u {
	a_uint32_t val;
	struct lpi_port_wakeup_timer bf;
};

struct toeplitz_secret_key {
	a_uint32_t  key_word:32;
};

union toeplitz_secret_key_u {
	a_uint32_t val;
	struct toeplitz_secret_key bf;
};

//from adpt_appe_acl.c
sw_error_t
appe_pre_ipo_rule_reg_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		union pre_ipo_rule_reg_u *value);

sw_error_t
appe_pre_ipo_rule_reg_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		union pre_ipo_rule_reg_u *value);

sw_error_t
appe_pre_ipo_mask_reg_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		union pre_ipo_mask_reg_u *value);

sw_error_t
appe_pre_ipo_mask_reg_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		union pre_ipo_mask_reg_u *value);
sw_error_t
appe_l3_vp_port_tbl_ipo_vp_profile_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);
sw_error_t
appe_l3_vp_port_tbl_ipo_vp_profile_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

#define PRE_IPO_RULE_REG_SRC_OFFSET  63
#define IPO_RULE_REG_SRC_OFFSET  63

//from adpt_appe_portvlan.c
sw_error_t
appe_vp_isol_tbl_vp_profile_map_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint64_t value);

sw_error_t
appe_vp_isol_tbl_vp_profile_map_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint64_t *value);

//from adpt_appe_tunnel_program.c
sw_error_t
appe_tpr_program_hdr_hdr_type_map_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);
sw_error_t
appe_tpr_program_hdr_hdr_type_map_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

//from adpt_hppe_acl.c
sw_error_t
hppe_ipo_rule_reg_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		union ipo_rule_reg_u *value);
sw_error_t
hppe_ipo_rule_reg_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		union ipo_rule_reg_u *value);
sw_error_t
hppe_ipo_mask_reg_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		union ipo_mask_reg_u *value);

sw_error_t
hppe_ipo_mask_reg_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		union ipo_mask_reg_u *value);

//from adpt_hppe_ip.c
sw_error_t
hppe_host_ipv4_add(
		a_uint32_t dev_id, a_uint32_t op_mode,
		a_uint32_t *index, union host_tbl_u *entry);

sw_error_t
hppe_host_ipv6_add(
		a_uint32_t dev_id, a_uint32_t op_mode,
		a_uint32_t *index, union host_ipv6_tbl_u *entry);

sw_error_t
hppe_host_ipv4_mcast_add(
		a_uint32_t dev_id, a_uint32_t op_mode,
		a_uint32_t *index, union host_ipv4_mcast_tbl_u *entry);

sw_error_t
hppe_host_ipv6_mcast_add(
		a_uint32_t dev_id, a_uint32_t op_mode,
		a_uint32_t *index, union host_ipv6_mcast_tbl_u *entry);

sw_error_t
hppe_host_ipv4_del(
		a_uint32_t dev_id, a_uint32_t op_mode,
		a_uint32_t *index, union host_tbl_u *entry);

sw_error_t
hppe_host_ipv6_del(
		a_uint32_t dev_id, a_uint32_t op_mode,
		a_uint32_t *index, union host_ipv6_tbl_u *entry);

sw_error_t
hppe_host_ipv4_mcast_del(
		a_uint32_t dev_id, a_uint32_t op_mode,
		a_uint32_t *index, union host_ipv4_mcast_tbl_u *entry);

sw_error_t
hppe_host_ipv6_mcast_del(
		a_uint32_t dev_id, a_uint32_t op_mode,
		a_uint32_t *index, union host_ipv6_mcast_tbl_u *entry);

sw_error_t
hppe_host_ipv4_get(
		a_uint32_t dev_id, a_uint32_t op_mode,
		a_uint32_t *index, union host_tbl_u *entry);

sw_error_t
hppe_host_ipv6_get(
		a_uint32_t dev_id, a_uint32_t op_mode,
		a_uint32_t *index, union host_ipv6_tbl_u *entry);

sw_error_t
hppe_host_ipv4_mcast_get(
		a_uint32_t dev_id, a_uint32_t op_mode,
		a_uint32_t *index, union host_ipv4_mcast_tbl_u *entry);

sw_error_t
hppe_host_ipv6_mcast_get(
		a_uint32_t dev_id, a_uint32_t op_mode,
		a_uint32_t *index, union host_ipv6_mcast_tbl_u *entry);

sw_error_t
hppe_host_flush_common(a_uint32_t dev_id);

/* from adpt_hppe_flow.c */
sw_error_t
hppe_flow_host_ipv4_data_rd_add(
		a_uint32_t dev_id, a_uint32_t op_mode,
		a_uint32_t *index, union host_tbl_u *entry);

sw_error_t
hppe_flow_host_ipv6_data_rd_add(
		a_uint32_t dev_id, a_uint32_t op_mode,
		a_uint32_t *index, union host_ipv6_tbl_u *entry);

sw_error_t
hppe_flow_host_ipv4_data_add(
		a_uint32_t dev_id, a_uint32_t op_mode,
		a_uint32_t *index, union host_tbl_u *entry);

sw_error_t
hppe_flow_host_ipv6_data_add(
		a_uint32_t dev_id, a_uint32_t op_mode,
		a_uint32_t *index, union host_ipv6_tbl_u *entry);

sw_error_t
hppe_flow_host_ipv4_data_get(
		a_uint32_t dev_id, a_uint32_t op_mode,
		a_uint32_t *index, union host_tbl_u *entry);

sw_error_t
hppe_flow_host_ipv6_data_get(
		a_uint32_t dev_id, a_uint32_t op_mode,
		a_uint32_t *index, union host_ipv6_tbl_u *entry);

sw_error_t
hppe_flow_host_ipv4_data_del(
		a_uint32_t dev_id, a_uint32_t op_mode,
		a_uint32_t *index, union host_tbl_u *entry);

sw_error_t
hppe_flow_host_ipv6_data_del(
		a_uint32_t dev_id, a_uint32_t op_mode,
		a_uint32_t *index, union host_ipv6_tbl_u *entry);

sw_error_t
hppe_flow_entry_host_op_ipv6_3tuple_del(
		a_uint32_t dev_id, a_uint32_t op_mode,
		a_uint32_t *index, union in_flow_ipv6_3tuple_tbl_u *entry);

sw_error_t
hppe_flow_host_tbl_op_rslt_host_entry_index_get(
		a_uint32_t dev_id,
		unsigned int *value);

sw_error_t
hppe_flow_host_tbl_rd_op_rslt_host_entry_index_get(
		a_uint32_t dev_id,
		unsigned int *value);

sw_error_t
hppe_flow_ipv4_5tuple_add(
		a_uint32_t dev_id, a_uint32_t op_mode,
		a_uint32_t *index, union in_flow_tbl_u *entry);

sw_error_t
hppe_flow_ipv4_3tuple_add(
		a_uint32_t dev_id, a_uint32_t op_mode,
		a_uint32_t *index, union in_flow_3tuple_tbl_u *entry);

sw_error_t
hppe_flow_ipv6_5tuple_add(
		a_uint32_t dev_id, a_uint32_t op_mode,
		a_uint32_t *index, union in_flow_ipv6_5tuple_tbl_u *entry);

sw_error_t
hppe_flow_ipv6_3tuple_add(
		a_uint32_t dev_id, a_uint32_t op_mode,
		a_uint32_t *index, union in_flow_ipv6_3tuple_tbl_u *entry);

sw_error_t
hppe_flow_ipv4_5tuple_del(
		a_uint32_t dev_id, a_uint32_t op_mode,
		a_uint32_t *index, union in_flow_tbl_u *entry);

sw_error_t
hppe_flow_ipv4_3tuple_del(
		a_uint32_t dev_id, a_uint32_t op_mode,
		a_uint32_t *index, union in_flow_3tuple_tbl_u *entry);

sw_error_t
hppe_flow_ipv6_5tuple_del(
		a_uint32_t dev_id, a_uint32_t op_mode,
		a_uint32_t *index, union in_flow_ipv6_5tuple_tbl_u *entry);

sw_error_t
hppe_flow_ipv6_3tuple_del(
		a_uint32_t dev_id, a_uint32_t op_mode,
		a_uint32_t *index, union in_flow_ipv6_3tuple_tbl_u *entry);

sw_error_t
hppe_flow_ipv4_5tuple_get(
		a_uint32_t dev_id, a_uint32_t op_mode,
		a_uint32_t *index, union in_flow_tbl_u *entry);

sw_error_t
hppe_flow_ipv4_3tuple_get(
		a_uint32_t dev_id, a_uint32_t op_mode,
		a_uint32_t *index, union in_flow_3tuple_tbl_u *entry);

sw_error_t
hppe_flow_ipv6_5tuple_get(
		a_uint32_t dev_id, a_uint32_t op_mode,
		a_uint32_t *index, union in_flow_ipv6_5tuple_tbl_u *entry);

sw_error_t
hppe_flow_ipv6_3tuple_get(
		a_uint32_t dev_id, a_uint32_t op_mode,
		a_uint32_t *index, union in_flow_ipv6_3tuple_tbl_u *entry);

sw_error_t
hppe_flow_host_flush_common(a_uint32_t dev_id);

sw_error_t
hppe_flow_entry_host_op_ipv4_5tuple_add(
		a_uint32_t dev_id, a_uint32_t op_mode,
		a_uint32_t *index, union in_flow_tbl_u *entry);


sw_error_t
hppe_flow_entry_host_op_ipv4_3tuple_add(
		a_uint32_t dev_id, a_uint32_t op_mode,
		a_uint32_t *index, union in_flow_3tuple_tbl_u *entry);


sw_error_t
hppe_flow_entry_host_op_ipv6_5tuple_add(
		a_uint32_t dev_id, a_uint32_t op_mode,
		a_uint32_t *index, union in_flow_ipv6_5tuple_tbl_u *entry);


sw_error_t
hppe_flow_entry_host_op_ipv6_3tuple_add(
		a_uint32_t dev_id, a_uint32_t op_mode,
		a_uint32_t *index, union in_flow_ipv6_3tuple_tbl_u *entry);


sw_error_t
hppe_flow_entry_host_op_ipv4_5tuple_del(
		a_uint32_t dev_id, a_uint32_t op_mode,
		a_uint32_t *index, union in_flow_tbl_u *entry);


sw_error_t
hppe_flow_entry_host_op_ipv4_3tuple_del(
		a_uint32_t dev_id, a_uint32_t op_mode,
		a_uint32_t *index, union in_flow_3tuple_tbl_u *entry);


sw_error_t
hppe_flow_entry_host_op_ipv6_5tuple_del(
		a_uint32_t dev_id, a_uint32_t op_mode,
		a_uint32_t *index, union in_flow_ipv6_5tuple_tbl_u *entry);


sw_error_t
hppe_flow_entry_host_op_ipv6_3tuple_del(
		a_uint32_t dev_id, a_uint32_t op_mode,
		a_uint32_t *index, union in_flow_ipv6_3tuple_tbl_u *entry);


sw_error_t
hppe_flow_entry_host_op_ipv4_5tuple_get(
		a_uint32_t dev_id, a_uint32_t op_mode,
		a_uint32_t *index, union in_flow_tbl_u *entry);


sw_error_t
hppe_flow_entry_host_op_ipv4_3tuple_get(
		a_uint32_t dev_id, a_uint32_t op_mode,
		a_uint32_t *index, union in_flow_3tuple_tbl_u *entry);


sw_error_t
hppe_flow_entry_host_op_ipv6_5tuple_get(
		a_uint32_t dev_id, a_uint32_t op_mode,
		a_uint32_t *index, union in_flow_ipv6_5tuple_tbl_u *entry);


sw_error_t
hppe_flow_entry_host_op_ipv6_3tuple_get(
		a_uint32_t dev_id, a_uint32_t op_mode,
		a_uint32_t *index, union in_flow_ipv6_3tuple_tbl_u *entry);


sw_error_t
hppe_flow_host_data_op_common(
		a_uint32_t dev_id,
		a_uint32_t op_type,
		a_uint32_t op_mode,
		a_uint32_t *index);

sw_error_t
hppe_flow_flush_common(a_uint32_t dev_id);

#if defined(MPPE)
sw_error_t
mppe_qos_mapping_tbl_flow_policer_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *policer_en,
		a_uint32_t *policer_index);

sw_error_t
mppe_qos_mapping_tbl_flow_policer_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t policer_en,
		a_uint32_t policer_index);
#endif

//from adpt_hppe_portvlan.c
sw_error_t
hppe_bridge_config_bridge_type_get(
		a_uint32_t dev_id,
		unsigned int *value);

sw_error_t
hppe_bridge_config_bridge_type_set(
		a_uint32_t dev_id,
		unsigned int value);

sw_error_t
appe_eg_vsi_vp_tag_tagged_mode_vp_bitmap_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
appe_eg_vsi_vp_tag_tagged_mode_vp_bitmap_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
hppe_eg_vsi_tag_tagged_mode_port_bitmap_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
hppe_eg_vsi_tag_tagged_mode_port_bitmap_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

//hppe_portctrl.h
#define PPE_CAPACITY_PORT_NUM	256

sw_error_t
appe_link_oam_ctrl_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		union link_oam_ctrl_u *value);

sw_error_t
appe_link_oam_ctrl_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		union link_oam_ctrl_u *value);

sw_error_t
appe_link_oam_ctrl_loopback_state_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
appe_link_oam_ctrl_loopback_state_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
mrppe_lpi_port_enable_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		union lpi_port_enable_u *value);

sw_error_t
mrppe_lpi_port_enable_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		union lpi_port_enable_u *value);

sw_error_t
mrppe_lpi_wakeup_timer_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		union lpi_port_wakeup_timer_u *value);

sw_error_t
mrppe_lpi_wakeup_timer_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		union lpi_port_wakeup_timer_u *value);

sw_error_t
mrppe_lpi_sleep_timer_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		union lpi_port_sleep_timer_u *value);

sw_error_t
mrppe_lpi_sleep_timer_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		union lpi_port_sleep_timer_u *value);

sw_error_t
mrppe_lpi_1us_cnt_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		union lpi_1us_cnt_u *value);

sw_error_t
mrppe_lpi_1us_cnt_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		union lpi_1us_cnt_u *value);

sw_error_t
ppe_mru_mtu_ctrl_tbl_source_filtering_mode_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
ppe_mru_mtu_ctrl_tbl_source_filtering_mode_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
ppe_mru_mtu_ctrl_tbl_source_filtering_bypass_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
ppe_mru_mtu_ctrl_tbl_source_filtering_bypass_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
hppe_mac_ctrl2_mac_tx_thd_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
hppe_mac_jumbo_size_mac_jumbo_size_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
hppe_mac_jumbo_size_mac_jumbo_size_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);
sw_error_t
hppe_drop_cnt_drop_cnt_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);
sw_error_t
hppe_drop_cnt_drop_cnt_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);
sw_error_t
hppe_rx_fifo_cfg_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		union rx_fifo_cfg_u *value);

sw_error_t
hppe_rx_fifo_cfg_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		union rx_fifo_cfg_u *value);
sw_error_t
hppe_tdm_ctrl_set(
		a_uint32_t dev_id,
		union tdm_ctrl_u *value);

sw_error_t
hppe_tdm_cfg_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		union tdm_cfg_u *value);
sw_error_t
hppe_port_fc_status_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		union port_fc_status_u *value);

sw_error_t
hppe_port_fc_status_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		union port_fc_status_u *value);

sw_error_t
hppe_drop_stat_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		union drop_stat_u *value);
sw_error_t
hppe_drop_stat_bytes_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint64_t *value);
sw_error_t
hppe_drop_stat_bytes_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint64_t value);
sw_error_t
hppe_drop_stat_pkts_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);
sw_error_t
hppe_drop_stat_pkts_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);
sw_error_t
hppe_ipr_pkt_num_tbl_reg_packets_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
hppe_ipr_byte_low_reg_reg_bytes_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);
sw_error_t
hppe_ipr_byte_high_reg_bytes_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);
//hppe_policer.h
#define CPU_CODE_CNT_TBL_MAX_ENTRY	256

//hppe_qm.h
#define PPE_CAPACITY_QUEUES_NUM	300
//hppe_ctrlpkt.h
sw_error_t
appe_l2_cpu_code_ctrl_exception_fmt_ctrl_en_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
appe_l2_cpu_code_ctrl_exception_fmt_ctrl_en_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

//appe_geneve.h
sw_error_t
appe_tpr_geneve_cfg_udp_port_map_get(
                a_uint32_t dev_id,
                unsigned int *value);

sw_error_t
appe_tpr_geneve_cfg_udp_port_map_set(
                a_uint32_t dev_id,
                unsigned int value);
//appe_global.h
sw_error_t
appe_port_mux_ctrl_get(
		a_uint32_t dev_id,
		union appe_port_mux_ctrl_u *value);

sw_error_t
appe_port_mux_ctrl_set(
		a_uint32_t dev_id,
		union appe_port_mux_ctrl_u *value);

sw_error_t
hmsppe_pon_mode_get(
		a_uint32_t dev_id,
		union hmsppe_pon_mode_u *value);
sw_error_t
hmsppe_pon_mode_set(
		a_uint32_t dev_id,
		union hmsppe_pon_mode_u *value);
//appe_portvlan.h
sw_error_t
appe_vp_isol_tbl_vp_profile_map_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint64_t *value);

sw_error_t
appe_vp_isol_tbl_vp_profile_map_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint64_t value);

//appe_vxlan.h
sw_error_t
appe_tpr_vxlan_cfg_udp_port_map_get(
		a_uint32_t dev_id,
		unsigned int *value);

sw_error_t
appe_tpr_vxlan_cfg_udp_port_map_set(
		a_uint32_t dev_id,
		unsigned int value);

sw_error_t
appe_tpr_vxlan_gpe_cfg_udp_port_map_get(
		a_uint32_t dev_id,
		unsigned int *value);

sw_error_t
appe_tpr_vxlan_gpe_cfg_udp_port_map_set(
		a_uint32_t dev_id,
		unsigned int value);
//appe_l2_vp.h
sw_error_t
appe_l2_vp_port_tbl_physical_port_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
appe_l2_vp_port_tbl_physical_port_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);
//appe_qos.h
sw_error_t
appe_qos_mapping_tbl_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		union qos_mapping_tbl_u *value);

sw_error_t
appe_qos_mapping_tbl_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		union qos_mapping_tbl_u *value);
//hppe_rss.h

sw_error_t
mrppe_toeplitz_secret_key_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		union toeplitz_secret_key_u *value);

sw_error_t
mrppe_toeplitz_secret_key_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		union toeplitz_secret_key_u *value);

sw_error_t
mrppe_rss_hash_type_sel_get(
		a_uint32_t dev_id,
		union rss_hash_type_sel_u *value);

sw_error_t
mrppe_rss_hash_type_sel_set(
		a_uint32_t dev_id,
		union rss_hash_type_sel_u *value);

sw_error_t
mrppe_toeplitz_hash_type_sel_0_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		union toeplitz_hash_type_sel_0_u *value);

sw_error_t
mrppe_toeplitz_hash_type_sel_0_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		union toeplitz_hash_type_sel_0_u *value);

sw_error_t
mrppe_toeplitz_hash_type_sel_1_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		union toeplitz_hash_type_sel_1_u *value);

sw_error_t
mrppe_toeplitz_hash_type_sel_1_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		union toeplitz_hash_type_sel_1_u *value);

sw_error_t
mrppe_toeplitz_hash_type_tuple_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		union toeplitz_hash_type_tuple_u *value);

sw_error_t
mrppe_toeplitz_hash_type_tuple_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		union toeplitz_hash_type_tuple_u *value);

sw_error_t
mrppe_rss_hash_type_sel_toeplitz_enable_get(
		a_uint32_t dev_id,
		unsigned int *value);

sw_error_t
mrppe_rss_hash_type_sel_toeplitz_enable_set(
		a_uint32_t dev_id,
		unsigned int value);

sw_error_t
mrppe_rss_hash_type_sel_extract_bit_pos_get(
		a_uint32_t dev_id,
		unsigned int *value);

sw_error_t
mrppe_rss_hash_type_sel_extract_bit_pos_set(
		a_uint32_t dev_id,
		unsigned int value);

sw_error_t
hppe_flow_entry_get(
		a_uint32_t dev_id, a_uint32_t op_mode,
		a_uint32_t *index, a_uint32_t *entry,
		a_uint32_t entry_size, a_bool_t flow_host);

sw_error_t
hppe_flow_entry_op(
		a_uint32_t dev_id,
		a_uint32_t op_type, a_uint32_t op_mode,
		a_uint32_t *index, a_uint32_t *entry,
		a_uint32_t entry_size, a_bool_t flow_host);

#if defined(JHPPE)
sw_error_t
jhppe_flow_entry_host_op_ip_6tuple_add(
		a_uint32_t dev_id, a_uint32_t op_mode,
		a_uint32_t *index, union in_flow_6tuple_tbl_u *entry);

sw_error_t
jhppe_flow_entry_host_op_ip_6tuple_del(
		a_uint32_t dev_id, a_uint32_t op_mode,
		a_uint32_t *index, union in_flow_6tuple_tbl_u *entry);

sw_error_t
jhppe_flow_entry_host_op_ip_6tuple_get(
		a_uint32_t dev_id, a_uint32_t op_mode,
		a_uint32_t *index, union in_flow_6tuple_tbl_u *entry);

sw_error_t
jhppe_flow_ip_6tuple_add(
		a_uint32_t dev_id, a_uint32_t op_mode,
		a_uint32_t *index, union in_flow_6tuple_tbl_u *entry);

sw_error_t
jhppe_flow_ip_6tuple_del(
		a_uint32_t dev_id, a_uint32_t op_mode,
		a_uint32_t *index, union in_flow_6tuple_tbl_u *entry);

sw_error_t
jhppe_flow_ip_6tuple_get(
		a_uint32_t dev_id, a_uint32_t op_mode,
		a_uint32_t *index, union in_flow_6tuple_tbl_u *entry);
#endif
#endif
