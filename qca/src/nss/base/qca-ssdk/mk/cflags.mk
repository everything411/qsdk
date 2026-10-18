#############################################
#             OS version
#############################################
OS=linux


ifeq ($(KVER),$(filter 5.4%,$(KVER)))
    OS_VER=5_4
endif

ifeq ($(KVER),$(filter 6.1%,$(KVER)))
    OS_VER=6_1
endif

ifeq ($(KVER),$(filter 6.6%,$(KVER)))
    OS_VER=6_6
endif

#############################################
#             define cflags
#############################################
MODULE_CFLAG :=

ifeq (TRUE, $(SWCONFIG))
  MODULE_CFLAG += -DIN_SWCONFIG
endif

ifeq (TRUE, $(IN_ACL))
  MODULE_CFLAG += -DIN_ACL
endif

ifeq (TRUE, $(IN_FDB))
  MODULE_CFLAG += -DIN_FDB
endif

ifeq (TRUE, $(IN_FDB_MINI))
  MODULE_CFLAG += -DIN_FDB_MINI
endif

ifeq (TRUE, $(IN_IGMP))
  MODULE_CFLAG += -DIN_IGMP
endif

ifeq (TRUE, $(IN_LEAKY))
  MODULE_CFLAG += -DIN_LEAKY
endif

ifeq (TRUE, $(IN_LED))
  MODULE_CFLAG += -DIN_LED
endif

ifeq (TRUE, $(IN_MIB))
  MODULE_CFLAG += -DIN_MIB
endif

ifeq (TRUE, $(IN_MIRROR))
  MODULE_CFLAG += -DIN_MIRROR
endif

ifeq (TRUE, $(IN_MISC))
  MODULE_CFLAG += -DIN_MISC
endif

ifeq (TRUE, $(IN_MISC_MINI))
  MODULE_CFLAG += -DIN_MISC_MINI
endif

ifeq (TRUE, $(IN_PORTCONTROL))
  MODULE_CFLAG += -DIN_PORTCONTROL
endif

ifeq (TRUE, $(IN_PORTCONTROL_MINI))
  MODULE_CFLAG += -DIN_PORTCONTROL_MINI
endif

ifeq (TRUE, $(IN_PORTVLAN))
  MODULE_CFLAG += -DIN_PORTVLAN
endif

ifeq (TRUE, $(IN_PORTVLAN_MINI))
  MODULE_CFLAG += -DIN_PORTVLAN_MINI
endif

ifeq (TRUE, $(IN_QOS))
  MODULE_CFLAG += -DIN_QOS
endif

ifeq (TRUE, $(IN_QOS_MINI))
  MODULE_CFLAG += -DIN_QOS_MINI
endif

ifeq (TRUE, $(IN_RATE))
  MODULE_CFLAG += -DIN_RATE
endif

ifeq (TRUE, $(IN_STP))
  MODULE_CFLAG += -DIN_STP
endif

ifeq (TRUE, $(IN_VLAN))
  MODULE_CFLAG += -DIN_VLAN
endif

ifeq (TRUE, $(IN_VLAN_MINI))
  MODULE_CFLAG += -DIN_VLAN_MINI
endif

ifeq (TRUE, $(IN_REDUCED_ACL))
  MODULE_CFLAG += -DIN_REDUCED_ACL
endif

ifeq (TRUE, $(IN_COSMAP))
  MODULE_CFLAG += -DIN_COSMAP
endif

ifeq (TRUE, $(IN_COSMAP_MINI))
  MODULE_CFLAG += -DIN_COSMAP_MINI
endif

ifeq (TRUE, $(IN_IP))
  MODULE_CFLAG += -DIN_IP
endif

ifeq (TRUE, $(IN_IP_MINI))
  MODULE_CFLAG += -DIN_IP_MINI
endif

ifeq (TRUE, $(IN_NAT))
  MODULE_CFLAG += -DIN_NAT
endif

ifeq (TRUE, $(IN_FLOW))
  MODULE_CFLAG += -DIN_FLOW
endif

ifeq (TRUE, $(IN_FLOW_MINI))
  MODULE_CFLAG += -DIN_FLOW_MINI
endif

ifeq (TRUE, $(IN_SFE))
  MODULE_CFLAG += -DIN_SFE
endif

ifeq (TRUE, $(IN_TRUNK))
  MODULE_CFLAG += -DIN_TRUNK
endif

