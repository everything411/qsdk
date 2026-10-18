M ?= $(shell pwd)
KERNEL_SRC ?= /lib/modules/$(shell uname -r)/build

obj-m += ipq_cnss2.o

ifneq ($(QCA_CNSS_STREAM_MOD),)
ifneq ($(CONFIG_BUILD_OWRT),y)
ifneq ($(QCA_CNSS_LINUX_6_x_SUPPORT),y)
obj-m += ipq_cnss2_stream.o
endif
endif
endif

ipq_cnss2-objs := main.o
ifeq ($(QCA_CNSS_DEBUG_SUPPORT),y)
ipq_cnss2-objs += debug/debug.o
endif
ifeq ($(QCA_CNSS_PCI_SUPPORT),y)
ipq_cnss2-objs += pci/pci.o
ifneq ($(CONFIG_BUILD_OWRT),y)
ipq_cnss2-objs += legacyirq/legacyirq.o
endif
endif
ipq_cnss2-objs += ahb/ahb.o
ipq_cnss2-objs += cnss_common/cnss_common.o
ipq_cnss2-objs += power.o
ipq_cnss2-objs += qmi/qmi.o
ipq_cnss2-objs += wlan_firmware_service_v01.o
ipq_cnss2-objs += bus/bus.o
ipq_cnss2-objs += genl/genl.o
ipq_cnss2-objs += qmi/cnss_plat_ipc_qmi.o
ipq_cnss2-objs += cnss_plat_ipc_service_v01.o
ipq_cnss2_stream-objs := stream.o
ipq_cnss2-objs += coredump.o

CNSS2_INCLUDE = -I$(obj)
CNSS2_INCLUDE += -I$(obj)/include

ccflags-y += $(CNSS2_INCLUDE)
ccflags-y += -Wall -Werror -Wno-format-security
ifeq ($(QCA_CNSS_LINUX_6_x_SUPPORT),y)
ccflags-y += -Wno-implicit-fallthrough
endif

ifeq ($(QCA_CNSS_LOWMEM_PROFILE),y)
ccflags-y += -DCNSS_LOWMEM_PROFILE
endif

ifeq ($(ENABLE_FW_MOUNT_SUPPORT),y)
ccflags-y += -DCNSS_FW_MOUNT_SUPPORT
endif

ccflags-y += -DCONFIG_CNSS_QCN9000
ccflags-y += -DCONFIG_CNSS2_GENL

ifeq ($(QCA_CNSS_PCI_SUPPORT),y)
ccflags-y += -DCNSS_PCI_SUPPORT
endif

ifeq ($(QCA_CNSS_DEBUG_SUPPORT),y)
ccflags-y += -DCNSS_DEBUG_SUPPORT
endif

ifeq ($(CONFIG_BUILD_YOCTO),y)
	ccflags-y += -DCONFIG_CNSS2_KERNEL_MSM
	ccflags-y += -DCONFIG_CNSS2_DMA_ALLOC
	ccflags-y += -DCONFIG_CNSS2_SMMU
	ccflags-y += -DCONFIG_CNSS2_KERNEL_SSR_FRAMEWORK
else ifeq ($(CONFIG_BUILD_OWRT),y)
	ccflags-y += -DCONFIG_CNSS2_DMA_ALLOC
	ccflags-y += -DCONFIG_CNSS2_SMMU
	ccflags-y += -DCONFIG_CNSS2_KERNEL_5_15
else
	ccflags-y += -DCONFIG_CNSS2_KERNEL_IPQ
	ccflags-y += -DCONFIG_CNSS2_QGIC2M
	ccflags-y += -DCONFIG_CNSS2_KERNEL_RPROC_FRAMEWORK
ifeq ($(QCA_CNSS_PCI_SUPPORT),y)
	ccflags-y += -DCONFIG_CNSS2_LEGACY_IRQ
endif
ifeq ($(QCA_CNSS_KERNEL_DEPENDENCY),y)
	ccflags-y += -DCONFIG_CNSS2_QCOM_KERNEL_DEPENDENCY
endif
endif

ifneq (, $(filter y, $(strip ${CONFIG_TARGET_SDX75}) \
			$(strip ${CONFIG_TARGET_SDX85})))
	ccflags-y += -DCONFIG_TARGET_SDX_WKK
endif

all:
	$(MAKE) -C $(KERNEL_SRC) M=$(M) V=1 modules

modules_install:
	$(MAKE) -C $(KERNEL_SRC) M=$(M) modules_install

clean:
	$(MAKE) -C $(KERNEL_SRC) M=$(M) clean
