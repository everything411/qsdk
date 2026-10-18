OBJS_c = main.o mld_chan.o mld_ctrl.o ucode.o ubus.o \
	$(STAGING_DIR)/usr/lib/libwpa_ctrl.a \
	$(WPA_PATH)/utils/uloop.o


CC ?= $(TARGET_CC)
ifeq ($(CC),)
CC = $(TOOLPREFIX)gcc
endif

CFLAGS += -MMD -O2 -Wall -Werror -g $(TARGET_CFLAGS)
LDFLAGS += $(TARGET_LDFLAGS)


ALL=rptr-mgr
.PHONY: all clean install

all: clean $(ALL)

install: all
	mkdir -p $(DESTDIR)$(BINDIR)
	for i in $(ALL); do cp $$i $(DESTDIR)$(BINDIR)/$$i; done


ifndef CONFIG_OS
CONFIG_OS=unix
endif

ifeq ($(CONFIG_OS), internal)
CFLAGS += -DOS_NO_C_LIB_DEFINES
endif

LDFLAGS += -lpthread -ldl
LDFLAGS += -rdynamic -lucode -lubox -lubus

ifdef CONFIG_WPA_TRACE
CFLAGS += -DWPA_TRACE
LDFLAGS += -L$(STAGING_DIR)/usr/lib
CFLAGS += -funwind-tables
ifdef CONFIG_WPA_TRACE_BFD
CFLAGS += -DWPA_TRACE_BFD
endif
endif

ifndef CONFIG_L2_PACKET
CONFIG_L2_PACKET=linux
endif

ifdef CONFIG_CTRL_IFACE
ifeq ($(CONFIG_CTRL_IFACE), y)
ifdef CONFIG_NATIVE_WINDOWS
CONFIG_CTRL_IFACE=named_pipe
else
CONFIG_CTRL_IFACE=unix
endif
endif
CFLAGS += -DCONFIG_CTRL_IFACE
ifeq ($(CONFIG_CTRL_IFACE), unix)
CFLAGS += -DCONFIG_CTRL_IFACE_UNIX
endif
ifeq ($(CONFIG_CTRL_IFACE), udp)
CFLAGS += -DCONFIG_CTRL_IFACE_UDP
endif
ifeq ($(CONFIG_CTRL_IFACE), named_pipe)
CFLAGS += -DCONFIG_CTRL_IFACE_NAMED_PIPE
endif
endif

 

LIBS_c += $(LIBS)

ifndef LDO
LDO=$(CC)
endif

 

rptr-mgr: $(OBJS_c)
	$(LDO) $(LDFLAGS) -o rptr-mgr $(OBJS_c) $(LIBS_c)

%.o: %.c
	$(CC) -c -o $@ $(CPPFLAGS) $(CFLAGS) $<

 

clean:
	rm -f core *~ *.o *.d eap_*.so $(ALL)

LOCAL_OBJS = main.o mld_chan.o mld_ctrl.o ucode.o ubus.o
-include $(LOCAL_OBJS:%.o=%.d)
