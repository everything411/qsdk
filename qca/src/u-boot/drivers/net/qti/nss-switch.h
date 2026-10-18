/*
 **************************************************************************
 * Copyright (c) 2016-2019, 2021, The Linux Foundation. All rights reserved.
 *
 * Copyright (c) 2022-2024, Qualcomm Innovation Center, Inc. All rights reserved.
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
 **************************************************************************
*/

#include <asm/global_data.h>
#include <asm/io.h>
#include <phy.h>
#include <net.h>
#include <miiphy.h>
#include <fdtdec.h>
#include <reset.h>
#include <dm/device_compat.h>
#include <linux/delay.h>
#include <asm-generic/gpio.h>
#include <clk.h>
#include <i2c.h>
#include <common.h>
#include <cpu_func.h>
#include <dm.h>
#include <errno.h>
#include <malloc.h>
#include <regmap.h>
#include <serial.h>
#include <syscon.h>
#include <asm/io.h>
#include <linux/err.h>
#include <linux/iopoll.h>
#include <dm/pinctrl.h>
#include <memalign.h>
#include <dt-bindings/net/qti-ipqsoc.h>

#define UPDATE_EDMA_CONFIG(_src, _dest)					\
	do {								\
		_dest->txdesc_ring_start = _src->txdesc_ring_start;	\
		_dest->txdesc_rings = _src->txdesc_rings;		\
		_dest->txdesc_ring_end = _src->txdesc_ring_end;	\
		_dest->txcmpl_ring_start = _src->txcmpl_ring_start;	\
		_dest->txcmpl_rings = _src->txcmpl_rings;		\
		_dest->txcmpl_ring_end = _src->txcmpl_ring_end;	\
		_dest->rxfill_ring_start = _src->rxfill_ring_start;	\
		_dest->rxfill_rings = _src->rxfill_rings;		\
		_dest->rxfill_ring_end = _src->rxfill_ring_end;	\
		_dest->rxdesc_ring_start = _src->rxdesc_ring_start;	\
		_dest->rxdesc_rings = _src->rxdesc_rings;		\
		_dest->rxdesc_ring_end = _src->rxdesc_ring_end;		\
		_dest->max_txcmpl_rings = _src->max_txcmpl_rings;	\
		_dest->max_txdesc_rings = _src->max_txdesc_rings;	\
		_dest->max_rxdesc_rings = _src->max_rxdesc_rings;	\
		_dest->max_rxfill_rings = _src->max_rxfill_rings;	\
		_dest->max_ports = _src->ports;	\
		_dest->start_ports = _src->start_ports;	\
	} while(0)

#define	WRITE_REG_ARRAY(_base, _offset, _size, _val, _count)		\
	do {								\
		int _i;							\
		for (_i = 0; _i < _count; ++_i)				\
			writel(_val, (_base + _offset + (_i * _size)));		\
	} while(0)

#define UPDATE_ACL_SET(_base, _var1, _var2, _var3, _var4, _var5, _var6,	\
			_var7, _var8, _var9)					\
			do {						\
				_base.reg_base = _var1;			\
				_base.rule_id = _var2;			\
				_base.rule_type = _var3;		\
				_base.field0 = _var4;			\
				_base.field1 = _var5;			\
				_base.mask = _var6;			\
				_base.permit = _var7;			\
				_base.deny = _var8;			\
				_base.ipo_cnt = _var9;			\
			}while(0)

#define LINK_STATUS				BIT(7)
#define DUPLEX					BIT(5)
#define SPEED					(BIT(0) | BIT(1) | BIT(2))

#define CLKOUT_50M_CTRL_OPTION			0x610

#define EDMA_SW_VER_1_ID			0x01
#define EDMA_SW_VER_2_ID			0x02

/* Number of descriptors in each ring is defined with below macro */
#ifdef CONFIG_ETH_LOW_MEM
#define EDMA_TX_RING_SIZE			32
#define EDMA_RX_RING_SIZE			32
#else
#define EDMA_TX_RING_SIZE			128
#define EDMA_RX_RING_SIZE			128
#endif
#define EDMA_TX_BUFF_SIZE			2048
#define EDMA_RX_BUFF_SIZE			2048

/* Number of byte in a descriptor is defined with below macros for each of
 * the rings respectively */
#define EDMA_TXDESC_DESC_SIZE	(sizeof(struct ipq_edma_txdesc_desc))
#define EDMA_TXCMPL_DESC_SIZE	(sizeof(struct ipq_edma_txcmpl_desc))
#define EDMA_RXDESC_DESC_SIZE	(sizeof(struct ipq_edma_rxdesc_desc))
#define EDMA_RXFILL_DESC_SIZE	(sizeof(struct ipq_edma_rxfill_desc))
#define EDMA_RX_SEC_DESC_SIZE	(sizeof(struct ipq_edma_rx_sec_desc))
#define EDMA_TX_SEC_DESC_SIZE	(sizeof(struct ipq_edma_tx_sec_desc))

#define EDMA_GET_DESC(R, i, type) (&(((type *)((R)->desc))[i]))
#define EDMA_RXFILL_DESC(R, i) 	EDMA_GET_DESC(R, i, struct ipq_edma_rxfill_desc)
#define EDMA_RXDESC_DESC(R, i) 	EDMA_GET_DESC(R, i, struct ipq_edma_rxdesc_desc)
#define EDMA_TXDESC_DESC(R, i) 	EDMA_GET_DESC(R, i, struct ipq_edma_txdesc_desc)
#define EDMA_TXCMPL_DESC(R, i) 	EDMA_GET_DESC(R, i, struct ipq_edma_txcmpl_desc)
/*
 * EDMA register
 */
#define EDMA_REG_MAS_CTRL		0x0
#define EDMA_REG_PORT_CTRL		0x4
#define EDMA_REG_RXDESC2FILL_MAP_0	0x14
#define EDMA_REG_RXDESC2FILL_MAP_1	0x18
#define EDMA_REG_RXDESC2FILL_MAP_2	0x1c
#define EDMA_REG_DMAR_CTRL		0x48
#define EDMA_REG_MISC_INT_STAT		0x5c
#define EDMA_REG_MISC_INT_MASK		0x60
#define EDMA_REG_TXDESC2CMPL_MAP_0	0x8c
#define EDMA_REG_TXDESC2CMPL_MAP_1	0x90
#define EDMA_REG_TXDESC2CMPL_MAP_2	0x94
#define EDMA_REG_TXDESC2CMPL_MAP_3	0x98
#define EDMA_REG_TXDESC2CMPL_MAP_4	0x9c
#define EDMA_REG_TXDESC2CMPL_MAP_5	0xa0

