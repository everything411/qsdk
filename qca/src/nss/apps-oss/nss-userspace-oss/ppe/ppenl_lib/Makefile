MKDIR = @mkdir -p $(@D)
SRCPATH = ./
OBJPATH = obj

BINARY = $(OBJPATH)/libnl-ppe.so
SOURCES = $(wildcard $(SRCPATH)/*.c)
OBJECTS = $(SOURCES:$(SRCPATH)/%.c=$(OBJPATH)/%.o)
HEADERS = $(wildcard $(SRCPATH)/*.h)

INCLUDE += -I./include
EXTRA_CFLAGS = -Wall -Werror -fPIC -Wl,-z,relro -Wl,-z,now

ifneq ($(filter $(SoC),ipq96xx ipq52xx),)
EXTRA_CFLAGS += -DNSS_PPE_DSCP_FEATURE
EXTRA_CFLAGS += -DNSS_PPE_PM_FEATURE
EXTRA_CFLAGS += -DNSS_PPE_EXT_VLAN_FEATURE
EXTRA_CFLAGS += -DNSS_PPE_PORT_MGMT_FEATURE
EXTRA_CFLAGS += -DNSS_EDMA_DDRQ_FEATURE
EXTRA_CFLAGS += -DNSS_PPE_PORT_MIRROR_FEATURE
else
SOURCES := $(filter-out $(SRCPATH)/nss_ppenl_dscp_api.c, $(SOURCES))
SOURCES := $(filter-out $(SRCPATH)/nss_ppenl_pm_api.c, $(SOURCES))
SOURCES := $(filter-out $(SRCPATH)/nss_ppenl_vlan_api.c, $(SOURCES))
SOURCES := $(filter-out $(SRCPATH)/nss_ppenl_port_mgmt_api.c, $(SOURCES))
SOURCES := $(filter-out $(SRCPATH)/nss_ppenl_edma_ddrq_api.c, $(SOURCES))
SOURCES := $(filter-out $(SRCPATH)/nss_ppenl_port_mirror_api.c, $(SOURCES))
endif
ifneq ($(filter $(SoC),ipq52xx),)
EXTRA_CFLAGS += -DNSS_PPE_DOT1P_FEATURE
EXTRA_CFLAGS += -DNSS_PPE_GEMPORT_FEATURE
EXTRA_CFLAGS += -DNSS_PPE_PON_PORT_FEATURE
EXTRA_CFLAGS += -DNSS_PPE_PON_SUPPORT
EXTRA_CFLAGS += -DCONFIG_NSS_PPENL_PON_PORT
EXTRA_CFLAGS += -DNSS_PPE_MCAST_FEATURE
else
SOURCES := $(filter-out $(SRCPATH)/nss_ppenl_dot1p_api.c, $(SOURCES))
SOURCES := $(filter-out $(SRCPATH)/nss_ppenl_gemport_api.c, $(SOURCES))
SOURCES := $(filter-out $(SRCPATH)/nss_ppenl_mcast_api.c, $(SOURCES))
endif

ifeq ($(PPE_TUN_RPS_ENABLED), y)
EXTRA_CFLAGS += -DNSS_PPE_TUN_RPS_FEATURE
else
SOURCES := $(filter-out $(SRCPATH)/nss_ppenl_tun_rps_api.c, $(SOURCES))
endif

OBJECTS = $(SOURCES:$(SRCPATH)/%.c=$(OBJPATH)/%.o)

EXTRA_LDFLAGS = -pie -Wl,-z,relro -Wl,-z,now

all: release

release: $(BINARY)

$(OBJPATH)/%.o: $(SRCPATH)/%.c $(HEADERS)
	$(MKDIR)
	@echo [CC] $@
	@$(CC) -c $(CFLAGS) $(INCLUDE) $(EXTRA_CFLAGS) -o $@ $<

$(BINARY): $(OBJECTS)
	@echo $(BINARY)
	@echo [LD] $@
	@$(CC) -shared -o $@ $^ $(LDFLAGS) $(LDLIBS)

clean:
	@echo [Clean]
	@rm -f $(OBJECTS)
	@rm -f $(BINARY)
	@rm -rf $(OBJPATH)

.PHONY: clean
