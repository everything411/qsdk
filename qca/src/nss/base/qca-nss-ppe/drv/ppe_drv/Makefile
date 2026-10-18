# Makefile for PPE driver

ccflags-y := -I$(obj)/../exports -I$(obj)/..
ccflags-y += -DPPE_DRV_BUILD_ID="$(BUILD_ID)"
ifeq ($(SoC),$(filter $(SoC),ipq53xx))
ccflags-y += -DNSS_PPE_IPQ53XX
ccflags-y += -DPPE_DRV_BRIDGE_NEXT_HOP
ccflags-y += -DPPE_DRV_SRC_INTERFACE_CHECK
ccflags-y += -DPPE_DRV_FLOW_TBL_POLICER
endif

ifeq ($(SoC),$(filter $(SoC),ipq95xx))
ccflags-y += -DNSS_PPE_IPQ95XX
endif

ifeq ($(SoC),$(filter $(SoC),ipq52xx))
ccflags-y += -DNSS_PPE_IPQ52XX
ccflags-y += -DNSS_PPE_PON_SUPPORT
endif

ifeq ($(SoC),$(filter $(SoC),ipq96xx))
ccflags-y += -DNSS_PPE_IPQ96XX
endif

ifeq ($(SoC),$(filter $(SoC),ipq96xx ipq52xx))
ccflags-y += -DNSS_PPE_DRV_HW_GRO
ccflags-y += -DPPE_DRV_DDRQ_ENABLE
endif

ifeq ($(SoC),$(filter $(SoC),ipq95xx ipq53xx))
ccflags-y += -DPPE_DRV_FLOW_IG_MAC_WAR
endif

ifeq ($(SoC),$(filter $(SoC),ipq96xx))
	ccflags-y += -DPPE_DRV_IPSEC_FULL_INLINE_SC_CONFIG
endif

ifeq ($(SoC),$(filter $(SoC),ipq96xx ipq52xx))
ccflags-y += -DNSS_PPE_FEATURE_EXCEPTION_EDIT
ccflags-y += -DNSS_PPE_DSCP_PBIT_FEATURE_SUPPORT
ccflags-y += -DNSS_PPE_PM_COUNTER_FEATURE_SUPPORT
ccflags-y += -DNSS_PPE_EXT_VLAN_FEATURE_SUPPORT
ccflags-y += -DPPE_DRV_MCAST_ENQ_CTRL
ccflags-y += -DNSS_PPE_DSA_QINQ_SUPPORT
ccflags-y += -DNSS_PPE_DSA_QUEUE_SUPPORT
endif

ifeq ($(SoC),$(filter $(SoC),ipq96xx ipq52xx ipq54xx))
ccflags-y += -DNSS_PPE_SIXTH_TUPLE
endif

ifdef PPE_DRV_ESP_SPI_PASSTH_ENABLE
ccflags-y += -DPPE_DRV_ESP_SPI_PASSTH_ENABLE
endif