#define EDMA_REG_TXDESC_BA(n)		(0x1000 + (0x1000 * n))
#define EDMA_REG_TXDESC_PROD_IDX(n)	(0x1004 + (0x1000 * n))
#define EDMA_REG_TXDESC_CONS_IDX(n)	(0x1008 + (0x1000 * n))
#define EDMA_REG_TXDESC_RING_SIZE(n)	(0x100c + (0x1000 * n))
#define EDMA_REG_TXDESC_CTRL(n)		(0x1010 + (0x1000 * n))
#define EDMA_REG_TXDESC_BA2(n)		(0x1014 + (0x1000 * n))
#define EDMA_REG_TXDESC_BA_HIGH(n)	(0x1018 + (0x1000 * n))
#define EDMA_REG_TXDESC_BA2_HIGH(n)	(0x101c + (0x1000 * n))

#define EDMA_REG_RXFILL_BA(n)		(0x29000 + (0x1000 * n))
#define EDMA_REG_RXFILL_PROD_IDX(n)	(0x29004 + (0x1000 * n))
#define EDMA_REG_RXFILL_CONS_IDX(n)	(0x29008 + (0x1000 * n))
#define EDMA_REG_RXFILL_RING_SIZE(n)	(0x2900c + (0x1000 * n))
#define EDMA_REG_RXFILL_RING_SIZE_V2(n)	(0x29010 + (0x1000 * n))
#define EDMA_REG_RXFILL_RING_EN(n)	(0x2901c + (0x1000 * n))
#define EDMA_REG_RXFILL_INT_STAT(n)	(0x31000 + (0x1000 * n))
#define EDMA_REG_RXFILL_INT_MASK(n)	(0x31004 + (0x1000 * n))
#define EDMA_REG_RXFILL_BA_HIGH(n)	(0x29028 + (0x1000 * n))

#define EDMA_REG_RXDESC_BA(n)		(0x39000 + (0x1000 * n))
#define EDMA_REG_RXDESC_PROD_IDX(n)	(0x39004 + (0x1000 * n))
#define EDMA_REG_RXDESC_CONS_IDX(n)	(0x39008 + (0x1000 * n))
#define EDMA_REG_RXDESC_RING_SIZE(n)	(0x3900c + (0x1000 * n))
#define EDMA_REG_RXDESC_FC_THRE(n)	(0x39010 + (0x1000 * n))
#define EDMA_REG_RXDESC_CTRL(n)		(0x39018 + (0x1000 * n))
#define EDMA_REG_RXDESC_BA2(n)		(0x39028 + (0x1000 * n))
#define EDMA_REG_RXDESC_INT_STAT(n)	(0x59000 + (0x1000 * n))
#define EDMA_REG_RXDESC_INT_MASK(n)	(0x59004 + (0x1000 * n))
#define EDMA_REG_RX_INT_CTRL(n)		(0x5900c + (0x1000 * n))
#define EDMA_REG_RXDESC_BA_HIGH(n)	(0x3902c + (0x1000 * n))
#define EDMA_REG_RXDESC_BA2_HIGH(n)	(0x39030 + (0x1000 * n))

#define EDMA_REG_TXCMPL_BA(n)		(0x79000 + (0x1000 * n))
#define EDMA_REG_TXCMPL_PROD_IDX(n)	(0x79004 + (0x1000 * n))
#define EDMA_REG_TXCMPL_CONS_IDX(n)	(0x79008 + (0x1000 * n))
#define EDMA_REG_TXCMPL_RING_SIZE(n)	(0x7900c + (0x1000 * n))
#define EDMA_REG_TXCMPL_CTRL(n)		(0x79014 + (0x1000 * n))
#define EDMA_REG_TXCMPL_BA_HIGH(n)	(0x7901C + (0x1000 * n))

#define EDMA_REG_TX_INT_STAT(n)		(0x99000 + (0x1000 * n))
#define EDMA_REG_TX_INT_MASK(n)		(0x99004 + (0x1000 * n))
#define EDMA_REG_TX_INT_CTRL(n)		(0x9900c + (0x1000 * n))
/*
 * EDMA QID2RID configuration
 */
#define EDMA_QID2RID_TABLE_MEM(q)	(0xb9000 + (0x4 * q))

#define EDMA_CPU_PORT_MC_QID_MIN		256
#define EDMA_CPU_PORT_MC_QID_MAX		271
#define EDMA_QID2RID_NUM_PER_REG		4
/*
 * EDMA_REG_DMAR_CTRL register
 */
#define EDMA_DMAR_REQ_PRI_MASK			0x7
#define EDMA_DMAR_REQ_PRI_SHIFT			0x0
#define EDMA_DMAR_BURST_LEN_MASK		0x1
#define EDMA_DMAR_BURST_LEN_SHIFT		3
#define EDMA_DMAR_TXDATA_OUTSTANDING_NUM_MASK	0x1f
#define EDMA_DMAR_TXDATA_OUTSTANDING_NUM_SHIFT	4
#define EDMA_DMAR_TXDESC_OUTSTANDING_NUM_MASK	0x7
#define EDMA_DMAR_TXDESC_OUTSTANDING_NUM_SHIFT	9
#define EDMA_DMAR_RXFILL_OUTSTANDING_NUM_MASK	0x7
#define EDMA_DMAR_RXFILL_OUTSTANDING_NUM_SHIFT	12

#define EDMA_DMAR_REQ_PRI_SET(x) 				\
		(((x) & EDMA_DMAR_REQ_PRI_MASK) 		\
		<< EDMA_DMAR_REQ_PRI_SHIFT)
#define EDMA_DMAR_TXDATA_OUTSTANDING_NUM_SET(x) 		\
		(((x) & EDMA_DMAR_TXDATA_OUTSTANDING_NUM_MASK)	\
		<< EDMA_DMAR_TXDATA_OUTSTANDING_NUM_SHIFT)
#define EDMA_DMAR_TXDESC_OUTSTANDING_NUM_SET(x)			\
		(((x) & EDMA_DMAR_TXDESC_OUTSTANDING_NUM_MASK) 	\
		<< EDMA_DMAR_TXDESC_OUTSTANDING_NUM_SHIFT)
