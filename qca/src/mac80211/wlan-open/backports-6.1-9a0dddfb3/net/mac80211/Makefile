# SPDX-License-Identifier: GPL-2.0
obj-$(CPTCFG_MAC80211) += mac80211.o

# mac80211 objects
mac80211-y := \
	main.o status.o \
	driver-ops.o \
	sta_info.o \
	wep.o \
	aead_api.o \
	wpa.o \
	scan.o offchannel.o \
	ht.o agg-tx.o agg-rx.o \
	vht.o \
	he.o \
	s1g.o \
	ibss.o \
	iface.o \
	link.o \
	rate.o \
	michael.o \
	tkip.o \
	aes_cmac.o \
	aes_gmac.o \
	fils_aead.o \
	cfg.o \
	ethtool.o \
	rx.o \
	spectmgmt.o \
	tx.o \
	key.o \
	util.o parse.o \
	wme.o \
	chan.o \
	trace.o mlme.o \
	tdls.o \
	ocb.o \
	airtime.o \
	eht.o uhr.o smd.o

mac80211-$(CPTCFG_MAC80211_LEDS) += led.o
mac80211-$(CPTCFG_MAC80211_DEBUGFS) += \
	debugfs.o \
	debugfs_sta.o \
	debugfs_netdev.o \
	debugfs_key.o

# Add wlan-open-extns/subsys/src to the include path so that debugfs_netdev.c
# can resolve debugfs_bigtk.h (declared there, compiled via qcn_extns/).
ccflags-$(CPTCFG_MAC80211_DEBUGFS) += -I$(src)/../../../../../wlan-open-extns/subsys/src

mac80211-$(CPTCFG_MAC80211_MESH) += \
	mesh.o \
	mesh_pathtbl.o \
	mesh_plink.o \
	mesh_hwmp.o \
	mesh_sync.o \
	mesh_ps.o

mac80211-$(CONFIG_PM) += pm.o

CFLAGS_trace.o := -I$(src)

rc80211_minstrel-y := \
	rc80211_minstrel_ht.o

rc80211_minstrel-$(CPTCFG_MAC80211_DEBUGFS) += \
	rc80211_minstrel_ht_debugfs.o

mac80211-$(CPTCFG_MAC80211_RC_MINSTREL) += $(rc80211_minstrel-y)

mac80211-$(CPTCFG_QCN_EXTN) += \
	qcn_extns/bw240MHz.o \
	qcn_extns/driver-ops_extn.o \
	qcn_extns/iface_extn.o \
	qcn_extns/debugfs_bigtk.o \
	qcn_extns/debugfs_gtk.o \
	qcn_extns/debugfs_ptk.o \
	qcn_extns/debugfs_igtk.o \
	qcn_extns/debugfs_cigtk.o \
	qcn_extns/notify.o \
	qcn_extns/dfs_extn.o

obj-y += tests/

mac80211-y += wbrf.o

ccflags-y += -DDEBUG
