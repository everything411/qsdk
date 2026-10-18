MKDIR	= mkdir -p $(@D)
SRCPATH	= ./
OBJPATH	= obj
SRCDIR = ./

BINARY	= $(OBJPATH)/netfn
SOURCES = $(wildcard $(SRCDIR)/*.c)
HEADERS = $(wildcard $(SRCDIR)/*.h)
OBJECTS = $(SOURCES:$(SRCDIR)/%.c=$(OBJPATH)/%.o)

INCLUDE += -I./include
EXTRA_CFLAGS = -Wall -Werror -fpie -Wl,-z,relro -Wl,-z,now
EXTRA_LDFLAGS = -lnl-3 -lnl-genl-3 -pie -Wl,-z,relro -Wl,-z,now
LDLIBS += -ljansson

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
	@rmdir $(OBJPATH)

.PHONY: clean