#define EDMA_DMAR_RXFILL_OUTSTANDING_NUM_SET(x)			\
		(((x) & EDMA_DMAR_RXFILL_OUTSTANDING_NUM_MASK) 	\
		<< EDMA_DMAR_RXFILL_OUTSTANDING_NUM_SHIFT)
#define EDMA_DMAR_BURST_LEN_SET(x)				\
		(((x) & EDMA_DMAR_BURST_LEN_MASK) 		\
		<< EDMA_DMAR_BURST_LEN_SHIFT)

#define EDMA_BURST_LEN_ENABLE			0x0

/*
 * EDMA_REG_PORT_CTRL register
 */
#define EDMA_PORT_CTRL_EN			0x3

/*
 * EDMA_REG_TXDESC_PROD_IDX register
 */
#define EDMA_TXDESC_PROD_IDX_MASK		0xffff

/*
 * EDMA_REG_TXDESC_CONS_IDX register
 */
#define EDMA_TXDESC_CONS_IDX_MASK		0xffff

/*
 * EDMA_REG_TXDESC_RING_SIZE register
 */
#define EDMA_TXDESC_RING_SIZE_MASK		0xffff

/*
 * EDMA_REG_TXDESC_CTRL register
 */
#define EDMA_TXDESC_TX_EN			0x1

#define EDMA_TXDESC_BUF_HI_ADD_MASK		0xFF
/*
 * EDMA_REG_TXCMPL_PROD_IDX register
 */
#define EDMA_TXCMPL_PROD_IDX_MASK		0xffff

/*
 * EDMA_REG_TXCMPL_CONS_IDX register
 */
#define EDMA_TXCMPL_CONS_IDX_MASK		0xffff

/*
 * EDMA_REG_TX_INT_CTRL register
 */
#define EDMA_TX_INT_MASK			0x3

/*
 * EDMA_REG_RXFILL_PROD_IDX register
 */
#define EDMA_RXFILL_PROD_IDX_MASK		0xffff

/*
 * EDMA_REG_RXFILL_CONS_IDX register
 */
#define EDMA_RXFILL_CONS_IDX_MASK		0xffff

/*
 * EDMA_REG_RXFILL_RING_SIZE register
 */
#define EDMA_RXFILL_RING_SIZE_MASK		0xffff
#define EDMA_RXFILL_BUF_SIZE_MASK		0xffff0000
#define EDMA_RXFILL_BUF_SIZE_SHIFT		16

/*
 * EDMA_REG_RXFILL_RING_EN register
 */
#define EDMA_RXFILL_RING_EN			0x1

/*
 * EDMA_REG_RXFILL_INT_MASK register
 */
#define EDMA_RXFILL_INT_MASK			0x1

#define EDMA_RXFILL_BUF_HI_ADD_MASK		0xFF

/*
 * EDMA_REG_RXDESC_PROD_IDX register
 */
#define EDMA_RXDESC_PROD_IDX_MASK		0xffff

/*
 * EDMA_REG_RXDESC_CONS_IDX register
 */
#define EDMA_RXDESC_CONS_IDX_MASK		0xffff

/*
 * EDMA_REG_RXDESC_RING_SIZE register
 */
#define EDMA_RXDESC_RING_SIZE_MASK		0xffff
#define EDMA_RXDESC_PL_OFFSET_MASK		0x1ff
#define EDMA_RXDESC_PL_OFFSET_SHIFT		16
#define EDMA_RXDESC_PL_OFFSET_SHIFT_V2		23

/*
 * EDMA_REG_RXDESC_CTRL register
 */
#define EDMA_RXDESC_RX_EN			0x1

/*
 * EDMA_REG_TX_INT_MASK register
 */
#define EDMA_TX_INT_MASK_PKT_INT		0x1
#define EDMA_TX_INT_MASK_UGT_INT		0x2

/*
 * EDMA_REG_RXDESC_INT_MASK register
 */
#define EDMA_RXDESC_INT_MASK_PKT_INT		0x1
#define EDMA_MASK_INT_DISABLE			0x0

/*
 * TXDESC shift values
 */
#define EDMA_TXDESC_DATA_OFFSET_SHIFT		0
#define EDMA_TXDESC_DATA_OFFSET_MASK		0xfff

#define EDMA_TXDESC_DATA_LENGTH_SHIFT		0
#define EDMA_TXDESC_DATA_LENGTH_MASK		0x1ffff

#define EDMA_DST_PORT_TYPE			2
#define EDMA_DST_PORT_TYPE_SHIFT		28
#define EDMA_DST_PORT_TYPE_MASK					\
			(0xf << EDMA_DST_PORT_TYPE_SHIFT)
#define EDMA_DST_PORT_ID_SHIFT			16
#define EDMA_DST_PORT_ID_MASK					\
			(0xfff << EDMA_DST_PORT_ID_SHIFT)

#define EDMA_DST_PORT_TYPE_SET(x)				\
			(((x) << EDMA_DST_PORT_TYPE_SHIFT) &	\
			EDMA_DST_PORT_TYPE_MASK)
#define EDMA_DST_PORT_ID_SET(x)					\
			(((x) << EDMA_DST_PORT_ID_SHIFT) &	\
			EDMA_DST_PORT_ID_MASK)

#define EDMA_RXDESC_SRCINFO_TYPE_PORTID		0x2000
#define EDMA_RXDESC_SRCINFO_TYPE_SHIFT		8
#define EDMA_RXDESC_SRCINFO_TYPE_MASK		0xf000
#define EDMA_RXDESC_PORTNUM_BITS		0x0FFF

#define EDMA_RING_DMA_MASK			0xffffffff

/*
 * RXDESC shift values
 */
#define EDMA_RXDESC_PKT_SIZE_MASK		0x3ffff
#define EDMA_RXDESC_PKT_SIZE_SHIFT		0
#define EDMA_RXDESC_SRC_INFO_GET(x)		(x & 0xFFFF)
#define EDMA_RXDESC_RING_INT_STATUS_MASK	0x3
#define EDMA_RXFILL_RING_INT_STATUS_MASK	0x1

#define EDMA_TXCMPL_RING_INT_STATUS_MASK	0x3
#define EDMA_TXCMPL_RETMODE_OPAQUE		0x0
#define EDMA_TX_NE_INT_EN			0x2
#define EDMA_RX_NE_INT_EN			0x2
#define EDMA_TX_INITIAL_PROD_IDX		0x0

#define EDMA_MISC_INTR_MASK			0xFF
#define EDMA_RX_PAYLOAD_OFFSET			0x0

/*
 * PPE register
 */
