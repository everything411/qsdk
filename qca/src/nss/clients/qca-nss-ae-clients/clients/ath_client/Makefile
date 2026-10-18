ccflags-y := -I$(obj)/../../exports -I$(obj)/.. -I$(obj)/exports
ccflags-y += -DNSS_PPE_BUILD_ID="$(BUILD_ID)"

obj-m += qca-nss-ppe-ath-clients.o
qca-nss-ppe-ath-clients-objs := nss_ppe_ath_client.o

ccflags-y += -DNSS_PPE_ATH_CLIENT_DEBUG_LEVEL=4
ccflags-y += -Wall -Werror
