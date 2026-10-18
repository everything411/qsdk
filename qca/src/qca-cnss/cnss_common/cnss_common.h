/* Copyright (c) 2016-2018, The Linux Foundation. All rights reserved.
 * Copyright (c) 2022-2023 Qualcomm Innovation Center, Inc. All rights reserved.
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

#ifndef _CNSS_COMMON_H
#define _CNSS_COMMON_H

#include "include/cnss2.h"

 #define QCATHR_VENDOR_ID		0x168C
#define QCN_VENDOR_ID			0x17CB
#define QCA6174_DEVICE_ID		0x003E
#define QCA6174_REV_ID_OFFSET		0x08
#define QCA6174_REV3_VERSION		0x5020000
#define QCA6174_REV3_2_VERSION		0x5030000
#define QCN9000_EMULATION_DEVICE_ID	0xABCD
#define QCA8074_DEVICE_ID               0xFFFF
#define QCA8074V2_DEVICE_ID             0xFFFE
#define QCA6018_DEVICE_ID               0xFFFD
#define QCA5018_DEVICE_ID               0xFFFC
#define QCN6122_DEVICE_ID		0xFFFB
#define QCA9574_DEVICE_ID		0xFFFA
#define QCA5332_DEVICE_ID		0xFFF9
#define QCN9160_DEVICE_ID		0xFFF8
#define QCN6432_DEVICE_ID		0xFFF7
#define QCA5424_DEVICE_ID		0xFFF6
#define QCA6174_DEVICE_ID		0x003E
#define QCA6390_DEVICE_ID		0x1101
#define QCA6490_DEVICE_ID		0x1103
#define QCN9000_DEVICE_ID		0x1104
#define QCN9224_DEVICE_ID		0x1109
#define QCN6122_DEVICE_BAR_SIZE		0x200000
#define QCN6122_ETR_DEV_NODE_PREFIX	"q6_qcn6122_etr"
#define QCN9160_ETR_DEV_NODE_PREFIX	"q6_qcn9160_etr"
#define QCN6432_ETR_DEV_NODE_PREFIX	"q6_qcn6432_etr"
#define ETR_DEV_NODE_LEN		17
#define QCN6122_M3_DUMP_PREFIX		"m3_dump_qcn6122"
#define QCN9160_M3_DUMP_PREFIX		"m3_dump_qcn9160"
#define QCN6432_M3_DUMP_PREFIX		"m3_dump_qcn6432"
#define M3_DUMP_NODE_LEN		18

#define MODE_0_RECOVERY_MODE		1
#define MODE_1_RECOVERY_MODE		2

#define MAX_M3_FILE_NAME_LENGTH		15
#define DEFAULT_M3_FILE_NAME		"m3.bin"
#define FW_V2_FILE_NAME			"amss20.bin"
#define FW_V2_NUMBER			2
#define AFC_SLOT_SIZE			0x1000
#define AFC_MAX_SLOT			2
#define AFC_MEM_SIZE			(AFC_SLOT_SIZE * AFC_MAX_SLOT)
#define AFC_AUTH_STATUS_OFFSET		1
#define AFC_AUTH_SUCCESS		1
#define AFC_AUTH_ERROR			0
#define AFC_QCN6122_MEM_OFFSET		0xD8000

#define QCA6390_PCIE_REMAP_BAR_CTRL_OFFSET     0x310C
#define QCN9000_PCIE_REMAP_BAR_CTRL_OFFSET	0x310C
#define QCN9000_PCIE_SOC_GLOBAL_RESET_ADDRESS 0x3008
#define QCN9000_PCIE_SOC_GLOBAL_RESET_VALUE 0x5

#define QCN9000_PCIE_MHI_RESET_ADDRESS 0x38
#define QCN9000_PCIE_MHI_RESET_VALUE 0x2

#define SHADOW_REG_COUNT			36
#define QCA6390_PCIE_SHADOW_REG_VALUE_0		0x8FC
#define QCA6390_PCIE_SHADOW_REG_VALUE_34	0x984
#define QCA6390_PCIE_SHADOW_REG_VALUE_35	0x988
#define QCA6390_WLAON_GLOBAL_COUNTER_CTRL3	0x1F80118
#define QCA6390_WLAON_GLOBAL_COUNTER_CTRL4	0x1F8011C
#define QCA6390_WLAON_GLOBAL_COUNTER_CTRL5	0x1F80120

#define QCN9000_WLAON_GLOBAL_COUNTER_CTRL3	0x1F80118
#define QCN9000_WLAON_GLOBAL_COUNTER_CTRL4	0x1F8011C
#define QCN9000_WLAON_GLOBAL_COUNTER_CTRL5	0x1F80120
#define PCIE_SOC_PCIE_REG_PCIE_SCRATCH_0	0x1E04040

#define QCN9224_QFPROM_RAW_RFA_PDET_ROW13_LSB	0x1E20338
#define OTP_BOARD_ID_MASK			0xFFFF

#define QCN9224_PCIE_PCIE_MHI_TIME_LOW          0x1E0EB28
#define QCN9224_PCIE_PCIE_MHI_TIME_HIGH         0x1E0EB2C
#define QCN9224_PCIE_TYPE0_STATUS_COMMAND_REG	0x1E1E004

#define SHADOW_REG_INTER_COUNT			43
#define QCA6390_PCIE_SHADOW_REG_INTER_0		0x1E05000
#define QCA6390_PCIE_SHADOW_REG_HUNG		0x1E050A8

#define QDSS_APB_DEC_CSR_BASE			0x1C01000

#define QDSS_APB_DEC_CSR_ETRIRQCTRL_OFFSET	0x6C
#define QDSS_APB_DEC_CSR_PRESERVEETF_OFFSET	0x70
#define QDSS_APB_DEC_CSR_PRESERVEETR0_OFFSET	0x74
#define QDSS_APB_DEC_CSR_PRESERVEETR1_OFFSET	0x78

#define MAX_UNWINDOWED_ADDRESS			0x80000
#define WINDOW_ENABLE_BIT			0x40000000
#define WINDOW_SHIFT				19
#define WINDOW_VALUE_MASK			0x3F
#define WINDOW_START				MAX_UNWINDOWED_ADDRESS
#define WINDOW_RANGE_MASK			0x7FFFF
#define MSI_MHI_VECTOR_MASK 0xF
#define MSI_MHI_VECTOR_SHIFT 0

#define MSI_QDSS_VECTOR_MASK 0xF0
#define MSI_QDSS_VECTOR_SHIFT 4

#define MSI_CE_VECTOR_MASK 0xFF00
#define MSI_CE_VECTOR_SHIFT 8

#define MSI_DP_VECTOR_MASK 0xFF0000
#define MSI_DP_VECTOR_SHIFT 16

/* Currently there is only support for MHI to operate with 3 MSIs. */
#define MAX_MHI_VECTORS 3
#define MIN_MHI_VECTORS 2
#define DEFAULT_MHI_VECTORS MAX_MHI_VECTORS