#define PORT5_MUX_PCS_UNIPHY0			0x0
#define PORT5_MUX_PCS_UNIPHY1			0x1

#define PORT_MUX_MAC_TYPE			0
#define PORT_MUX_XMAC_TYPE			1

#define ADPT_ACL_HPPE_IPV4_DIP_RULE		4
#define ADPT_ACL_HPPE_MAC_SA_RULE		1
#define ADPT_ACL_HPPE_MAC_DA_RULE		0
#define MAX_RULE				512

#define PORT_MUX_CTRL				0x10
#define PORT_MUX_CTRL_NUM			1
#define PORT_MUX_CTRL_INC			0x4
#define PORT_MUX_CTRL_DEFAULT			0x0

#define PORT_PHY_STATUS_ADDRESS			0x40
#define PORT_PHY_STATUS_ADDRESS1		0x44

#define PORT_PHY_STATUS_PORT2_OFFSET		8
#define PORT_PHY_STATUS_PORT3_OFFSET		16
#define PORT_PHY_STATUS_PORT4_OFFSET		24
#define PORT_PHY_STATUS_PORT5_1_OFFSET		8
#define PORT_PHY_STATUS_PORT6_OFFSET		16

#define PPE_IPE_L3_BASE_ADDR			0x200000
#define PPE_L3_VP_PORT_TBL_ADDR			(PPE_IPE_L3_BASE_ADDR + 0x4000)
#define PPE_L3_VP_PORT_TBL_INC			0x10

#define PPE_TL_PORT_VP_TBL_ADDR			0x302000
#define PPE_MRU_MTU_CTRL_TBL_ADDR		0x65000
#define PPE_MC_MTU_CTRL_TBL_ADDR		0x60a00
#define PPE_PORT_EG_VLAN_TBL_ADDR		0x20020

#define PPE_UCAST_QUEUE_AC_EN_BASE_ADDR 	0x848000
#define PPE_MCAST_QUEUE_AC_EN_BASE_ADDR 	0x84a000
#define PPE_QUEUE_MANAGER_BASE_ADDR		0x800000
#define PPE_UCAST_QUEUE_MAP_TBL_ADDR		0x10000
#define PPE_UCAST_QUEUE_MAP_TBL_INC		0x10
#define PPE_QM_UQM_TBL			(PPE_QUEUE_MANAGER_BASE_ADDR +\
					 PPE_UCAST_QUEUE_MAP_TBL_ADDR)
#define PPE_UCAST_PRIORITY_MAP_TBL_ADDR		0x42000
#define PPE_QM_UPM_TBL			(PPE_QUEUE_MANAGER_BASE_ADDR +\
					 PPE_UCAST_PRIORITY_MAP_TBL_ADDR)

#define PPE_STP_BASE				0x060100
#define PPE_MAC_ENABLE				0x001000
#define PPE_MAC_SPEED_OFF			0x4
#define PPE_MAC_MIB_CTL_OFF			0x34


#define PPE_TRAFFIC_MANAGER_BASE_ADDR		0x400000
#define PPE_TM_SHP_CFG_L0_OFFSET		0x00000030
#define PPE_TM_SHP_CFG_L1_OFFSET		0x00000034
#define PPE_TM_SHP_CFG_L0		PPE_TRAFFIC_MANAGER_BASE_ADDR +\
						PPE_TM_SHP_CFG_L0_OFFSET
#define PPE_TM_SHP_CFG_L1		PPE_TRAFFIC_MANAGER_BASE_ADDR +\
						PPE_TM_SHP_CFG_L1_OFFSET

#define PPE_L0_FLOW_PORT_MAP_TBL_ADDR		0x10000
#define PPE_L0_FLOW_PORT_MAP_TBL_INC		0x10
#define PPE_L0_FLOW_PORT_MAP_TBL	(PPE_TRAFFIC_MANAGER_BASE_ADDR +\
					 PPE_L0_FLOW_PORT_MAP_TBL_ADDR)

#define PPE_L0_FLOW_MAP_TBL_ADDR		0x2000
#define PPE_L0_FLOW_MAP_TBL_INC			0x10
#define PPE_L0_FLOW_MAP_TBL		(PPE_TRAFFIC_MANAGER_BASE_ADDR +\
					 PPE_L0_FLOW_MAP_TBL_ADDR)

#define PPE_L1_FLOW_PORT_MAP_TBL_ADDR		0x46000
#define PPE_L1_FLOW_PORT_MAP_TBL_INC		0x10
#define PPE_L1_FLOW_PORT_MAP_TBL	(PPE_TRAFFIC_MANAGER_BASE_ADDR +\
					 PPE_L1_FLOW_PORT_MAP_TBL_ADDR)

#define PPE_L1_FLOW_MAP_TBL_ADDR		0x40000
#define PPE_L1_FLOW_MAP_TBL_INC			0x10
#define PPE_L1_FLOW_MAP_TBL		(PPE_TRAFFIC_MANAGER_BASE_ADDR +\
					 PPE_L1_FLOW_MAP_TBL_ADDR)

#define PPE_L0_C_SP_CFG_TBL_ADDR		0x4000
#define PPE_L0_C_SP_CFG_TBL		(PPE_TRAFFIC_MANAGER_BASE_ADDR +\
					 PPE_L0_C_SP_CFG_TBL_ADDR)

#define PPE_L1_C_SP_CFG_TBL_ADDR		0x42000
#define PPE_L1_C_SP_CFG_TBL		(PPE_TRAFFIC_MANAGER_BASE_ADDR +\
					 PPE_L1_C_SP_CFG_TBL_ADDR)

#define PPE_L0_E_SP_CFG_TBL_ADDR		0x6000
#define PPE_L0_E_SP_CFG_TBL		(PPE_TRAFFIC_MANAGER_BASE_ADDR +\
					 PPE_L0_E_SP_CFG_TBL_ADDR)

#define PPE_L1_E_SP_CFG_TBL_ADDR		0x44000
#define PPE_L1_E_SP_CFG_TBL		(PPE_TRAFFIC_MANAGER_BASE_ADDR +\
					 PPE_L1_E_SP_CFG_TBL_ADDR)

#define PPE_FPGA_GPIO_BASE_ADDR			0x01008000

#define PPE_MAC_PORT_MUX_OFFSET			0x10
#define PPE_FPGA_GPIO_OFFSET			0xc000
#define PPE_FPGA_SCHED_OFFSET			0x47a000
#define PPE_TDM_CFG_DEPTH_OFFSET		0xb000
#define PPE_TDM_SCHED_DEPTH_OFFSET		0x400000
#define PPE_PORT_BRIDGE_CTRL_OFFSET		0x060300

