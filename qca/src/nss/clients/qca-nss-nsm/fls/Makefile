#
# Flow stats Makefile
#

ifeq ($(FLS_LITE_ENABLE), y)
obj-m += qca-nss-fls-lite.o
qca-nss-fls-lite-y := fls_debug.o fls_init.o fls_flow.o fls_chardev.o
ccflags-y += -DFLS_LITE_ENABLE
else
obj-m += qca-nss-fls.o
qca-nss-fls-y := fls_debug.o fls_init.o fls_flow.o fls_chardev.o fls_rfs.o fls_conn.o fls_def_sensor.o fls_sensor_manager.o fls_stats.o
endif

ccflags-$(FLS_ECM_CLASSIFIER_EMESH_ENABLE) += -DFLS_ECM_CLASSIFIER_EMESH_ENABLE

ccflags-y += -Werror -Wall -Iexports/
