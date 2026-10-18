###############################################################################
#                                 SSDK OBJS
###############################################################################
OBJ-COMMON  :=
OBJ-ISISC   :=
OBJ-HPPE    :=
OBJ-APPE    :=
OBJ-CPPE    :=
OBJ-MPPE    :=
OBJ-MRPPE    :=
OBJ-MHT     :=
OBJ-SCOMPHY :=

###############################################################################
#                                 IN_ACL
###############################################################################
ifeq (TRUE, $(IN_ACL))
OBJ-COMMON += src/fal/fal_acl.o src/ref/ref_acl.o
OBJ-ISISC  += src/hsl/isisc/isisc_acl.o src/hsl/isisc/isisc_acl_parse.o \
              src/hsl/isisc/isisc_multicast_acl.o
OBJ-HPPE   += src/adpt/hppe/adpt_hppe_acl.o src/hsl/hppe/hppe_acl.o
OBJ-APPE   += src/adpt/appe/adpt_appe_acl.o src/hsl/appe/appe_acl.o
endif

###############################################################################
#                                IN_FDB
###############################################################################
ifeq (TRUE, $(IN_FDB))
OBJ-COMMON += src/ref/ref_fdb.o src/fal/fal_fdb.o src/hsl/hppe/hppe_fdb.o
OBJ-ISISC  += src/hsl/isisc/isisc_fdb.o
OBJ-HPPE   += src/hsl/hppe/hppe_fdb.o src/adpt/hppe/adpt_hppe_fdb.o
OBJ-APPE   += src/adpt/appe/adpt_appe_fdb.o
endif

###############################################################################
#                                IN_IGMP
###############################################################################
ifeq (TRUE, $(IN_IGMP))
OBJ-COMMON += src/fal/fal_igmp.o
OBJ-ISISC  += src/hsl/isisc/isisc_igmp.o
endif

###############################################################################
#                                IN_IGMP
###############################################################################
ifeq (TRUE, $(IN_LEAKY))
OBJ-COMMON += src/fal/fal_leaky.o
OBJ-ISISC  += src/hsl/isisc/isisc_leaky.o
endif

###############################################################################
#                                IN_LED
###############################################################################
ifeq (TRUE, $(IN_LED))
OBJ-COMMON += src/fal/fal_led.o src/init/ssdk_led.o
OBJ-ISISC  += src/hsl/isisc/isisc_led.o
endif

###############################################################################
#                                IN_MIB
###############################################################################
ifeq (TRUE, $(IN_MIB))
OBJ-COMMON += src/fal/fal_mib.o src/ref/ref_mib.o
OBJ-ISISC  += src/hsl/isisc/isisc_mib.o
OBJ-HPPE   += src/adpt/hppe/adpt_hppe_mib.o src/hsl/hppe/hppe_mib.o \
              src/hsl/hppe/hppe_xgmacmib.o
OBJ-CPPE   += src/adpt/cppe/adpt_cppe_mib.o
endif

###############################################################################
#                               IN_MIRROR
###############################################################################
ifeq (TRUE, $(IN_MIRROR))
OBJ-COMMON += src/fal/fal_mirror.o
OBJ-ISISC  += src/hsl/isisc/isisc_mirror.o
OBJ-HPPE   += src/hsl/hppe/hppe_mirror.o src/adpt/hppe/adpt_hppe_mirror.o
endif

###############################################################################
#                              IN_MISC
###############################################################################
ifeq (TRUE, $(IN_MISC))
OBJ-COMMON += src/adpt/hppe/adpt_hppe_misc.o src/ref/ref_misc.o \
              src/fal/fal_misc.o
OBJ-ISISC  += src/hsl/isisc/isisc_misc.o
endif

###############################################################################
#                             IN_PORTCONTROL
###############################################################################
ifeq (TRUE, $(IN_PORTCONTROL))
OBJ-COMMON += src/fal/fal_port_ctrl.o src/ref/ref_port_ctrl.o
OBJ-ISISC  += src/hsl/isisc/isisc_port_ctrl.o
OBJ-HPPE   += src/adpt/hppe/adpt_hppe_portctrl.o src/hsl/hppe/hppe_portctrl.o \
              src/hsl/hppe/hppe_xgportctrl.o
OBJ-APPE   += src/adpt/appe/adpt_appe_portctrl.o
OBJ-CPPE   += src/hsl/cppe/cppe_portctrl.o src/adpt/cppe/adpt_cppe_portctrl.o
OBJ-MHT    += src/hsl/mht/mht_port_ctrl.o
endif

