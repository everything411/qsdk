ccflags-y := -Wall -Werror
ccflags-y += -I$(obj)/include -I$(obj)/..
ccflags-y += -DNSS_CLIENT_BUILD_ID="$(BUILD_ID)"
ccflags-y += -DQCA_WIFI_PLUGINS_DEBUG_LEVEL=4

qca-wifi-plugins-objs := qca_nss_wifi_plugins_init.o
qca-wifi-plugins-objs += qca_nss_wifi_plugins_spm.o

obj-m += qca-wifi-plugins.o