#define PPE_TDM_CFG_DEPTH_VAL			0x80000064
#define PPE_MAC_PORT_MUX_OFFSET_VAL		0x15
#define PPE_TDM_SCHED_DEPTH_VAL			0x32
#define PPE_TDM_CFG_VALID			0x20
#define PPE_TDM_CFG_DIR_INGRESS			0x0
#define PPE_TDM_CFG_DIR_EGRESS			0x10
#define PPE_PORT_EDMA				0x0
#define PPE_PORT_QTI1				0x1
#define PPE_PORT_QTI2				0x2
#define PPE_PORT_QTI3				0x3
#define PPE_PORT_QTI4				0x4
#define PPE_PORT_XGMAC1				0x5
#define PPE_PORT_XGMAC2				0x6
#define PPE_PORT_CRYPTO1			0x7
#define PPE_PORT_BRIDGE_CTRL_PROMISC_EN		0x20000
#define PPE_PORT_BRIDGE_CTRL_TXMAC_EN		0x10000
#define PPE_PORT_BRIDGE_CTRL_PORT_ISOLATION_BMP	0x7f00
#define PPE_PORT_BRIDGE_CTRL_STATION_LRN_EN	0x8
#define PPE_PORT_BRIDGE_CTRL_NEW_ADDR_LRN_EN	0x1

#define PPE_PORT_EDMA_BITPOS		0x1
#define PPE_PORT_QTI1_BITPOS		(1 << PPE_PORT_QTI1)
#define PPE_PORT_QTI2_BITPOS		(1 << PPE_PORT_QTI2)
#define PPE_PORT_QTI3_BITPOS		(1 << PPE_PORT_QTI3)
#define PPE_PORT_QTI4_BITPOS		(1 << PPE_PORT_QTI4)
#define PPE_PORT_XGMAC1_BITPOS		(1 << PPE_PORT_XGMAC1)
#define PPE_PORT_XGMAC2_BITPOS		(1 << PPE_PORT_XGMAC2)
#define PPE_PORT_CRYPTO1_BITPOS		(1 << PPE_PORT_CRYPTO1)

#define PPE_SWITCH_NSS_SWITCH_XGMAC0		0x500000
#define NSS_SWITCH_XGMAC_MAC_TX_CONFIGURATION	0x4000
#define USS					(1 << 31)
#define SS(i)					(i << 29)
#define JD					(1 << 16)
#define TE					(1 << 0)
#define NSS_SWITCH_XGMAC_MAC_RX_CONFIGURATION	0x4000
#define MAC_RX_CONFIGURATION_ADDRESS		0x4
#define RE					(1 << 0)
#define ACS					(1 << 1)
#define CST					(1 << 2)
#define MAC_PACKET_FILTER_INC			0x4000
#define MAC_PACKET_FILTER_ADDRESS 		0x8

#define XGMAC_SPEED_SELECT_10000M 		0
#define XGMAC_SPEED_SELECT_5000M 		1
#define XGMAC_SPEED_SELECT_2500M 		2
#define XGMAC_SPEED_SELECT_1000M 		3

#define IPE_L2_BASE_ADDR			0x060000
#define PORT_BRIDGE_CTRL_ADDRESS		0x300
#define PORT_BRIDGE_CTRL_INC			0x4
#define TX_MAC_EN				(1 << 16)

#define IPO_CSR_BASE_ADDR			0x0b0000

#define IPO_RULE_REG_ADDRESS			0x0
#define IPO_RULE_REG_INC			0x10

#define IPO_MASK_REG_ADDRESS			0x2000
#define IPO_MASK_REG_INC			0x10

#define IPO_ACTION_ADDRESS			0x8000
#define IPO_ACTION_INC				0x20
/*
 * Uniphy register
 */
#define GCC_UNIPHY_REG_INC 			0x10

#define PPE_UNIPHY_OFFSET_CALIB_4		0x1E0
#define UNIPHY_CALIBRATION_DONE			0x1

#define PPE_UNIPHY_REG_INC 			0
#define PPE_UNIPHY_MODE_CONTROL			0x46C
#define UNIPHY_XPCS_MODE			(1 << 12)
#define UNIPHY_SG_PLUS_MODE			(1 << 11)
#define UNIPHY_SG_MODE				(1 << 10)
#define UNIPHY_CH0_PSGMII_QSGMII		(1 << 9)
#define UNIPHY_CH0_QSGMII_SGMII			(1 << 8)
#define UNIPHY_CH4_CH1_0_SGMII			(1 << 2)
#define UNIPHY_CH1_CH0_SGMII			(1 << 1)
#define UNIPHY_CH0_ATHR_CSCO_MODE_25M		(1 << 0)

#define UNIPHY_INSTANCE_LINK_DETECT		0x570

#define UNIPHY_MISC2_REG_OFFSET 		0x218
#define UNIPHY_MISC2_REG_SGMII_MODE 		0x30
#define UNIPHY_MISC2_REG_SGMII_PLUS_MODE 	0x50

#define UNIPHY_MISC2_REG_VALUE			0x70

#define UNIPHY_MISC_SOURCE_SELECTION_REG_OFFSET	0x21c
#define UNIPHY_MISC_SRC_PHY_MODE		0xa882

#define UNIPHY_DEC_CHANNEL_0_INPUT_OUTPUT_4	0x480
#define UNIPHY_FORCE_SPEED_25M			(1 << 3)

#define UNIPHY1_CLKOUT_50M_CTRL_OPTION		0x610
#define UNIPHY1_CLKOUT_50M_CTRL_CLK50M_DIV2_SEL	(1 << 5)
#define UNIPHY1_CLKOUT_50M_CTRL_50M_25M_EN	0x1

#define UNIPHY_PLL_RESET_REG_OFFSET 		0x780
#define UNIPHY_PLL_RESET_REG_VALUE 		0x02bf
#define UNIPHY_PLL_RESET_REG_DEFAULT_VALUE 	0x02ff

#define SR_XS_PCS_KR_STS1_ADDRESS 		0x30020
#define UNIPHY_10GR_LINKUP 			0x1