ifeq (TRUE, $(IN_SEC))
  MODULE_CFLAG += -DIN_SEC
endif

ifeq (TRUE, $(IN_SEC_MINI))
  MODULE_CFLAG += -DIN_SEC_MINI
endif

ifeq (TRUE, $(IN_QM))
  MODULE_CFLAG += -DIN_QM
endif

ifeq (TRUE, $(IN_QM_MINI))
  MODULE_CFLAG += -DIN_QM_MINI
endif

ifeq (TRUE, $(IN_NAT_HELPER))
  MODULE_CFLAG += -DIN_NAT_HELPER
endif

ifeq (TRUE, $(IN_INTERFACECONTROL))
  MODULE_CFLAG += -DIN_INTERFACECONTROL
endif

ifeq (TRUE, $(IN_CTRLPKT))
  MODULE_CFLAG += -DIN_CTRLPKT
endif

ifeq (TRUE, $(IN_SERVCODE))
  MODULE_CFLAG += -DIN_SERVCODE
endif

ifeq (TRUE, $(IN_RSS_HASH))
  MODULE_CFLAG += -DIN_RSS_HASH
endif

ifeq (TRUE, $(IN_MACBLOCK))
  MODULE_CFLAG += -DIN_MACBLOCK
endif

ifeq (TRUE, $(IN_RFS))
  MODULE_CFLAG += -DIN_RFS
endif

ifeq (TRUE, $(IN_AQUANTIA_PHY))
  MODULE_CFLAG += -DIN_AQUANTIA_PHY
endif

ifeq (TRUE, $(IN_QCA808X_PHY))
  MODULE_CFLAG += -DIN_QCA808X_PHY
endif

ifeq (TRUE, $(IN_SFP_PHY))
  MODULE_CFLAG += -DIN_SFP_PHY
endif

ifeq (TRUE, $(IN_VSI))
  MODULE_CFLAG += -DIN_VSI
endif

ifeq (TRUE, $(IN_VSI_MINI))
  MODULE_CFLAG += -DIN_VSI_MINI
endif

ifeq (TRUE, $(IN_PPPOE))
  MODULE_CFLAG += -DIN_PPPOE
endif

ifeq (TRUE, $(IN_BM))
  MODULE_CFLAG += -DIN_BM
endif

ifeq (TRUE, $(IN_BM_MINI))
  MODULE_CFLAG += -DIN_BM_MINI
endif

ifeq (TRUE, $(IN_SHAPER))
  MODULE_CFLAG += -DIN_SHAPER
endif

ifeq (TRUE, $(IN_SHAPER_MINI))
  MODULE_CFLAG += -DIN_SHAPER_MINI
endif

ifeq (TRUE, $(IN_POLICER))
  MODULE_CFLAG += -DIN_POLICER
endif

ifeq (TRUE, $(IN_POLICER_MINI))
  MODULE_CFLAG += -DIN_POLICER_MINI
endif

ifeq (TRUE, $(IN_UNIPHY))
  MODULE_CFLAG += -DIN_UNIPHY
endif

ifeq (TRUE, $(IN_UNIPHY_MINI))
  MODULE_CFLAG += -DIN_UNIPHY_MINI
endif

ifeq (TRUE, $(IN_PTP))
  MODULE_CFLAG += -DIN_PTP
endif

ifeq (TRUE, $(IN_VPORT))
  MODULE_CFLAG += -DIN_VPORT
endif

ifeq (TRUE, $(IN_TUNNEL))
  MODULE_CFLAG += -DIN_TUNNEL
endif

ifeq (TRUE, $(IN_TUNNEL_MINI))
  MODULE_CFLAG += -DIN_TUNNEL_MINI
endif

ifeq (TRUE, $(IN_MAPT))
  MODULE_CFLAG += -DIN_MAPT
endif

ifeq (TRUE, $(IN_VXLAN))
  MODULE_CFLAG += -DIN_VXLAN
endif

ifeq (TRUE, $(IN_VXLAN_MINI))
  MODULE_CFLAG += -DIN_VXLAN_MINI
endif

ifeq (TRUE, $(IN_GENEVE))
  MODULE_CFLAG += -DIN_GENEVE
endif

ifeq (TRUE, $(IN_TUNNEL_PROGRAM))
  MODULE_CFLAG += -DIN_TUNNEL_PROGRAM
endif

ifeq (TRUE, $(IN_TUNNEL_PROGRAM_MINI))
  MODULE_CFLAG += -DIN_TUNNEL_PROGRAM_MINI