#define MAX_CE_VECTORS 8
#define MIN_CE_VECTORS 1
#define DEFAULT_CE_VECTORS MIN_CE_VECTORS

#define MAX_DP_VECTORS 16
#define MIN_DP_VECTORS 1
#define DEFAULT_DP_VECTORS MIN_DP_VECTORS

#define MAX_QDSS_VECTORS 1
#define MIN_QDSS_VECTORS 0
#define DEFAULT_QDSS_VECTORS MIN_QDSS_VECTORS

#define PCI_BAR_NUM			0

#define CNSS_ETR_SG_ENT(phys_pte)	(((phys_pte >> PAGE_SHIFT) << 4) | 0x2)
#define CNSS_ETR_SG_NXT_TBL(phys_pte)	(((phys_pte >> PAGE_SHIFT) << 4) | 0x3)
#define CNSS_ETR_SG_LST_ENT(phys_pte)	(((phys_pte >> PAGE_SHIFT) << 4) | 0x1)
#define CNSS_ETR_SG_ENT_TO_BLK(phys_pte) \
		(((phys_addr_t)phys_pte >> 4) << PAGE_SHIFT)
#define MHI_SOC_RESET_DELAY	3000  /* in msecs */

#define RDDM_DONE_DELAY        100  /* in msecs */

