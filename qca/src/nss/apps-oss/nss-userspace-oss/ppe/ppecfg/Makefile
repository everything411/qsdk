MKDIR	= mkdir -p $(@D)
SRCPATH	= ./
OBJPATH	= obj
SRCDIR = ./

BINARY	= $(OBJPATH)/ppecfg
SOURCES = $(wildcard $(SRCDIR)/*.c)
HEADERS = $(wildcard $(SRCDIR)/*.h)

INCLUDE += -I./include
EXTRA_CFLAGS = -Wall -Werror -fpie -Wl,-z,relro -Wl,-z,now
EXTRA_CFLAGS += -DNSS_PPE_FEATURE_EXCEPTION_EDIT

ifneq ($(filter $(SoC),ipq96xx ipq52xx),)
EXTRA_CFLAGS += -DNSS_PPE_DSCP_FEATURE
EXTRA_CFLAGS += -DNSS_PPE_PM_FEATURE
EXTRA_CFLAGS += -DNSS_PPE_EXT_VLAN_FEATURE
EXTRA_CFLAGS += -DNSS_PPE_PORT_MGMT_FEATURE
EXTRA_CFLAGS += -DNSS_EDMA_DDRQ_FEATURE
EXTRA_CFLAGS += -DNSS_PPE_PORT_MIRROR_FEATURE
else
SOURCES := $(filter-out $(SRCDIR)/ppecfg_dscp.c, $(SOURCES))
SOURCES := $(filter-out $(SRCDIR)/ppecfg_pm.c, $(SOURCES))
SOURCES := $(filter-out $(SRCDIR)/ppecfg_vlan.c, $(SOURCES))
SOURCES := $(filter-out $(SRCDIR)/ppecfg_port_mgmt.c, $(SOURCES))
SOURCES := $(filter-out $(SRCDIR)/ppecfg_edma_ddrq.c, $(SOURCES))
SOURCES := $(filter-out $(SRCDIR)/ppecfg_port_mirror.c, $(SOURCES))
endif

ifneq ($(filter $(SoC),ipq52xx),)
EXTRA_CFLAGS += -DNSS_PPE_DOT1P_FEATURE
EXTRA_CFLAGS += -DNSS_PPE_GEMPORT_FEATURE
EXTRA_CFLAGS += -DNSS_PPE_PON_PORT_FEATURE
EXTRA_CFLAGS += -DNSS_PPE_PON_SUPPORT
EXTRA_CFLAGS += -DCONFIG_NSS_PPENL_PON_PORT
else
SOURCES := $(filter-out $(SRCDIR)/ppecfg_dot1p.c, $(SOURCES))
SOURCES := $(filter-out $(SRCDIR)/ppecfg_gemport.c, $(SOURCES))
endif

ifeq ($(PPE_TUN_RPS_ENABLED), y)
EXTRA_CFLAGS += -DNSS_PPE_TUN_RPS_FEATURE
else
SOURCES := $(filter-out $(SRCDIR)/ppecfg_tun_rps.c, $(SOURCES))
SOURCES := $(filter-out $(SRCDIR)/ppecfg_tun_rps_json_parser.c, $(SOURCES))
endif

OBJECTS = $(SOURCES:$(SRCDIR)/%.c=$(OBJPATH)/%.o)

EXTRA_LDFLAGS = -lnl-genl-3 -lnl-ppe -pie -Wl,-z,relro -Wl,-z,now
LDLIBS  = -L../ppecfg_lib/obj
LDLIBS  += -ljson-c

all: release

release: $(BINARY)


$(OBJPATH)/%.o: $(SRCPATH)/%.c $(HEADERS)
	$(MKDIR)
	@echo [CC] $@
	@$(CC) -c $(CFLAGS) $(INCLUDE) $(EXTRA_CFLAGS) -o $@ $<

$(BINARY): $(OBJECTS)
	@echo $(BINARY)
	@echo [LD] $@
	@$(CC) -o $@ $^ $(LDFLAGS) $(EXTRA_LDFLAGS) $(LDLIBS)
clean:
	@echo [Clean]
	@rm -f $(OBJECTS)
	@rm -f $(BINARY)
	@rm -rf $(OBJPATH)
.PHONY: clean
