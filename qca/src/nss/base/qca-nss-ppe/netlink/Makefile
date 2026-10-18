ccflags-y := -Wall -Werror
ccflags-y += -I$(obj)/include -I$(obj)/../exports -I$(obj)/..
ccflags-y += -DNSS_PPENL_DEBUG_LEVEL=2
ccflags-y += -DNSS_CLIENT_BUILD_ID="$(BUILD_ID)"
ccflags-y += -DCONFIG_NSS_PPENL_ACL=1
ccflags-y += -DCONFIG_NSS_PPENL_POLICER=1
ccflags-y += -DCONFIG_NSS_PPENL_QOS=1
ccflags-y += -DCONFIG_NSS_PPENL_COS_MAP=1
ccflags-y += -DCONFIG_NSS_PPENL_EXCEPTION=1
ccflags-y += -DCONFIG_NSS_PPENL_VLAN=1

ifeq ($(SoC),$(filter $(SoC),ipq96xx ipq52xx))
ccflags-y += -DNSS_PPE_FEATURE_EXCEPTION_EDIT
ccflags-y += -DCONFIG_NSS_PPENL_DSCP=1
ccflags-y += -DCONFIG_NSS_PPENL_PM=1
ccflags-y += -DNSS_PPE_DSCP_PBIT_FEATURE_SUPPORT
ccflags-y += -DNSS_PPE_PM_COUNTER_FEATURE_SUPPORT
ccflags-y += -DNSS_PPE_EXT_VLAN_FEATURE_SUPPORT
ccflags-y += -DCONFIG_NSS_PPENL_PORT_MGMT=1
ccflags-y += -DNSS_PPE_PPENL_PORT_MGMT_SUPPORT
ccflags-y += -DCONFIG_NSS_PPENL_EDMA_DDRQ=1
ccflags-y += -DNSS_PPE_EDMA_DDRQ_FEATURE_SUPPORT
ifeq ($(PPE_MIRROR_ENABLED), y)
ccflags-y += -DNSS_PPE_PORT_MIRROR_FEATURE_SUPPORT
ccflags-y += -DCONFIG_NSS_PPENL_PORT_MIRROR=1
endif
endif

qca-nss-ppe-netlink-objs := nss_ppenl.o
qca-nss-ppe-netlink-objs += nss_ppenl_acl.o
qca-nss-ppe-netlink-objs += nss_ppenl_policer.o
qca-nss-ppe-netlink-objs += nss_ppenl_qos.o
qca-nss-ppe-netlink-objs += nss_ppenl_cos_map.o
qca-nss-ppe-netlink-objs += nss_ppenl_exception.o

ifeq ($(SoC),$(filter $(SoC),ipq96xx ipq52xx))
qca-nss-ppe-netlink-objs += nss_ppenl_dscp.o
qca-nss-ppe-netlink-objs += nss_ppenl_pm.o
qca-nss-ppe-netlink-objs += nss_ppenl_port_mgmt.o
qca-nss-ppe-netlink-objs += nss_ppenl_vlan.o
qca-nss-ppe-netlink-objs += nss_ppenl_edma_ddrq.o
ifeq ($(PPE_MIRROR_ENABLED), y)
qca-nss-ppe-netlink-objs += nss_ppenl_port_mirror.o
endif
endif

ifeq ($(SoC),$(filter $(SoC),ipq52xx))
ccflags-y += -DNSS_PPE_FEATURE_DOT1P
ccflags-y += -DCONFIG_NSS_PPENL_DOT1P=1
qca-nss-ppe-netlink-objs += nss_ppenl_dot1p.o
qca-nss-ppe-netlink-objs += nss_ppenl_gemport.o
qca-nss-ppe-netlink-objs += nss_ppenl_mcast.o
ccflags-y += -DNSS_PPE_FEATURE_GEMPORT
ccflags-y += -DCONFIG_NSS_PPENL_GEM_PORT=1
ccflags-y += -DCONFIG_NSS_PPENL_PON_PORT=1
ccflags-y += -DNSS_PPE_PON_SUPPORT
ccflags-y += -DCONFIG_NSS_PPENL_MCAST=1
ccflags-y += -DNSS_PPE_MCAST_FEATURE_SUPPORT
endif

ifeq ($(PPE_TUN_RPS_ENABLED), y)
ccflags-y += -DCONFIG_NSS_PPENL_TUN_RPS=1
qca-nss-ppe-netlink-objs += nss_ppenl_tun_rps.o
endif

obj-m += qca-nss-ppe-netlink.o