obj-m += qca-nss-ppe.o
qca-nss-ppe-objs := ppe_drv.o
qca-nss-ppe-objs += ppe_drv_acl.o
qca-nss-ppe-objs += ppe_drv_br.o
qca-nss-ppe-objs += ppe_drv_cc.o
qca-nss-ppe-objs += ppe_drv_dp.o
ifdef PPE_IPSEC_ENABLE
qca-nss-ppe-objs += ppe_drv_eip.o
endif
qca-nss-ppe-objs += ppe_drv_exception.o
qca-nss-ppe-objs += ppe_drv_flow.o
qca-nss-ppe-objs += ppe_drv_host.o
qca-nss-ppe-objs += ppe_drv_iface.o
qca-nss-ppe-objs += ppe_drv_lag.o
qca-nss-ppe-objs += ppe_drv_l3_if.o
qca-nss-ppe-objs += ppe_drv_nexthop.o
qca-nss-ppe-objs += ppe_drv_policer.o
qca-nss-ppe-objs += ppe_drv_port.o
qca-nss-ppe-objs += ppe_drv_pppoe.o
qca-nss-ppe-objs += ppe_drv_pppoe_session.o
qca-nss-ppe-objs += ppe_drv_pub_ip.o
qca-nss-ppe-objs += ppe_drv_qos.o
qca-nss-ppe-objs += ppe_drv_cos_map.o
qca-nss-ppe-objs += ppe_drv_sc.o
qca-nss-ppe-objs += ppe_drv_stats.o
qca-nss-ppe-objs += ppe_drv_vlan.o
qca-nss-ppe-objs += ppe_drv_vp.o
qca-nss-ppe-objs += ppe_drv_vsi.o
qca-nss-ppe-objs += ppe_drv_v4.o
qca-nss-ppe-objs += ppe_drv_v6.o
qca-nss-ppe-objs += ppe_drv_flow_dump.o
qca-nss-ppe-objs += ppe_drv_if_map.o
ifeq ($(SoC),$(filter $(SoC),ipq96xx ipq52xx))
qca-nss-ppe-objs += ppe_drv_dscp.o
qca-nss-ppe-objs += ppe_drv_pm.o
qca-nss-ppe-objs += ppe_drv_port_mgmt.o
endif
ifdef PPE_TUN_ENABLE
ccflags-y += -DPPE_TUNNEL_ENABLE
qca-nss-ppe-objs += tun/ppe_drv_tun.o
qca-nss-ppe-objs += tun/ppe_drv_tun_decap.o
qca-nss-ppe-objs += tun/ppe_drv_tun_decap_map.o
qca-nss-ppe-objs += tun/ppe_drv_tun_decap_xlate_rule.o
qca-nss-ppe-objs += tun/ppe_drv_tun_encap.o
qca-nss-ppe-objs += tun/ppe_drv_tun_encap_xlate_rule.o
qca-nss-ppe-objs += tun/ppe_drv_tun_l2tp.o
qca-nss-ppe-objs += tun/ppe_drv_tun_l3_if.o
qca-nss-ppe-objs += tun/ppe_drv_tun_prgm_prsr.o
qca-nss-ppe-objs += tun/ppe_drv_tun_prgm_prsr_gre.o
qca-nss-ppe-objs += tun/ppe_drv_tun_custom.o
qca-nss-ppe-objs += tun/ppe_drv_tun_udf.o
qca-nss-ppe-objs += tun/ppe_drv_tun_v4.o
qca-nss-ppe-objs += tun/ppe_drv_tun_v6.o
qca-nss-ppe-objs += tun/ppe_drv_tun_gre.o
ifeq ($(SoC),$(filter $(SoC),ipq96xx ipq52xx))
ccflags-y += -DNSS_PPE_TUNNEL_ENHANCED_PARSER
ccflags-y += -DNSS_PPE_TUNNEL_TPR_ENABLE
qca-nss-ppe-objs += tun/ppe_drv_tun_tpr.o
endif
endif

ifeq ($(SoC),$(filter $(SoC),ipq54xx))
ccflags-$(CONFIG_NF_CONNTRACK_NPTV6_EXT) += -DPPE_DRV_NPTV6_HW_SUPPORT
ccflags-y += -DPPE_DRV_RPS_HASH_SELECT
ccflags-y += -DPPE_DRV_PKT_PADDING_STRIP
ccflags-y += -DPPE_DRV_BRIDGE_NEXT_HOP
ccflags-y += -DPPE_DRV_SRC_INTERFACE_CHECK
ccflags-y += -DPPE_DRV_FLOW_TBL_POLICER
ccflags-y += -DNSS_PPE_IPQ54XX
ifeq ($(CONFIG_NF_CONNTRACK_NPTV6_EXT),y)
qca-nss-ppe-objs += ppe_drv_nptv6.o
qca-nss-ppe-objs += ppe_drv_nptv6_hairpin.o
endif
endif