###############################################################################
#                            IN_PORTVLAN
###############################################################################
ifeq (TRUE, $(IN_PORTVLAN))
OBJ-COMMON += src/fal/fal_portvlan.o
OBJ-ISISC  += src/hsl/isisc/isisc_portvlan.o
OBJ-HPPE   += src/hsl/hppe/hppe_portvlan.o src/adpt/hppe/adpt_hppe_portvlan.o
OBJ-APPE   += src/hsl/appe/appe_portvlan.o src/adpt/appe/adpt_appe_portvlan.o
endif

###############################################################################
#                              IN_QOS
###############################################################################
ifeq (TRUE, $(IN_QOS))
OBJ-COMMON += src/fal/fal_qos.o
OBJ-ISISC  += src/hsl/isisc/isisc_qos.o
OBJ-HPPE   += src/hsl/hppe/hppe_qos.o src/adpt/hppe/adpt_hppe_qos.o
OBJ-CPPE   += src/hsl/cppe/cppe_qos.o src/adpt/cppe/adpt_cppe_qos.o
OBJ-APPE   += src/hsl/cppe/cppe_qos.o src/adpt/cppe/adpt_cppe_qos.o #to be fixed
endif

###############################################################################
#                             IN_RATE
###############################################################################
ifeq (TRUE, $(IN_RATE))
OBJ-COMMON += src/fal/fal_rate.o
OBJ-ISISC  += src/hsl/isisc/isisc_rate.o
endif

###############################################################################
#                             IN_STP
###############################################################################
ifeq (TRUE, $(IN_STP))
OBJ-COMMON += src/adpt/hppe/adpt_hppe_stp.o src/fal/fal_stp.o
OBJ-ISISC  += src/hsl/isisc/isisc_stp.o
OBJ-HPPE   += src/hsl/hppe/hppe_stp.o
endif

###############################################################################
#                             IN_VLAN
###############################################################################
ifeq (TRUE, $(IN_VLAN))
OBJ-COMMON += src/ref/ref_vlan.o src/fal/fal_vlan.o
OBJ-ISISC  += src/hsl/isisc/isisc_vlan.o
endif

###############################################################################
#                             IN_COSMAP
###############################################################################
ifeq (TRUE, $(IN_COSMAP))
OBJ-COMMON += src/fal/fal_cosmap.o
OBJ-ISISC  += src/hsl/isisc/isisc_cosmap.o
OBJ-MHT    += src/hsl/mht/mht_cosmap.o
endif

###############################################################################
#                              IN_IP
###############################################################################
ifeq (TRUE, $(IN_IP))
OBJ-COMMON  += src/fal/fal_ip.o
OBJ-ISISC   += src/hsl/isisc/isisc_ip.o
OBJ-MHT     += src/hsl/mht/mht_ip.o
OBJ-HPPE    += src/hsl/hppe/hppe_ip.o src/adpt/hppe/adpt_hppe_ip.o
endif

###############################################################################
#                             IN_FLOW
###############################################################################
ifeq (TRUE, $(IN_FLOW))
OBJ-COMMON  +=  src/fal/fal_flow.o
OBJ-HPPE    += src/hsl/hppe/hppe_flow.o src/adpt/hppe/adpt_hppe_flow.o
OBJ-CPPE    += src/adpt/cppe/adpt_cppe_flow.o
OBJ-APPE    += src/adpt/cppe/adpt_cppe_flow.o #to be fixed
endif

###############################################################################
#                              IN_NAT
###############################################################################
ifeq (TRUE, $(IN_NAT))
OBJ-COMMON  += src/fal/fal_nat.o
OBJ-ISISC   += src/hsl/isisc/isisc_nat.o
OBJ-MHT     += src/hsl/mht/mht_nat.o
endif

###############################################################################
#                              IN_TRUNK
###############################################################################
ifeq (TRUE, $(IN_TRUNK))
OBJ-COMMON  += src/fal/fal_trunk.o
OBJ-ISISC   += src/hsl/isisc/isisc_trunk.o
OBJ-HPPE    += src/hsl/hppe/hppe_trunk.o src/adpt/hppe/adpt_hppe_trunk.o
endif