#define VR_XS_PCS_DIG_CTRL1_ADDRESS 		0x38000
#define VR_XS_PCS_EEE_MCTRL0_ADDRESS            0x38006
#define VR_XS_PCS_KR_CTRL_ADDRESS               0x38007
#define VR_XS_PCS_EEE_TXTIMER_ADDRESS           0x38008
#define VR_XS_PCS_EEE_RXTIMER_ADDRESS           0x38009
#define VR_XS_PCS_EEE_MCTRL1_ADDRESS            0x3800b
#define VR_XS_PCS_DIG_STS_ADDRESS               0x3800a

#define USXG_EN					(1 << 9)
#define USRA_RST				(1 << 10)
#define USXG_MODE                               (5 << 10)
#define VR_RST                                  (1 << 15)
#define AM_COUNT                                (0x6018 << 0)

#define SR_MII_CTRL_CHANNEL1_ADDRESS            0x1a0000
#define SR_MII_CTRL_CHANNEL2_ADDRESS            0x1b0000
#define SR_MII_CTRL_CHANNEL3_ADDRESS            0x1c0000
#define SR_MII_CTRL_ADDRESS 			0x1f0000

#define VR_MII_AN_CTRL_CHANNEL1_ADDRESS         0x1a8001
#define VR_MII_AN_CTRL_CHANNEL2_ADDRESS         0x1b8001
#define VR_MII_AN_CTRL_CHANNEL3_ADDRESS         0x1c8001
#define VR_MII_AN_CTRL_ADDRESS			0x1f8001
#define MII_AN_INTR_EN				(1 << 0)
#define MII_CTRL				(1 << 8)

#define SR_MII_CTRL_ADDRESS 			0x1f0000
#define AN_ENABLE				(1 << 12)
#define SS5					(1 << 5)
#define SS6					(1 << 6)
#define SS13					(1 << 13)
#define DUPLEX_MODE				(1 << 8)

#define VR_MII_AN_INTR_STS			0x1f8002
#define CL37_ANCMPLT_INTR			(1 << 0)

#define UNIPHYQP_USXG_OPITON1                   0x584
#define GMII_SRC_SEL                            (1 << 0)

#define VR_XAUI_MODE_CTRL_CHANNEL1_ADDRESS      0x1a8004
#define VR_XAUI_MODE_CTRL_CHANNEL2_ADDRESS      0x1b8004
#define VR_XAUI_MODE_CTRL_CHANNEL3_ADDRESS      0x1c8004
#define VR_XAUI_MODE_CTRL_ADDRESS               0x1f8004
#define IPG_CHECK                               0x1

#define UNIPHY_XPCS_TSL_TIMER                   (0xa << 0)
#define SIGN_BIT                                (1 << 6)
#define MULT_FACT_100NS                         (1 << 8)
#define UNIPHY_XPCS_TLU_TIMER                   (0x3 << 6)
#define UNIPHY_XPCS_TWL_TIMER                   (0x16 << 8)
#define UNIPHY_XPCS_100US_TIMER                 (0xc8 << 0)
#define UNIPHY_XPCS_TWR_TIMER                   (0x1c << 8)
#define LRX_EN                                  (1 << 0)
#define LTX_EN                                  (1 << 1)
#define TRN_LPI                                 (1 << 0)
#define TRN_RXLPI                               (1 << 8)

enum uniphy_reset_type {
	UNIPHY0_SOFT_RESET = 0,
	UNIPHY0_XPCS_RESET,
	UNIPHY1_SOFT_RESET,
	UNIPHY1_XPCS_RESET,
	UNIPHY2_SOFT_RESET,
	UNIPHY2_XPCS_RESET,
	UNIPHY_RST_MAX
};

enum {
	GMAC = 0,
	XGMAC,
};

enum ipq_edma_tx {
	EDMA_TX_OK = 0,			/* Tx success */
	EDMA_TX_DESC = 1,		/* Not enough descriptors */
	EDMA_TX_FAIL = 2,		/* Tx failure */
};

enum port_wrapper_cfg {
        PORT_WRAPPER_PSGMII = 0,
        PORT_WRAPPER_SGMII0_RGMII4 = 1,
        PORT_WRAPPER_USXGMII = 2,
        PORT_WRAPPER_SGMII1_RGMII4 = 3,
        PORT_WRAPPER_SGMII4_RGMII4 = 4,
        PORT_WRAPPER_QSGMII= 5,
        PORT_WRAPPER_SGMII_PLUS = 6,
        PORT_WRAPPER_10GBASE_R = 7,
        PORT_WRAPPER_SGMII_FIBER = 8,
	PORT_WRAPPER_UQXGMII = 9,
	PORT_WRAPPER_EMULATION = 10,
};

enum {
	TCP_PKT,
	UDP_PKT,
};

/*
 * RxDesc descriptor
 */
struct ipq_edma_rxdesc_desc {
	uint32_t rdes0; /* Contains lower 32-bit of buffer address */
	uint32_t rdes1;
	/*
	 * v1: Contains more bit, priority bit, service code
	 * v2: Contains more bit, priority bit, service code & higher 8-bit of
	 * buffer address.
	 */
	uint32_t rdes2; /* Contains opaque */
	uint32_t rdes3; /* Contains opaque high bits */
	uint32_t rdes4; /* Contains destination and source information */
	uint32_t rdes5; /* Contains WiFi QoS, data length */
	uint32_t rdes6; /* Contains hash value, check sum status */
	uint32_t rdes7; /* Contains DSCP, packet offsets */
};

/*
 * EDMA Rx Secondary Descriptor
 */
struct ipq_edma_rx_sec_desc {
	uint32_t rx_sec0; /* Contains timestamp */
	uint32_t rx_sec1; /* Contains secondary checksum status */
	uint32_t rx_sec2; /* Contains QoS tag */
	uint32_t rx_sec3; /* Contains flow index details */
	uint32_t rx_sec4; /* Contains secondary packet offsets */
	uint32_t rx_sec5; /* Contains multicast bit, checksum */
	uint32_t rx_sec6; /* Contains SVLAN, CVLAN */
	uint32_t rx_sec7; /* Contains secondary SVLAN, CVLAN */
};

/*
 * RxFill descriptor
 */
struct ipq_edma_rxfill_desc {
	uint32_t rdes0; /* Contains Lower 32-bit of buffer address */
	uint32_t rdes1;
	/*
	 * v1: Contains buffer size
	 * v2: Contains buffer & higher 8-bit of buffer address.
	 */
	uint32_t rdes2; /* Contains opaque */
	uint32_t rdes3; /* Contains opaque high bits */
};

/*
 * TxDesc descriptor
 */