ifeq ($(CONFIG_NET_DSA)_$(filter $(SoC),ipq95xx ipq53xx ipq54xx ipq96xx ipq52xx),y_$(SoC))
ccflags-y += -DNSS_VLAN_BASED_DSA_SUPPORT
endif

ifeq ($(SoC),$(filter $(SoC),ipq95xx ipq54xx ipq53xx ipq96xx ipq52xx))
ccflags-y += -DPPE_LOOPBACK_RING_SUPPORT
ccflags-y += -DPPE_DRV_DDR_BUFFER_SPEED_MISMATCH
endif

ifeq ($(SoC),$(filter $(SoC),ipq53xx ipq52xx))
ccflags-y += -DNSS_PPE_ACL_RULE_RANGE
endif

ifeq ($(SoC),$(filter $(SoC),ipq96xx ipq52xx))
ccflags-y += -DPPE_LOOPBACK_PORT_SUPPORT
ccflags-y += -DPPE_ACL_DEST_BIND_SUPPORT
ccflags-y += -DNSS_PPE_DRV_PORT_ISOL_SUPPORT
ccflags-y += -DNSS_PPE_DRV_PORT_MGMT_SUPPORT
ccflags-$(CONFIG_NF_CONNTRACK_NPTV6_EXT) += -DPPE_DRV_NPTV6_HW_SUPPORT
ccflags-y += -DNSS_PPE_L2_VP_SC_ENQ_BYPASS
ccflags-y += -DNSS_PPE_FEATURE_EXCEPTION_EDIT
ccflags-y += -DPPE_DRV_RPS_HASH_SELECT
ccflags-y += -DPPE_DRV_PKT_PADDING_STRIP
ccflags-y += -DPPE_DRV_BRIDGE_NEXT_HOP
ccflags-y += -DPPE_DRV_SRC_INTERFACE_CHECK
ccflags-y += -DPPE_DRV_FLOW_TBL_POLICER
ifeq ($(CONFIG_NF_CONNTRACK_NPTV6_EXT),y)
qca-nss-ppe-objs += ppe_drv_nptv6.o
qca-nss-ppe-objs += ppe_drv_nptv6_hairpin.o
endif
ccflags-y += -DPPE_DRV_DDRQ_ENABLE
ccflags-y += -DPPE_UDP_ST_SUPPORT
endif

ifeq ($(CONFIG_NET_DSA)_$(filter $(SoC),ipq53xx ipq54xx ipq96xx ipq52xx),y_$(SoC))
ccflags-y += -DNSS_ATH_HDR_BASED_DSA_SUPPORT
qca-nss-ppe-objs += ppe_drv_athtag.o
endif

ifeq ($(SoC),$(filter $(SoC),ipq52xx))
ccflags-y += -DPPE_DRV_PON_PQ_MAPPING
ccflags-y += -DNSS_PPE_FEATURE_DOT1P
ccflags-y += -DNSS_PPE_FEATURE_GEMPORT
ccflags-y += -DNSS_PPE_MCAST_FEATURE_SUPPORT
qca-nss-ppe-objs += ppe_drv_dot1p.o
qca-nss-ppe-objs += ppe_drv_gemport.o
qca-nss-ppe-objs += ppe_drv_mcast.o
ifndef PPE_SFU_PROFILE
ccflags-y += -DPPE_DRV_VEIP_FEATURE_SUPPORT
qca-nss-ppe-objs += ppe_drv_veip.o
endif
endif

ifeq ($(SoC),$(filter $(SoC),ipq96xx ipq52xx))
ccflags-y += -DPPE_DRV_FLOW_COOKIE_48B_SUPPORT
ccflags-y += -DPPE_DRV_GRETAP_MAPT_SINGLEPASS
endif

ccflags-y += -Wall -Werror
ccflags-y += -DPPE_DRV_DEBUG_LEVEL=3
