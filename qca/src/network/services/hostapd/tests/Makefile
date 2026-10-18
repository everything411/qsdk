RUN_TESTS= \
	test-list \
	test-md4 test-rc4 test-sha1 test-sha256 \
	test-milenage test-aes \
	test-crypto_module \
	test-bss

ALL=$(RUN_TESTS) test-base64 test-https test-https_server

include ../src/build.rules

ifdef LIBFUZZER
CC=clang
CFLAGS = -MMD -O2 -Wall -g
CFLAGS += -fsanitize=fuzzer,address,signed-integer-overflow,unsigned-integer-overflow
CFLAGS += -DTEST_LIBFUZZER
LDFLAGS += -fsanitize=fuzzer,address,signed-integer-overflow,unsigned-integer-overflow
TEST_FUZZ=y
endif

ifdef TEST_FUZZ
CFLAGS += -DCONFIG_NO_RANDOM_POOL
CFLAGS += -DTEST_FUZZ
endif

CFLAGS += -DCONFIG_IEEE80211R_AP
CFLAGS += -DCONFIG_IEEE80211R
CFLAGS += -DCONFIG_TDLS

# test-crypto_module
CFLAGS += -DCONFIG_MODULE_TESTS
CFLAGS += -DCONFIG_DPP
#CFLAGS += -DCONFIG_DPP2
#CFLAGS += -DCONFIG_DPP3
CFLAGS += -DCONFIG_ECC
CFLAGS += -DCONFIG_HMAC_SHA256_KDF
CFLAGS += -DCONFIG_HMAC_SHA384_KDF
CFLAGS += -DCONFIG_MESH
CFLAGS += -DCONFIG_SHA256
CFLAGS += -DCONFIG_SHA384
CFLAGS += -DEAP_PSK
CFLAGS += -DEAP_FAST

CFLAGS += -I../src
CFLAGS += -I../src/utils

SLIBS = ../src/utils/libutils.a

DLIBS = ../src/tls/libtls.a \
	../src/crypto/libcrypto.a

_OBJS_VAR := LLIBS
include ../src/objs.mk
_OBJS_VAR := SLIBS
include ../src/objs.mk
_OBJS_VAR := DLIBS
include ../src/objs.mk

LIBS = $(SLIBS) $(DLIBS)
LLIBS = -Wl,--start-group $(DLIBS) -Wl,--end-group $(SLIBS)

ifeq ($(CONFIG_TLS),mbedtls)
CFLAGS += -DCONFIG_TLS_MBEDTLS
LLIBS += -lmbedtls -lmbedx509 -lmbedcrypto
else
ifeq ($(CONFIG_TLS),openssl)
CFLAGS += -DCONFIG_TLS_OPENSSL
LLIBS += -lssl -lcrypto
else
ifeq ($(CONFIG_TLS),gnutls)
CFLAGS += -DCONFIG_TLS_GNUTLS
LLIBS += -lgnutls -lgpg-error -lgcrypt
else
ifeq ($(CONFIG_TLS),wolfssl)
CFLAGS += -DCONFIG_TLS_WOLFSSL
LLIBS += -lwolfssl -lm
else
CFLAGS += -DCONFIG_TLS_INTERNAL
CFLAGS += -DCONFIG_TLS_INTERNAL_SERVER
ALL += test-rsa-sig-ver
ALL += test-x509v3
clean-config_tls_internal:
	rm -f test_x509v3_nist.out.*
	rm -f test_x509v3_nist2.out.*
endif
endif
endif
endif

# glibc < 2.17 needs -lrt for clock_gettime()
LLIBS += -lrt

TEST=test-aes
include test.mk

TEST=test-crypto_module
include test.mk

TEST=test-base64
include test.mk

TEST=test-https
include test.mk

TEST=test-https_server
include test.mk

TEST=test-list
include test.mk

TEST=test-md4
include test.mk

TEST=test-milenage
include test.mk

TEST=test-rc4
include test.mk

TEST=test-rsa-sig-ver
include test.mk

TEST=test-sha1
include test.mk

TEST=test-sha256
include test.mk

TEST=test-x509v3
include test.mk


# We could cut this down more by enabling fewer options (above)
ELIBS += $(SRC)/crypto/libcrypto.a
ELIBS += $(SRC)/tls/libtls.a

WPAS_SRC=../wpa_supplicant
SRC=../src
WPA_LIBS = $(SRC)/common/libcommon.a

CFLAGS += -I$(SRC) -I$(SRC)/utils -I$(WPAS_SRC)

CFLAGS += -DCONFIG_DRIVER_NONE
CFLAGS += -DIEEE8021X_EAPOL

WPA_OBJS = $(WPAS_SRC)/bssid_ignore.o
WPA_OBJS += $(WPAS_SRC)/bss.o
WPA_OBJS += $(WPAS_SRC)/config.o
WPA_OBJS += $(WPAS_SRC)/config_file.o
WPA_OBJS += $(WPAS_SRC)/eap_register.o
WPA_OBJS += $(WPAS_SRC)/events.o
WPA_OBJS += $(WPAS_SRC)/notify.o
WPA_OBJS += $(WPAS_SRC)/offchannel.o
WPA_OBJS += $(WPAS_SRC)/op_classes.o
WPA_OBJS += $(WPAS_SRC)/robust_av.o
WPA_OBJS += $(WPAS_SRC)/rrm.o
WPA_OBJS += $(WPAS_SRC)/scan.o
WPA_OBJS += $(WPAS_SRC)/wmm_ac.o
WPA_OBJS += $(WPAS_SRC)/wpa_supplicant.o
WPA_OBJS += $(WPAS_SRC)/wpas_glue.o
WPA_OBJS += $(SRC)/rsn_supp/wpa.o
WPA_OBJS += $(SRC)/rsn_supp/wpa_ft.o
WPA_OBJS += $(SRC)/rsn_supp/wpa_ie.o
WPA_OBJS += $(SRC)/rsn_supp/tdls.o
WPA_OBJS += $(SRC)/rsn_supp/preauth.o
WPA_OBJS += $(SRC)/rsn_supp/pmksa_cache.o
WPA_OBJS += $(SRC)/eapol_supp/eapol_supp_sm.o
WPA_OBJS += $(SRC)/eap_common/eap_common.o
WPA_OBJS += $(SRC)/eap_peer/eap.o
WPA_OBJS += $(SRC)/eap_peer/eap_methods.o
WPA_OBJS += $(SRC)/drivers/driver_common.o
WPA_OBJS += $(SRC)/drivers/driver_none.o
WPA_OBJS += $(SRC)/drivers/drivers.o
WPA_OBJS += $(SRC)/l2_packet/l2_packet_none.o

_OBJS_VAR := WPA_OBJS
include ../src/objs.mk

_OBJS_VAR := WPA_LIBS
include ../src/objs.mk

_OBJS_VAR := ELIBS
include ../src/objs.mk

LIBS=$(SLIBS) $(DLIBS) $(WPA_LIBS) $(ELIBS)

TEST=test-bss
CFLAGS_test-bss=$(WPA_CFLAGS)
OBJS_test-bss=$(WPA_OBJS)
include test.mk

run-tests: $(ALL)
	@set -ex; for i in $(RUN_TESTS); do ./$$i; done
	@echo
	@echo All tests completed successfully.

clean: common-clean clean-config_tls_internal
	rm -f *~

.PHONY: run-tests clean-config_tls_internal
