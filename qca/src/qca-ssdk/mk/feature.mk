BUILD_DATE=$(shell date -u  +%F-%T)
VERSION=3.1.0

#############################################
#	SDK Generic Setting	            #
#############################################
ifeq ($(SWCONFIG_FEATURE), disable)
	SWCONFIG=FALSE
else
	SWCONFIG=TRUE
endif

KERNEL_MODE=TRUE

#compatiable with OpenWRT
ifeq ($(SWITCH_SSDK_MODE),user)
	KERNEL_MODE=FLASE
endif

#FAL=FALSE or not define FAL, FAL will not be included in SSDK
FAL=TRUE

#############################################
#	SDK OS Specfic Setting		    #
#############################################
#UK_IF=FALSE or not define UK_IF, UK_IF will not be included in SSDK
#when UK_IF=TRUE one of UK_NETLINK,UK_IOCTL must be defined as TRUE
UK_IF=TRUE
#UK_IOCTL=TRUE define user-kernel space communication based on ioctl
UK_IOCTL=TRUE
UK_MINOR_DEV=254

#API_LOCK=FALSE or not define API_LOCK, API_LOCK will not be included in SSDK
API_LOCK=FALSE

#REG_ACCESS_SPEEDUP=FALSE or not define REG_ACCESS_SPEEDUP, REG_ACCESS_SPEEDUP will not be enabled, now only ISIS supports
REG_ACCESS_SPEEDUP=FALSE


#############################################
#	All Supported Features	    	    #
#############################################
#ACL FDB IGMP LEAKY LED MIB MIRROR MISC PORTCONTROL PORTVLAN QOS RATE STP VLAN
#IN_X=FALSE or not define IN_X, X will not be included in SSDK
IN_ACL=TRUE
IN_FDB=TRUE
IN_IGMP=TRUE
IN_LEAKY=TRUE
IN_LED=TRUE
IN_MIB=TRUE
IN_MIRROR=TRUE
IN_MISC=TRUE
IN_PORTCONTROL=TRUE
IN_PORTVLAN=TRUE
IN_QOS=TRUE
IN_RATE=TRUE
IN_STP=TRUE
IN_VLAN=TRUE
IN_REDUCED_ACL=FALSE
IN_COSMAP=TRUE
IN_IP=TRUE
IN_NAT=TRUE
IN_TRUNK=TRUE
IN_SEC=TRUE
IN_PPPOE=TRUE

ifeq ($(HNAT_FEATURE), enable)
	IN_NAT_HELPER=TRUE
else
	IN_NAT_HELPER=FALSE
endif

ifeq ($(RFS_FEATURE), enable)
	IN_RFS=TRUE
else
	IN_RFS=FALSE
endif

IN_INTERFACECONTROL=TRUE
IN_MACBLOCK=FALSE

#############################################
#        Platform Special Features          #
#############################################
ifeq ($(SoC),$(filter $(SoC),ipq53xx ipq54xx ipq95xx ipq807x ipq60xx))
	PTP_FEATURE=enable
endif

ifeq ($(SoC),$(filter $(SoC),ipq54xx ipq53xx ipq95xx ipq60xx))
	MHT_ENABLE=enable
endif

ifeq ($(SoC),$(filter $(SoC),ipq54xx ipq53xx ipq807x ipq60xx ipq50xx))
	ISISC_ENABLE=enable
endif

#############################################
# PHY CHIP Features According To Switch     #
#############################################
ifeq (APPE, $(CHIP_TYPE))
	IN_AQUANTIA_PHY=TRUE
	IN_SFP_PHY=TRUE
	IN_SFP=TRUE
else ifneq (, $(filter MRPPE MPPE, $(CHIP_TYPE)))
	ifeq ($(LOWMEM_FLASH), enable)
		IN_SFP_PHY=TRUE
		IN_SFP=TRUE
	else
		IN_AQUANTIA_PHY=TRUE
		IN_SFP_PHY=TRUE
		IN_SFP=TRUE
	endif
else
	IN_AQUANTIA_PHY=FALSE
	IN_SFP_PHY=FALSE
	IN_SFP=FALSE
endif

ifeq ($(SFE_FEATURE), enable)
	IN_SFE=TRUE
else
	IN_SFE=FALSE
endif

ifeq ($(PTP_FEATURE), enable)
	IN_PTP=TRUE
else
	IN_PTP=FALSE
endif

