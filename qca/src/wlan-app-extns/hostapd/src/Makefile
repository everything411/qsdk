CFLAGS += -DCONFIG_QCN_EXTN
CFLAGS += -DCONFIG_QCN_LOG_EXTN

ifdef CONFIG_QCA_LAB_TEST_FEATURES
CFLAGS += -DCONFIG_QCA_LAB_TEST_FEATURES
endif

OBJS += \
	qcn_extns/ap_drv_ops_extn.o \
	qcn_extns/wps_rdk_extn.o \
	qcn_extns/wpa_auth_rdk_extn.o \
	qcn_extns/240mhz.o \
	qcn_extns/cmn_extn.o \
	qcn_extns/driver_nl80211_extn.o \
	qcn_extns/ap_config_extn.o \
	qcn_extns/ctrl_iface_extn.o \
	qcn_extns/block_channel.o \
	qcn_extns/primary_chanlist.o \
	qcn_extns/drv_callbacks_extn.o \
	qcn_extns/hostapd_rptr_extn.o \
	qcn_extns/esp.o \
	qcn_extns/rnr.o \
	qcn_extns/macaddr.o \
	qcn_extns/reg_extn.o \
	qcn_extns/repurpose.o \
	qcn_extns/dfs_extn.o \
	qcn_extns/ucode_extn.o \
	qcn_extns/mucapwar.o \
	qcn_extns/dcs.o \
	qcn_extns/afc.o \
	qcn_extns/hostapd_extn.o \
	qcn_extns/hostapd_log_extn.o \
	qcn_extns/qcn_ie_extn.o \
	qcn_extns/ttlm_extn.o \
	qcn_extns/dfs_rcsa_extn.o \
	qcn_extns/wds_ie.o

ifdef CONFIG_QCA_WPA_SUPPLICANT_BUILD
OBJS += qcn_extns/wpa_supplicant_extn.o
OBJS += qcn_extns/wpa_config_extn.o
endif

ifeq ($(CONFIG_PACKAGE_udbg-enhanced),y)
ifeq ($(CONFIG_UDBG_ENH),y)
CFLAGS += -DCONFIG_UDBG_ENH
ifndef UDBG_CLIENT_CORE_INC
$(error UDBG_CLIENT_CORE_INC must be provided when CONFIG_UDBG_ENH is enabled)
endif
ifndef UDBG_CLIENT_CORE_LIB
$(error UDBG_CLIENT_CORE_LIB must be provided when CONFIG_UDBG_ENH is enabled)
endif
CFLAGS += -I$(UDBG_CLIENT_CORE_INC)
LIBS += $(UDBG_CLIENT_CORE_LIB)
OBJS += qcn_extns/udbg_enh.o
endif
endif

ifdef CONFIG_EXTERNAL_PLUGIN
CFLAGS += -DHOSTAPD_EXTERNAL_PLUGIN
CFLAGS += -DHOSTAPD_EXTERNAL_PLUGIN_TESTAPP
OBJS += qcn_extns/hostapd_if_eloop.o
OBJS += qcn_extns/hostapd_if_plugin.o
endif

ifdef CONFIG_ACS
OBJS += qcn_extns/acs_extn.o
OBJS += qcn_extns/cbs.o

ifneq ($(wildcard ../qcn_extns/qacs/qacs.c),)
CFLAGS += -DCONFIG_QCN_APP_EXTN
OBJS += qcn_extns/qacs/qacs.o
endif
endif

ifdef CONFIG_IEEE80211BN
OBJS += qcn_extns/mapc_extn.o
endif
