###################################################
# Makefile for the NSS NETFN DTLS client
###################################################

NETFN_DTLS_MOD_NAME=qca-nss-netfn-dtls

ccflags-y += -Wall -Werror
ccflags-y += -DNSS_NETFN_BUILD_ID=\"'Build_ID - $(shell date +'%m/%d/%y, %H:%M:%S') SoC=$(SoC)'\"

obj-m += $(NETFN_DTLS_MOD_NAME).o

$(NETFN_DTLS_MOD_NAME)-objs += netfn_dtls.o
$(NETFN_DTLS_MOD_NAME)-objs += netfn_dtls_tun.o
$(NETFN_DTLS_MOD_NAME)-objs += netfn_dtls_session.o
$(NETFN_DTLS_MOD_NAME)-objs += netfn_dtls_dec.o
$(NETFN_DTLS_MOD_NAME)-objs += netfn_dtls_enc.o

obj ?= .

ccflags-y += $(NETFN_DTLS_INCLUDE)