#############################################
# SDK Features According To Switch Chip     #
#############################################
ifneq (, $(filter MRPPE MPPE APPE HPPE CPPE ALL_CHIP, $(CHIP_TYPE)))
	IN_FLOW=TRUE
	IN_RSS_HASH=TRUE
	IN_QM=TRUE
	IN_VSI=TRUE
	IN_CTRLPKT=TRUE
	IN_SERVCODE=TRUE
	IN_BM=TRUE
	IN_SHAPER=TRUE
	IN_POLICER=TRUE
	IN_UNIPHY=TRUE
	IN_NETLINK=TRUE
endif

ifneq (, $(filter MP, $(CHIP_TYPE)))
	IN_UNIPHY=TRUE
endif

#############################################
# 	MINI SSDK Features Selection        #
#############################################
ifeq ($(MINI_SSDK), enable)
	IN_FDB_MINI=TRUE
	IN_MISC_MINI=TRUE
	IN_PORTCONTROL_MINI=TRUE
	IN_QOS_MINI=TRUE
	IN_COSMAP_MINI=TRUE
	IN_PORTVLAN_MINI=TRUE
	IN_VLAN_MINI=TRUE
	IN_VSI_MINI=TRUE
	IN_BM_MINI=TRUE
	IN_SHAPER_MINI=TRUE
	IN_POLICER_MINI=TRUE
	IN_FLOW_MINI=TRUE
	IN_QM_MINI=TRUE
	IN_UNIPHY_MINI=TRUE
	IN_IP_MINI=TRUE
	IN_SEC_MINI=TRUE
	IN_SFP=FALSE
	IN_PTP=FALSE

	#disable modules for MINI HPPE/CPPE/APPE/MPPE/MRPPE
	ifneq (, $(filter MRPPE MPPE APPE HPPE CPPE, $(CHIP_TYPE)))
		IN_NAT=FALSE
		IN_COSMAP=FALSE
		IN_RATE=FALSE
		IN_IGMP=FALSE
		IN_LEAKY=FALSE
		IN_LED=FALSE
		IN_INTERFACECONTROL=FALSE
	endif

	ifeq ($(MHT_ENABLE), enable)
		IN_INTERFACECONTROL=TRUE
		IN_NAT=TRUE
		IN_IGMP=TRUE
	endif

	ifeq ($(ISISC_ENABLE), enable)
		IN_IGMP=TRUE
	endif

	ifeq ($(HNAT_FEATURE), enable)
		IN_MISC_MINI=FALSE
		IN_PORTVLAN_MINI=FALSE
		IN_IP_MINI=FALSE
		IN_MISC=TRUE
		IN_PORTVLAN=TRUE
		IN_IP=TRUE
		IN_NAT=TRUE
	endif
endif

#############################################
# SDK Features According To Specfic Switch  #
#############################################
ifeq (MP, $(CHIP_TYPE))
	ifeq (disable, $(ISISC_ENABLE))
		IN_ACL=FALSE
		IN_FDB=FALSE
		IN_IGMP=FALSE
		IN_LEAKY=FALSE
		IN_LED=FALSE
		IN_MIRROR=FALSE
		IN_MISC=FALSE
		IN_PORTVLAN=FALSE
		IN_QOS=FALSE
		IN_RATE=FALSE
		IN_STP=FALSE
		IN_VLAN=FALSE
		IN_REDUCED_ACL=FALSE
		IN_COSMAP=FALSE
		IN_IP=FALSE
		IN_NAT=FALSE
		IN_FLOW=FALSE
		IN_TRUNK=FALSE
		IN_RSS_HASH=FALSE
		IN_SEC=FALSE
		IN_QM=FALSE
		IN_PPPOE=FALSE
		IN_VSI=FALSE
		IN_SERVCODE=FALSE
		IN_BM=FALSE
		IN_SHAPER=FALSE
		IN_POLICER=FALSE
	endif
	IN_CTRLPKT=TRUE
endif

ifneq (, $(filter MRPPE MPPE APPE , $(CHIP_TYPE)))
	IN_VPORT=TRUE
	IN_TUNNEL=TRUE
	IN_VXLAN=TRUE
	IN_GENEVE=TRUE
	IN_TUNNEL_PROGRAM=TRUE
	IN_MAPT=TRUE
	ifeq ($(MINI_SSDK), enable)
		IN_TUNNEL=FALSE
		IN_VXLAN=FALSE
		IN_GENEVE=FALSE
		IN_TUNNEL_PROGRAM=FALSE
		IN_MAPT=FALSE
	endif
endif

ifneq (, $(filter MRPPE MPPE , $(CHIP_TYPE)))
	IN_ATHTAG=TRUE
endif

ifneq (, $(filter MRPPE, $(CHIP_TYPE)))
	IN_PKTEDIT=TRUE
endif
#auto_insert_flag