endif

ifeq (TRUE, $(IN_ATHTAG))
  MODULE_CFLAG += -DIN_ATHTAG
endif

ifeq (TRUE, $(IN_PKTEDIT))
  MODULE_CFLAG += -DIN_PKTEDIT
endif

ifeq (TRUE, $(IN_SAMPL))
  MODULE_CFLAG += -DIN_SAMPL
endif

ifeq (TRUE, $(IN_PON))
  MODULE_CFLAG += -DIN_PON
endif

ifeq (TRUE, $(IN_PON_PM))
  MODULE_CFLAG += -DIN_PON_PM
endif

ifeq (TRUE, $(IN_CROSSCHIP))
  MODULE_CFLAG += -DIN_CROSSCHIP
endif

ifeq (TRUE, $(IN_IPMC))
  MODULE_CFLAG += -DIN_IPMC
endif

ifneq (TRUE, $(FAL))
  MODULE_CFLAG += -DHSL_STANDALONG
endif

ifeq (TRUE, $(UK_IF))
  MODULE_CFLAG += -DUK_IF
endif

#ifdef UK_NL_PROT
  MODULE_CFLAG += -DUK_NL_PROT=$(UK_NL_PROT)
#endif

#ifdef UK_MINOR_DEV
  MODULE_CFLAG += -DUK_MINOR_DEV=$(UK_MINOR_DEV)
#endif

ifeq (TRUE, $(API_LOCK))
  MODULE_CFLAG += -DAPI_LOCK
  MODULE_CFLAG += -DSW_API_LOCK
endif

ifeq (TRUE, $(REG_ACCESS_SPEEDUP))
  MODULE_CFLAG += -DREG_ACCESS_SPEEDUP
endif

ifeq (TRUE, $(IN_NETLINK))
  MODULE_CFLAG += -DIN_NETLINK
endif

ifeq (TRUE, $(DEBUG_ON))
  MODULE_CFLAG += -g
endif

MODULE_CFLAG += $(OPT_FLAG) -Wall -DVERSION=\"$(VERSION)\" -DBUILD_DATE=\"$(BUILD_DATE)\" -DOS=\"$(OS)\" -D"KBUILD_STR(s)=\#s"

#############################################
#            include header path
#############################################
MODULE_INC += -I$(PRJ_PATH)/include \
                   -I$(PRJ_PATH)/include/common \
                   -I$(PRJ_PATH)/include/api \
                   -I$(PRJ_PATH)/include/fal \
                   -I$(PRJ_PATH)/include/ref \
                   -I$(PRJ_PATH)/include/adpt \
                   -I$(PRJ_PATH)/include/hsl \
                   -I$(PRJ_PATH)/include/hsl/phy \
                   -I$(PRJ_PATH)/include/sal/os \
                   -I$(PRJ_PATH)/include/sal/os/linux \
                   -I$(PRJ_PATH)/include/sal/sd \
                   -I$(PRJ_PATH)/include/sal/sd/linux/uk_interface \
                   -I$(PRJ_PATH)/include/init \
                   -I$(PRJ_PATH)/include/hsl/hppe \
                   -I$(PRJ_PATH)/include/adpt/hppe \
                   -I$(PRJ_PATH)/include/hsl/appe \
                   -I$(PRJ_PATH)/include/adpt/appe

ifneq (,$(findstring ISISC, $(SUPPORT_CHIP)))
  MODULE_INC   += -I$(PRJ_PATH)/include/hsl/isisc
  MODULE_CFLAG += -DISISC
endif

ifneq (,$(findstring MPPE, $(SUPPORT_CHIP)))
  MODULE_INC   += -I$(PRJ_PATH)/include/hsl/mppe
  MODULE_INC   += -I$(PRJ_PATH)/include/adpt/mppe
  MODULE_CFLAG += -DMPPE
endif

ifneq (,$(findstring MRPPE, $(SUPPORT_CHIP)))
  MODULE_INC   += -I$(PRJ_PATH)/include/hsl/mrppe
  MODULE_INC   += -I$(PRJ_PATH)/include/adpt/mrppe
  MODULE_CFLAG += -DMRPPE
endif

ifneq (,$(findstring HMSPPE, $(SUPPORT_CHIP)))
  MODULE_INC   += -I$(PRJ_PATH)/include/hsl/hmsppe
  MODULE_INC   += -I$(PRJ_PATH)/include/adpt/hmsppe
  MODULE_CFLAG += -DHMSPPE
