ccflags-y := -I$(obj)/..
ccflags-y += -Wall -Werror
ccflags-y += -DNSS_CLIENT_BUILD_ID="$(BUILD_ID)"

qca-nss-debug-objs := nss_debug.o
qca-nss-debug-objs += nss_debug_procfs.o

obj-m += qca-nss-debug.o
