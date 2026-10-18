TEST_OBJ = $(TEST).o
_OBJS_VAR := TEST_OBJ
include ../src/objs.mk

$(TEST): $(TEST_OBJ) $(OBJS_$(TEST)) $(LIBS)
	$(LDO) $(LDFLAGS) -o $@ $^ $(LLIBS) $(CFLAGS_$(TEST))
