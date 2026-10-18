obj-m += ath_debug.o
ath_debug-y += athdbg_core.o \
			athdbg_minidump.o \
			athdbg_debugfs.o \
			athdbg_mem.o \
			athdbg_qmi.o \
			athdbg_qdss.o \
			athdbg_mhi.o \
			athdbg_wmi_recording.o \
			athdbg_netlink.o
ath_debug-$(CPTCFG_ATHDEBUG_UIO_LOGGING) += athdbg_uio.o