struct ipq_edma_txdesc_desc {
	uint32_t tdes0; /* Lower 32-bit of buffer address */
	uint32_t tdes1;
	/*
	 * v1: Buffer recycling, PTP tag flag, PRI valid flag
	 * v2: Buffer recycling, PTP tag flag, PRI valid flag & higher 8-bit
	 * of buffer address
	 */
	uint32_t tdes2; /* Low 32-bit of opaque value */
	uint32_t tdes3; /* High 32-bit of opaque value */
	uint32_t tdes4; /* Source/Destination port info */
	uint32_t tdes5; /* VLAN offload, csum_mode, ip_csum_en,
				tso_en, data length */
	uint32_t tdes6; /* MSS/hash_value/PTP tag, data offset */
	uint32_t tdes7; /* L4/L3 offset, PROT type, L2 type,
				CVLAN/SVLAN tag, service code */
};

/*
 * EDMA Tx Secondary Descriptor
 */
struct ipq_edma_tx_sec_desc {
	uint32_t tx_sec0; /* Reserved */
	uint32_t tx_sec1; /* Custom csum offset, payload offset,
				TTL/NAT action */
	uint32_t rx_sec2; /* NAPT translated port, DSCP value, TTL value */
	uint32_t rx_sec3; /* Flow index value and valid flag */
	uint32_t rx_sec4; /* Reserved */
	uint32_t rx_sec5; /* Reserved */
	uint32_t rx_sec6; /* CVLAN/SVLAN command */
	uint32_t rx_sec7; /* CVLAN/SVLAN tag value */
};

/*
 * TxCmpl descriptor
 */
struct ipq_edma_txcmpl_desc {
	uint32_t tdes0; /* Low 32-bit opaque value */
	uint32_t tdes1; /* High 32-bit opaque value */
	uint32_t tdes2; /* More fragment, transmit ring id, pool id */
	uint32_t tdes3; /* Error indications */
};

/*
 * Tx descriptor ring
 */
struct ipq_edma_txdesc_ring {
	uint32_t prod_idx;		/* Producer index */
	uint32_t avail_desc;		/* Number of available descriptor
						to process */
	uint32_t id;			/* TXDESC ring number */
	struct ipq_edma_txdesc_desc *desc;
					/* descriptor ring virtual address */
	dma_addr_t dma;			/* descriptor ring physical address */
	struct ipq_edma_tx_sec_desc *sdesc;
					/* Secondary descriptor ring
						virtual addr */
	dma_addr_t sdma;		/* Secondary descriptor ring
						physical address */
	uint16_t count;			/* number of descriptors */
};

/*
 * TxCmpl ring
 */
struct ipq_edma_txcmpl_ring {
	uint32_t cons_idx;		/* Consumer index */
	uint32_t avail_pkt;		/* Number of available packets
						to process */
	struct ipq_edma_txcmpl_desc *desc;
					/* descriptor ring virtual address */
	uint32_t id;			/* TXCMPL ring number */
	dma_addr_t dma;			/* descriptor ring physical address */
	uint32_t count;			/* Number of descriptors in the ring */
};

/*
 * RxFill ring
 */
struct ipq_edma_rxfill_ring {
	uint32_t id;			/* RXFILL ring number */
	uint32_t count;			/* number of descriptors in the ring */
	uint32_t prod_idx;		/* Ring producer index */
	struct ipq_edma_rxfill_desc *desc;
					/* descriptor ring virtual address */
	dma_addr_t dma;			/* descriptor ring physical address */
};

/*
 * RxDesc ring
 */
struct ipq_edma_rxdesc_ring {
	uint32_t id;			/* RXDESC ring number */
	uint32_t count;			/* number of descriptors in the ring */
	uint32_t cons_idx;		/* Ring consumer index */
	struct ipq_edma_rxdesc_desc *desc;
					/* Primary descriptor ring
							virtual addr */
	struct ipq_edma_sec_rxdesc_ring *sdesc;
					/* Secondary desc ring VA */
	struct ipq_edma_rxfill_ring *rxfill;
					/* RXFILL ring used */
	dma_addr_t dma;			/* Primary descriptor ring
							physical address */
	dma_addr_t sdma;		/* Secondary descriptor ring
							physical address */
};

struct port_mux_ctrl {
        uint32_t port1_pcs_sel:1;
        uint32_t port2_pcs_sel:1;
        uint32_t port3_pcs_sel:1;
        uint32_t port4_pcs_sel:1;
        uint32_t port5_pcs_sel:1;
        uint32_t port6_pcs_sel:1;
        uint32_t _reserved0:2;
        uint32_t port1_mac_sel:1;
        uint32_t port2_mac_sel:1;
        uint32_t port3_mac_sel:1;
        uint32_t port4_mac_sel:1;
        uint32_t port5_mac_sel:1;
        uint32_t port6_mac_sel:1;
        uint32_t _reserved1:18;
};

union port_mux_ctrl_u {
        uint32_t val;
        struct port_mux_ctrl bf;
};

struct ipo_rule_reg {
        uint32_t  rule_field_0:32;
        uint32_t  rule_field_1:20;
        uint32_t  fake_mac_header:1;
        uint32_t  range_en:1;
        uint32_t  inverse_en:1;
        uint32_t  rule_type:5;
        uint32_t  src_type:3;
        uint32_t  src_0:1;
        uint32_t  src_1:7;
        uint32_t  pri:9;
        uint32_t  res_chain:1;
        uint32_t  post_routing_en:1;
        uint32_t  _reserved0:14;
};

union ipo_rule_reg_u {
        uint32_t val[3];
        struct ipo_rule_reg bf;
};

struct ipo_mask_reg {
        uint32_t  maskfield_0:32;
        uint32_t  maskfield_1:21;
        uint32_t  _reserved0:11;
};

union ipo_mask_reg_u {
        uint32_t val[2];
        struct ipo_mask_reg bf;
};

struct ipo_action {
        uint32_t  dest_info_change_en:1;
	uint32_t  fwd_cmd:2;
	uint32_t  _reserved0:15;
	uint32_t bypass_bitmap_0:14;
	uint32_t bypass_bitmap_1:18;
	uint32_t  _reserved1:14;
	uint32_t  _reserved2:32;
	uint32_t  _reserved3:32;
	uint32_t  _reserved4:32;
	uint32_t  _reserved5:32;
};

union ipo_action_u {
        uint32_t val[6];
        struct ipo_action bf;
};

struct ppe_acl_set {
	phys_addr_t reg_base;
	uint32_t rule_id;
	uint32_t rule_type;
	uint32_t field0;
	uint32_t field1;
	uint32_t mask;
	uint32_t permit;
	uint32_t deny;
	uint32_t ipo_cnt;
};


