# SPDX-License-Identifier: BSD-3-Clause-Clear
obj-$(CPTCFG_ATH12K) += ath12k_wifi8.o
ath12k_wifi8-y += core.o \
		  mgmt_rx.o \
	          pci.o \
		  hw.o \
		  mhi.o \
		  ce.o \
		  wmi.o \
		  dp.o \
		  dp_tx.o \
		  dp_rx.o \
		  hal.o \
		  hal_tx.o \
		  hal_rx.o \
		  hal_qcn9625.o\
		  hal_mon.o \
		  hal_mon_qcn9625.o \
		  hal_qcn9625_v1.o \
		  dp_peer.o\
		  dp_ast.o \
		  dp_tx_flow_info.o \
		  dp_htt.o \
		  hal_queue.o \
		  dp_pool.o \
		  dp_msdu_queue.o \
		  dp_tx_tid.o \
		  dp_tx_queue.o \
		  dp_mon2.o \
		  umac_reset.o \
		  dp_telemetry.o \
		  dp_rx_err.o

ath12k_wifi8-$(CPTCFG_QCN_EXTN) +=  qcn_extns/wifi8_hw_extn.o \
				    qcn_extns/mgmt_rx_extn.o \
                                    qcn_extns/dp_rx_extn.o

ath12k_wifi8-$(CPTCFG_ATH12K_PPE_DS_SUPPORT) += ppeds.o
ath12k_wifi8-$(CPTCFG_EXT_IPA_OFFLOAD) += qcn_extns/ipa/dp_ipa_fse.o \
					  qcn_extns/ipa/dp_ipa.o \
					  qcn_extns/ipa/dp_ipa_rx.o \
					  qcn_extns/ipa/dp_ipa_tx.o \
					  qcn_extns/ipa/dp_ipa_debugfs.o
