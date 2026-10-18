###################################################
# Makefile for the NSS dscpstats driver
###################################################

obj ?= .

obj-m += qca-nss-dscpstats.o
qca-nss-dscpstats-objs += nss_dscpstats.o nss_dscpstats_db.o nss_dscpstats_nl.o

ccflags-y += -I$(obj)/exports
ccflags-y += -Wall -Werror
