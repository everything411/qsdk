MKDIR	= mkdir -p $(@D)
SRCPATH	= ./
OBJPATH	= obj
SRCDIR = ./

BINARY	= $(OBJPATH)/ppecfg
SOURCES = $(wildcard $(SRCDIR)/*.c)
HEADERS = $(wildcard $(SRCDIR)/*.h)
OBJECTS = $(SOURCES:$(SRCDIR)/%.c=$(OBJPATH)/%.o)

INCLUDE += -I./include
EXTRA_CFLAGS = -Wall -Werror -fpie -Wl,-z,relro -Wl,-z,now
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