###############################################################################
#                             IN_SEC
###############################################################################
ifeq (TRUE, $(IN_SEC))
OBJ-COMMON  += src/fal/fal_sec.o
OBJ-ISISC   += src/hsl/isisc/isisc_sec.o
OBJ-HPPE    += src/hsl/hppe/hppe_sec.o src/adpt/hppe/adpt_hppe_sec.o
OBJ-APPE    += src/hsl/appe/appe_sec.o src/adpt/appe/adpt_appe_sec.o
OBJ-MHT     += src/hsl/mht/mht_sec_ctrl.o
endif

###############################################################################
#                             IN_PPPOE
###############################################################################
ifeq (TRUE, $(IN_PPPOE))
OBJ-COMMON  += src/fal/fal_pppoe.o
OBJ-HPPE    += src/hsl/hppe/hppe_pppoe.o src/adpt/hppe/adpt_hppe_pppoe.o
OBJ-APPE    += src/hsl/appe/appe_pppoe.o src/adpt/appe/adpt_appe_pppoe.o
endif

###############################################################################
#                     IN_INTERFACECONTROL
###############################################################################
ifeq (TRUE, $(IN_INTERFACECONTROL))
OBJ-COMMON  += src/fal/fal_interface_ctrl.o
OBJ-ISISC   += src/hsl/isisc/isisc_interface_ctrl.o
OBJ-MHT     += src/hsl/mht/mht_interface_ctrl.o
endif

###############################################################################
#                         IN_RSS_HASH
###############################################################################
ifeq (TRUE, $(IN_RSS_HASH))
OBJ-COMMON  += src/fal/fal_rss_hash.o
OBJ-HPPE    += src/hsl/hppe/hppe_rss.o src/adpt/hppe/adpt_hppe_rss_hash.o
endif

###############################################################################
#                             IN_QM
###############################################################################
ifeq (TRUE, $(IN_QM))
OBJ-COMMON  += src/fal/fal_qm.o
OBJ-HPPE    += src/hsl/hppe/hppe_qm.o src/adpt/hppe/adpt_hppe_qm.o
OBJ-APPE    += src/hsl/appe/appe_qm.o src/adpt/appe/adpt_appe_qm.o
OBJ-CPPE    += src/adpt/cppe/adpt_cppe_qm.o
endif

###############################################################################
#                             IN_VSI
###############################################################################
ifeq (TRUE, $(IN_VSI))
OBJ-COMMON += src/fal/fal_vsi.o src/ref/ref_vsi.o
OBJ-HPPE    += src/hsl/hppe/hppe_vsi.o src/adpt/hppe/adpt_hppe_vsi.o
OBJ-APPE    += src/hsl/appe/appe_vsi.o src/adpt/appe/adpt_appe_vsi.o
endif

###############################################################################
#                             IN_CTRLPKT
###############################################################################
ifeq (TRUE, $(IN_CTRLPKT))
OBJ-COMMON  += src/fal/fal_ctrlpkt.o
OBJ-HPPE    += src/hsl/hppe/hppe_ctrlpkt.o src/adpt/hppe/adpt_hppe_ctrlpkt.o
OBJ-APPE    += src/adpt/appe/adpt_appe_ctrlpkt.o
endif

###############################################################################
#                              IN_SERVCODE
###############################################################################
ifeq (TRUE, $(IN_SERVCODE))
OBJ-COMMON  += src/fal/fal_servcode.o
OBJ-HPPE    += src/hsl/hppe/hppe_servcode.o src/adpt/hppe/adpt_hppe_servcode.o
OBJ-APPE    += src/hsl/appe/appe_servcode.o src/adpt/appe/adpt_appe_servcode.o
OBJ-MPPE    += src/hsl/mppe/mppe_servcode.o src/adpt/mppe/adpt_mppe_servcode.o
endif


###############################################################################
#                               IN_BM
###############################################################################
ifeq (TRUE, $(IN_BM))
OBJ-COMMON  += src/fal/fal_bm.o
OBJ-HPPE    += src/hsl/hppe/hppe_bm.o src/adpt/hppe/adpt_hppe_bm.o
endif


###############################################################################
#                              IN_SHAPER
###############################################################################
ifeq (TRUE, $(IN_SHAPER))
OBJ-COMMON  += src/fal/fal_shaper.o
OBJ-HPPE    += src/hsl/hppe/hppe_shaper.o src/adpt/hppe/adpt_hppe_shaper.o
OBJ-APPE    += src/hsl/appe/appe_shaper.o src/adpt/appe/adpt_appe_shaper.o
endif


