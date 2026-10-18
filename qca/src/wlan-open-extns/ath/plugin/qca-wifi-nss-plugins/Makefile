ccflags-y += -I$(obj)/include -I$(obj)/..
ccflags-y += -DNSS_CLIENT_BUILD_ID="$(BUILD_ID)"
ccflags-y += -DQCA_WIFI_NSS_PLUGINS_DEBUG_LEVEL=1

qca-wifi-nss-plugins-objs := qca_wifi_nss_plugins_init.o

ifeq ($(QCA_WIFI_NSS_PLUGINS_ECM_EMESH),y)
ccflags-y += -I$(obj)/ecm_plugins/ -DQCA_WIFI_NSS_PLUGINS_ECM_EMESH
qca-wifi-nss-plugins-objs += ecm_plugins/qca_wifi_nss_plugins_emesh.o
endif

ifeq ($(QCA_WIFI_NSS_PLUGINS_ECM_FSE),y)
ccflags-y += -I$(obj)/ecm_plugins/ -DQCA_WIFI_NSS_PLUGINS_ECM_FSE
qca-wifi-nss-plugins-objs += ecm_plugins/qca_wifi_nss_plugins_fse.o
endif

ifeq ($(QCA_WIFI_NSS_PLUGINS_ECM_WIFI_CLASSIFIER),y)
ccflags-y += -I$(obj)/ecm_plugins/ -DQCA_WIFI_NSS_PLUGINS_ECM_WIFI_CLASSIFIER
qca-wifi-nss-plugins-objs += ecm_plugins/qca_wifi_nss_plugins_wifi_classifier.o
endif

ifeq ($(QCA_WIFI_NSS_PLUGINS_MSCS),y)
ccflags-y += -I$(obj)/ecm_plugins/ -DQCA_WIFI_NSS_PLUGINS_MSCS
qca-wifi-nss-plugins-objs += ecm_plugins/qca_wifi_nss_plugins_mscs.o
endif

ifeq ($(QCA_WIFI_NSS_PLUGINS_ECM_NL),y)
ccflags-y += -I$(obj)/ecm_plugins/ -DQCA_WIFI_NSS_PLUGINS_ECM_NL
qca-wifi-nss-plugins-objs += ecm_plugins/qca_wifi_nss_plugins_nl.o
endif

ifeq ($(QCA_WIFI_NSS_PLUGINS_PPE),y)
ccflags-y += -I$(obj)/ppe_plugins/ -DQCA_WIFI_NSS_PLUGINS_PPE
qca-wifi-nss-plugins-objs += ppe_plugins/qca_wifi_nss_plugins_ppe_fse.o
endif

ifeq ($(QCA_WIFI_NSS_PLUGINS_PPEDS),y)
ccflags-y += -I$(obj)/ppeds_plugins/ -DQCA_WIFI_NSS_PLUGINS_PPEDS
qca-wifi-nss-plugins-objs += ppeds_plugins/qca_wifi_nss_plugins_ppeds.o
endif

ifeq ($(QCA_WIFI_NSS_PLUGINS_IPA_FSE),y)
ccflags-y += -I$(obj)/ipa_plugins/ -DQCA_WIFI_NSS_PLUGINS_IPA_FSE
qca-wifi-nss-plugins-objs += ipa_plugins/qca_wifi_nss_plugins_ipa_fse.o
endif

obj-m += qca-wifi-nss-plugins.o

