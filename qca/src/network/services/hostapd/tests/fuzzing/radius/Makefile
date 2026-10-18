ALL=radius
include ../rules.include

CFLAGS += -DCONFIG_IPV6

LIBS += $(SRC)/common/libcommon.a
LIBS += $(SRC)/crypto/libcrypto.a
LIBS += $(SRC)/utils/libutils.a

ELIBS += $(SRC)/crypto/libcrypto.a

OBJS += $(SRC)/radius/radius.o

OBJS += radius.o

_OBJS_VAR := OBJS
include ../../../src/objs.mk

_OBJS_VAR := LIBS
include ../../../src/objs.mk

_OBJS_VAR := ELIBS
include ../../../src/objs.mk

radius: $(OBJS) $(LIBS)
	$(LDO) $(LDFLAGS) -o $@ $^ $(LIBS) $(ELIBS)

clean: common-clean
	rm -f radius *~ *.o *.d ../*~ ../*.o ../*.d
