# Makefile for flow manager

obj-m += qca-nss-netfn-flow-cookie.o
qca-nss-netfn-flow-cookie-objs += netfn_flow_cookie.o
qca-nss-netfn-flow-cookie-objs += netfn_flow_cookie_hash.o
qca-nss-netfn-flow-cookie-objs += netfn_flow_cookie_db.o
qca-nss-netfn-flow-cookie-objs += netfn_flow_cookie_stats.o
qca-nss-netfn-flow-cookie-objs += netfn_flow_cookie_dump.o

ccflags-y += -DNSS_NETFN_BUILD_ID=\"'Build_ID - $(shell date +'%m/%d/%y, %H:%M:%S') SoC=$(SoC)'\"
ccflags-y += -Wall -Werror