struct ipq_eth_port_config {
	uint8_t id;
	uint32_t clk_rate[6];
	uint8_t mac_mode[6];
	uint8_t mode[6];
}__attribute__ ((aligned(8)));

struct ipq_tdm_config {
	uint8_t val[128];
}__attribute__ ((aligned(8)));

struct ipq_eth_sku {
	phys_addr_t reg;
	uint8_t bit;
}__attribute__ ((aligned(8)));

extern struct ipq_tdm_config *tdm_config;
extern struct ipq_eth_port_config *port_config;
extern struct ipq_eth_sku *ipq_uniphy;
extern uint32_t nb_vsi_config[CONFIG_ETH_MAX_MAC];

struct edma_config {
	struct ipq_eth_port_config *pconfig;
	uint32_t tdm_ctrl_val;
	uint32_t sw_version;
	uint8_t txdesc_ring_start;
	uint8_t txdesc_rings;
	uint8_t txdesc_ring_end;
	uint8_t txcmpl_ring_start;
	uint8_t txcmpl_rings;
	uint8_t txcmpl_ring_end;
	uint8_t rxfill_ring_start;;
	uint8_t rxfill_rings;
	uint8_t rxfill_ring_end;
	uint8_t rxdesc_ring_start;
	uint8_t rxdesc_rings;
	uint8_t rxdesc_ring_end;
	uint8_t max_txcmpl_rings;
	uint8_t max_txdesc_rings;
	uint8_t max_rxdesc_rings;
	uint8_t max_rxfill_rings;
	uint8_t iports;
	uint8_t ports;
	uint8_t start_ports;
	uint8_t vsi;
	uint8_t tx_map;
	uint8_t rx_map;
	uint8_t ipo_action;
	bool  hw_reset;
};

extern struct edma_config ipq_edma_config;

/* edma hw specific data */
struct ipq_edma_hw {
	phys_addr_t iobase;		/* EDMA base address */
	struct ipq_edma_txdesc_ring *txdesc_ring;
					/* Tx Desc Ring, SW is producer */
	struct ipq_edma_txcmpl_ring *txcmpl_ring;
					/* Tx Compl Ring, SW is consumer */
	struct ipq_edma_rxdesc_ring *rxdesc_ring;
					/* Rx Desc Ring, SW is consumer */
	struct ipq_edma_rxfill_ring *rxfill_ring;
					/* Rx Fill Ring, SW is producer */
	uint32_t sw_version;		/* EDMA SW version */
	uint32_t rxfill_intr_mask;	/* Rx fill ring interrupt mask */
	uint32_t rxdesc_intr_mask;	/* Rx Desc ring interrupt mask */
	uint32_t txcmpl_intr_mask;	/* Tx Cmpl ring interrupt mask */
	uint32_t misc_intr_mask;	/* misc interrupt interrupt mask */
	uint32_t flags;			/* internal flags */
	uint16_t rx_payload_offset;	/* start of the payload offset */
	uint16_t rx_buff_size;		/* To do chk type Rx buffer size */
	uint8_t intr_clear_type;	/* interrupt clear */
	uint8_t intr_sw_idx_w;		/* To do chk type intr sw index */
	uint8_t rss_type;		/* rss protocol type */
	uint8_t txdesc_rings;		/* Number of TxDesc rings */
	uint8_t txdesc_ring_start;	/* Id of first TXDESC ring */
	uint8_t txdesc_ring_end;	/* Id of the last TXDESC ring */
	uint8_t txcmpl_rings;		/* Number of TxCmpl rings */
	uint8_t txcmpl_ring_start;	/* Id of first TXCMPL ring */
	uint8_t txcmpl_ring_end;	/* Id of last TXCMPL ring */
	uint8_t rxfill_rings;		/* Number of RxFill rings */
	uint8_t rxfill_ring_start;	/* Id of first RxFill ring */
	uint8_t rxfill_ring_end;	/* Id of last RxFill ring */
	uint8_t rxdesc_rings;		/* Number of RxDesc rings */
	uint8_t rxdesc_ring_start;	/* Id of first RxDesc ring */
	uint8_t rxdesc_ring_end;	/* Id of last RxDesc ring */
	uint8_t tx_intr_mask;		/* Tx intr mask */
	uint8_t rx_intr_mask;		/* Rx intr MAsk */
	uint8_t max_txcmpl_rings;	/* Max tx Completion rings */
	uint8_t max_txdesc_rings;	/* Max tx Descriptor rings */
	uint8_t max_rxdesc_rings;	/* Max rx Descriptor rings */
	uint8_t max_rxfill_rings;	/* Max rx fill rings */
	uint8_t max_ports;		/* Max ports index  */
	uint8_t start_ports;		/* Initial  ports index switch */
};

struct port_info {
	struct phy_device *phydev;
	struct mii_dev *bus;
	phys_addr_t uniphy_base;
	phy_interface_t interface;
	struct gpio_desc rst_gpio;
	ofnode node;
	ofnode pnode;
	uint32_t max_speed;
	uint32_t cur_speed;
	uint8_t phyaddr;
	uint8_t mac_mode;
	uint8_t mac_speed;
	uint8_t duplex;
	uint8_t id;
	uint8_t phy_id;
	uint8_t uniphy_id;
	uint8_t uniphy_mode;
	uint8_t uniphy_type;
	uint8_t gmac_type;
	uint8_t cur_uniphy_mode;
	uint8_t cur_gmac_type;
	bool isforce_speed;
	bool xgmac;
	bool isconfigured;
	int i2c_bus;
}__attribute__ ((aligned(8)));

struct ppe_info {
	phys_addr_t base;
	uint32_t tdm_offset;
	uint32_t tdm_ctrl_val;
	uint8_t no_ports;
	uint8_t nos_iports;
	uint8_t vsi;
	uint8_t tdm_mode;
	uint8_t no_reg;
	uint8_t nbport; /* non bridge port*/
	uint8_t ipo_action;
	bool tm;
	bool bridge_mode;
}__attribute__ ((aligned(8)));

struct ipq_eth_dev {
	phys_addr_t uniphy_base;
	struct udevice *dev;
	struct ppe_info ppe;
	struct port_info *port[CONFIG_ETH_MAX_MAC];
	struct ipq_edma_hw hw;
	size_t uniphy_size;
	bool uniphy_50mhz;
	bool emulation;
};