endif

ifneq (,$(findstring JHPPE, $(SUPPORT_CHIP)))
  MODULE_INC   += -I$(PRJ_PATH)/include/hsl/hmsppe
  MODULE_INC   += -I$(PRJ_PATH)/include/adpt/hmsppe
  MODULE_INC   += -I$(PRJ_PATH)/include/hsl/jhppe
  MODULE_INC   += -I$(PRJ_PATH)/include/adpt/jhppe
  MODULE_CFLAG += -DJHPPE
endif

ifneq (,$(findstring MHT, $(SUPPORT_CHIP)))
  MODULE_INC   += -I$(PRJ_PATH)/include/hsl/mht
  MODULE_CFLAG += -DMHT
endif

ifneq (,$(findstring HTTPPE, $(SUPPORT_CHIP)))
  MODULE_INC   += -I$(PRJ_PATH)/include/adpt/httppe
  MODULE_INC   += -I$(PRJ_PATH)/include/hsl/httppe
  MODULE_CFLAG += -DHTTPPE
endif

ifeq (TRUE, $(IN_SFP))
  MODULE_INC   += -I$(PRJ_PATH)/include/hsl/sfp
  MODULE_INC   += -I$(PRJ_PATH)/include/adpt/sfp
  MODULE_CFLAG += -DIN_SFP
endif


#############################################
#              define cflags
#############################################
#auto_insert_flag
# check for GCC version
ifeq (4, $(GCC_VER))
  MODULE_CFLAG += -DGCCV4
endif

ifeq (KSLIB, $(MODULE_TYPE))

  MODULE_INC += -I$(PRJ_PATH)/include/shell_lib
  ifndef TARGET_NAME
    TARGET_NAME=arm-openwrt-linux-$(TARGET_SUFFIX)
  endif

  ifeq ($(CONFIG_KASAN_INLINE),y)
        CALL_THRESHOLD=10000
  else
        CALL_THRESHOLD=0
  endif
  ifneq ($(CONFIG_KASAN_SHADOW_OFFSET),)
        SHADOW_OFFSET=$(CONFIG_KASAN_SHADOW_OFFSET)
  else
        SHADOW_OFFSET=0xdfffff9000000000
  endif
  KASAN_OPTION=-fsanitize=kernel-address -fasan-shadow-offset=$(SHADOW_OFFSET) \
               --param asan-stack=1 --param asan-globals=1 \
               --param asan-instrumentation-with-call-threshold=$(CALL_THRESHOLD)

  ifeq ($(CONFIG_KASAN_SW_TAGS), y)
      KASAN_SHADOW_SCALE_SHIFT := 4
  else
      KASAN_SHADOW_SCALE_SHIFT := 3
  endif

  ifeq ($(OS_VER),$(filter 5_4 6_1 6_6, $(OS_VER)))
      ifeq ($(ARCH), arm64)
          KASAN_OPTION += -DKASAN_SHADOW_SCALE_SHIFT=$(KASAN_SHADOW_SCALE_SHIFT)
       endif
  endif
  ifeq ($(CONFIG_KASAN),y)
      #MODULE_CFLAG += $(KASAN_OPTION)
  endif

  #to review
  ifeq ($(OS_VER),$(filter 5_4 6_1 6_6, $(OS_VER)))
                MODULE_CFLAG += -DKVER34
                MODULE_CFLAG += -DKVER32
            MODULE_CFLAG += -DLNX26_22
  endif

  MODULE_CFLAG += -D__KERNEL__ -DKERNEL_MODULE 

endif

ifeq (SHELL, $(MODULE_TYPE))
    MODULE_CFLAG += -DKVER24

    ifeq (TRUE, $(KERNEL_MODE))
      MODULE_CFLAG += -static
    else
      MODULE_CFLAG += -static -DUSER_MODE
    endif
endif

ifneq (TRUE, $(KERNEL_MODE))
  ifneq (SHELL, $(MODULE_TYPE))
    MODULE_CFLAG +=  -DUSER_MODE
  endif
endif

ifeq ($(CONFIG_CPU_BIG_ENDIAN), y)
    MODULE_CFLAG +=  -mbig-endian
else
    MODULE_CFLAG +=  -mlittle-endian
endif

#############################################
#            combine all cflags
#############################################
ccflags-y += ${MODULE_INC} ${MODULE_CFLAG} -DFALLTHROUGH -Werror -Wall