enum cnss_log_level {
	CNSS_LOG_LEVEL_NONE,
	CNSS_LOG_LEVEL_ERROR,
	CNSS_LOG_LEVEL_WARN,
	CNSS_LOG_LEVEL_INFO,
	CNSS_LOG_LEVEL_DEBUG,
	CNSS_LOG_LEVEL_MAX
};

extern int log_level;
extern int rddm_done_timeout;

#if IS_ENABLED(CONFIG_IPC_LOGGING)
extern void *cnss_ipc_log_context;
extern void *cnss_ipc_log_long_context;

#define cnss_ipc_log_string(_x...) do {					\
		if (cnss_ipc_log_context)				\
			ipc_log_string(cnss_ipc_log_context, _x);	\
	} while (0)

#define cnss_ipc_log_long_string(_x...) do {				\
		if (cnss_ipc_log_long_context)				\
			ipc_log_string(cnss_ipc_log_long_context, _x);	\
	} while (0)
#else
#define cnss_ipc_log_string(_x...) do {                                        \
	} while (0)

#define cnss_ipc_log_long_string(_x...) do {                           \
	} while (0)
#endif

#define cnss_pr_err(_fmt, ...) do {					\
		if (plat_priv) {					\
			pr_err("cnss[%x]: ERR: " _fmt,			\
			       plat_priv->wlfw_service_instance_id,	\
			       ##__VA_ARGS__);				\
			cnss_ipc_log_string("[%x] ERR: " pr_fmt(_fmt),	\
					    plat_priv->			\
					    wlfw_service_instance_id,	\
					    ##__VA_ARGS__);		\
		} else {						\
			pr_err("cnss: ERR: " _fmt, ##__VA_ARGS__);	\
		}							\
	} while (0)

#define cnss_pr_warn(_fmt, ...) do {					\
		if (plat_priv) {					\
			if (log_level >= CNSS_LOG_LEVEL_WARN)		\
				pr_err("cnss[%x]: WARN: " _fmt,		\
				       plat_priv->			\
				       wlfw_service_instance_id,	\
				       ##__VA_ARGS__);			\
			else						\
				pr_warn("cnss[%x]: WARN: " _fmt,	\
					plat_priv->			\
					wlfw_service_instance_id,	\
					##__VA_ARGS__);			\
			cnss_ipc_log_string("[%x] WRN: " pr_fmt(_fmt),	\
					    plat_priv->			\
					    wlfw_service_instance_id,	\
					    ##__VA_ARGS__);		\
		} else {						\
			pr_err("cnss: WARN: " _fmt, ##__VA_ARGS__);	\
		}							\
	} while (0)

#define cnss_pr_info(_fmt, ...) do {					\
		if (plat_priv) {					\
			if (log_level >= CNSS_LOG_LEVEL_INFO)		\
				pr_err("cnss[%x]: INFO: " _fmt,		\
				       plat_priv->			\
				       wlfw_service_instance_id,	\
				       ##__VA_ARGS__);			\
			else						\
				pr_info("cnss[%x]: INFO: " _fmt,	\
					plat_priv->			\
					wlfw_service_instance_id,	\
					##__VA_ARGS__);			\
			cnss_ipc_log_string("[%x] INF: " pr_fmt(_fmt),	\
					    plat_priv->			\
					    wlfw_service_instance_id,	\
					    ##__VA_ARGS__);		\
		} else {						\
			pr_err("cnss: INFO: " _fmt, ##__VA_ARGS__);	\
		}							\
	} while (0)

#define cnss_pr_dbg(_fmt, ...) do {					\
		if (plat_priv) {					\
			if (log_level >= CNSS_LOG_LEVEL_DEBUG)		\
				pr_err("cnss[%x]: DBG: " _fmt,		\
				       plat_priv->			\
				       wlfw_service_instance_id,	\
				       ##__VA_ARGS__);			\
			else						\
				pr_debug("cnss[%x]: DBG: " _fmt,	\
					 plat_priv->			\
					 wlfw_service_instance_id,	\
					 ##__VA_ARGS__);		\
			cnss_ipc_log_string("[%x] DBG: " pr_fmt(_fmt),	\
					    plat_priv->			\
					    wlfw_service_instance_id,	\
					    ##__VA_ARGS__);		\
		} else {						\
			if (log_level >= CNSS_LOG_LEVEL_DEBUG)          \
				pr_err("cnss: DBG: " _fmt,              \
				       ##__VA_ARGS__);                  \
		}							\
	} while (0)

#define cnss_pr_vdbg(_fmt, ...) do {					\
		pr_err("cnss: " _fmt, ##__VA_ARGS__);	\
		cnss_ipc_log_long_string("%scnss: " _fmt, "",		\
					 ##__VA_ARGS__);		\
	} while (0)

#define CNSS_ASSERT(_condition) do {					\
		if (!(_condition) &&					\
		    cnss_wait_for_rddm_complete(plat_priv)) {		\
			cnss_dump_qmi_history();			\
			cnss_pr_err("ASSERT at line %d\n",		\
				    __LINE__);				\
			BUG_ON(1);					\
		}							\
	} while (0)

#define cnss_fatal_err(_fmt, ...) do {					\
		if (plat_priv) {					\
			pr_err("cnss[%x]: FATAL: " _fmt,		\
			       plat_priv->wlfw_service_instance_id,	\
			       ##__VA_ARGS__);				\
		} else {						\
			pr_err("cnss: FATAL: " _fmt, ##__VA_ARGS__);	\
		}							\
	} while (0)

struct cnss_msi_user {
	char *name;
	int num_vectors;
	u32 base_vector;
};

struct cnss_msi_config {
	int total_vectors;
	int total_users;
	struct cnss_msi_user *users;
};

struct cnss_reg_offset {
	char *name;
	u32 offset;
};

#ifdef CONFIG_CNSS2_QGIC2M
struct qgic2_msi *cnss_qgic2_enable_msi(struct cnss_plat_data *plat_priv);
void cnss_qgic2_disable_msi(struct cnss_plat_data *plat_priv);
#endif

void cnss_free_soc_info(struct cnss_plat_data *plat_priv);

void afc_memset(struct cnss_plat_data *plat_priv, void *s,
		       int c, size_t n);
int cnss_send_buffer_to_afcmem(struct device *dev, char *afcdb, uint32_t len,
			       uint8_t slotid);
int cnss_mlo_mem_alloc(struct cnss_plat_data *plat_priv, int index);

void cnss_do_mlo_global_memset(struct cnss_plat_data *plat_priv, u64 mem_size);
struct cnss_msi_config *cnss_get_msi_config(struct cnss_plat_data *plat_priv);
void cnss_override_msi_assignment(struct cnss_plat_data *plat_priv,
					struct cnss_msi_config *msi_config);

int cnss_etr_sg_tbl_alloc(struct cnss_plat_data *plat_priv);
void pci_update_msi_vectors(struct cnss_msi_config *msi_config,
				   char *user_name, int num_vectors,
				   int *vector_idx);

struct device_node *cnss_get_etr_dev_node(struct cnss_plat_data *plat_priv);
int cnss_bus_init_by_type(int type);
enum cnss_dev_bus_type cnss_get_bus_type(unsigned long device_id);
void *cnss_bus_dev_to_bus_priv(struct device *dev);
struct cnss_plat_data *cnss_bus_dev_to_plat_priv(struct device *dev);
struct cnss_bus_ops *cnss_ahb_get_ops(void);
void cnss_etr_sg_tbl_free(uint32_t *vaddr,
				 struct cnss_plat_data *plat_priv,
				 uint32_t ents);
bool cnss_wait_for_rddm_complete(struct cnss_plat_data *plat_priv);
int cnss_debugfs_create(struct cnss_plat_data *plat_priv);
void cnss_debugfs_destroy(struct cnss_plat_data *plat_priv);
void qmi_record(u8 instance_id, u16 msg_id, s8 error_msg, s8 resp_err_msg);
#endif