###############################################################################
#                              IN_POLICER
###############################################################################
ifeq (TRUE, $(IN_POLICER))
OBJ-COMMON  += src/fal/fal_policer.o
OBJ-HPPE    += src/hsl/hppe/hppe_policer.o src/adpt/hppe/adpt_hppe_policer.o
OBJ-APPE    += src/hsl/appe/appe_policer.o src/adpt/appe/adpt_appe_policer.o
endif


###############################################################################
#                              IN_UNIPHY
###############################################################################
ifeq (TRUE, $(IN_UNIPHY))
OBJ-HPPE    += src/hsl/hppe/hppe_uniphy.o src/adpt/hppe/adpt_hppe_uniphy.o
OBJ-CPPE    += src/adpt/cppe/adpt_cppe_uniphy.o
endif

###############################################################################
#                             IN_NETLINK
###############################################################################
ifeq (TRUE, $(IN_NETLINK))
OBJ-COMMON  += src/init/ssdk_netlink.o
endif

###############################################################################
#                             IN_SFP
###############################################################################
ifeq (TRUE, $(IN_SFP))
OBJ-COMMON  += src/hsl/sfp/sfp_access.o src/hsl/sfp/sfp.o \
               src/adpt/sfp/adpt_sfp.o src/fal/fal_sfp.o
endif

###############################################################################
#                             IN_PTP
###############################################################################
ifeq (TRUE, $(IN_PTP))
OBJ-COMMON  += src/fal/fal_ptp.o
endif

###############################################################################
#                              IN_VPORT
###############################################################################
ifeq (TRUE, $(IN_VPORT))
OBJ-COMMON  += src/ref/ref_vport.o src/fal/fal_vport.o
OBJ-APPE    += src/adpt/appe/adpt_appe_vport.o
endif

###############################################################################
#                              IN_TUNNEL
###############################################################################
ifeq (TRUE, $(IN_TUNNEL))
OBJ-COMMON  += src/ref/ref_tunnel.o src/fal/fal_tunnel.o
OBJ-APPE    += src/hsl/appe/appe_tunnel.o src/hsl/appe/appe_tunnel_map.o \
               src/adpt/appe/adpt_appe_tunnel.o
endif

ifeq (TRUE, $(IN_TUNNEL_PROGRAM))
OBJ-COMMON  += src/fal/fal_tunnel_program.o
OBJ-APPE    += src/hsl/appe/appe_tunnel_program.o src/adpt/appe/adpt_appe_tunnel_program.o
endif

###############################################################################
#                              IN_VXLAN
###############################################################################
ifeq (TRUE, $(IN_VXLAN))
OBJ-COMMON  += src/fal/fal_vxlan.o
OBJ-APPE    +=src/hsl/appe/appe_vxlan.o src/adpt/appe/adpt_appe_vxlan.o
endif

###############################################################################
#                              IN_MAPT
###############################################################################
ifeq (TRUE, $(IN_MAPT))
OBJ-COMMON  += src/ref/ref_mapt.o src/fal/fal_mapt.o
OBJ-APPE    += src/adpt/appe/adpt_appe_mapt.o
endif

###############################################################################
#                             IN_GENEVE
###############################################################################
ifeq (TRUE, $(IN_GENEVE))
OBJ-COMMON  += src/fal/fal_geneve.o
OBJ-APPE    += src/hsl/appe/appe_geneve.o src/adpt/appe/adpt_appe_geneve.o
endif

###############################################################################
#                             IN_ATHTAG
###############################################################################
ifeq (TRUE, $(IN_ATHTAG))
OBJ-COMMON  += src/fal/fal_athtag.o src/ref/ref_athtag.o
OBJ-MPPE    += src/hsl/mppe/mppe_athtag.o src/adpt/mppe/adpt_mppe_athtag.o
endif

###############################################################################
#                             IN_PKTEDIT
###############################################################################
ifeq (TRUE, $(IN_PKTEDIT))
OBJ-COMMON  += src/fal/fal_pktedit.o src/ref/ref_pktedit.o
OBJ-MRPPE   += src/hsl/mrppe/mrppe_pktedit.o src/adpt/mrppe/adpt_mrppe_pktedit.o
endif

###############################################################################
#                                PHY
###############################################################################
OBJ-COMMON  += src/hsl/phy/hsl_phy.o

