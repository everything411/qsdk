
STAGING_DIR=$(HOME)/openwrt-19.07/staging_dir

ifeq ($(wildcard $(STAGING_DIR)/.*),)
STAGING_DIR=$(HOME)/openwrt_source/openwrt/staging_dir
endif

export STAGING_DIR

CC=$(STAGING_DIR)/toolchain-x86_64_gcc-7.5.0_musl/bin/x86_64-openwrt-linux-musl-gcc
CFLAGS+=-Wall -I$(STAGING_DIR)/target-x86_64_musl/usr/include -I.
LIB=$(STAGING_DIR)/target-x86_64_musl/usr/lib/libubox.a -lubox -lubus -lblobmsg_json -ljson-c -L$(STAGING_DIR)/target-x86_64_musl/usr/lib

#%.o : %.c
#	$(CC) -c $(CFLAGS) $(CPPFLAGS) $< -o $@

all: util_uci

util_uci: util_ubus.o util_uci.o
	$(CC) $(CFLAGS) -o util_uci util_ubus.o util_uci.o $(LIB)

clean:
	@rm -f util_uci *.o 2>/dev/null

dist: all
#	scp util_ubus_example 192.168.1.69:
	echo "scp util_uci"
	scp util_uci root@172.20.0.69:/root/util_uci