ifeq (TRUE, $(IN_AQUANTIA_PHY))
OBJ-COMMON  += src/hsl/phy/aquantia_phy.o src/hsl/phy/qcaphy_c45_common.o
endif

ifeq (TRUE, $(IN_SFP_PHY))
OBJ-COMMON  += src/hsl/phy/sfp_phy.o
endif

ifeq (TRUE, $(IN_PTP))
ifeq ($(CONFIG_PTP_1588_CLOCK), y)
OBJ-COMMON  += src/hsl/phy/hsl_ptp.o
endif
endif

###############################################################################
#                               IN_NAT_HELPER
###############################################################################
ifeq (TRUE, $(IN_NAT_HELPER))
OBJ-ISISC   += app/nathelper/linux/lib/nat_helper_hsl.o app/nathelper/linux/nat_helper.o \
               app/nathelper/linux/napt_acl.o app/nathelper/linux/napt_procfs.o \
               app/nathelper/linux/lib/nat_helper_dt.o app/nathelper/linux/nat_ipt_helper.o \
               app/nathelper/linux/napt_helper.o app/nathelper/linux/host_helper.o
endif

###############################################################################
#                                  INIT
###############################################################################
OBJ-COMMON  += src/init/ssdk_init.o src/init/ssdk_plat.o src/init/ssdk_interrupt.o \
               src/init/ssdk_dts.o src/init/ssdk_phy_i2c.o src/init/ssdk_clk.o
OBJ-ISISC   += src/hsl/isisc/isisc_init.o
OBJ-HPPE    += src/init/ssdk_hppe.o  src/hsl/hppe/hppe_init.o
OBJ-APPE    += src/init/ssdk_appe.o
OBJ-MHT     += src/hsl/mht/mht_init.o src/init/ssdk_mht.o src/init/ssdk_mht_clk.o \
               src/init/ssdk_mht_pinctrl.o
OBJ-SCOMPHY += src/init/ssdk_scomphy.o

###############################################################################
#                                 SHELL_LIB
###############################################################################
OBJ-COMMON  += src/shell_lib/shell.o src/shell_lib/shell_config.o \
              src/shell_lib/shell_io.o src/shell_lib/shell_sw.o

###############################################################################
#                                 COMMON
###############################################################################
OBJ-COMMON  += src/fal/fal_init.o src/fal/fal_reg_access.o src/adpt/adpt.o \
               src/ref/ref_uci.o src/hsl/hsl_dev.o src/hsl/hsl_api.o \
               src/hsl/hsl_port_prop.o src/util/util.o src/sal/sd/sd.o \
               src/sal/sd/linux/uk_interface/sw_api_ks_ioctl.o \
               src/api/api_access.o
OBJ-ISISC   += src/hsl/isisc/isisc_reg_access.o
OBJ-HPPE    += src/hsl/hppe/hppe_global.o src/hsl/hppe/hppe_reg_access.o
OBJ-APPE    += src/hsl/appe/appe_counter.o src/hsl/appe/appe_global.o \
               src/hsl/appe/appe_l2_vp.o
OBJ-CPPE    += src/hsl/cppe/cppe_loopback.o

###############################################################################
#                              Collect OBJ
###############################################################################
OBJ := $(OBJ-COMMON)

ifneq (,$(findstring ISISC, $(SUPPORT_CHIP)))
OBJ += $(OBJ-ISISC)
endif

ifneq (,$(findstring HPPE, $(SUPPORT_CHIP)))
OBJ += $(OBJ-HPPE)
endif

ifneq (,$(findstring APPE, $(SUPPORT_CHIP)))
OBJ += $(OBJ-APPE)
endif

ifneq (,$(findstring CPPE, $(SUPPORT_CHIP)))
OBJ += $(OBJ-CPPE)
endif

ifneq (,$(findstring MPPE, $(SUPPORT_CHIP)))
OBJ += $(OBJ-MPPE)
endif

ifneq (,$(findstring MRPPE, $(SUPPORT_CHIP)))
OBJ += $(OBJ-MRPPE)
endif

ifneq (,$(findstring MHT, $(SUPPORT_CHIP)))
OBJ += $(OBJ-MHT)
endif

ifneq (,$(findstring SCOMPHY, $(SUPPORT_CHIP)))
OBJ += $(OBJ-SCOMPHY)
endif

SSDK_OBJ=$(addprefix ../, $(OBJ))
export SSDK_OBJ

