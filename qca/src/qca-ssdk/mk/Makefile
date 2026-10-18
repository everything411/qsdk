ifndef PRJ_PATH
  MK_PATH=$(shell pwd)
  PRJ_PATH=${MK_PATH}/../

  export MK_PATH
  export PRJ_PATH
endif


#keep this sequence to include
include $(MK_PATH)/feature.mk
include $(MK_PATH)/chip.mk
include $(MK_PATH)/cflags.mk
include $(MK_PATH)/obj.mk


obj-m :=  qca-ssdk.o
qca-ssdk-objs := ${SSDK_OBJ}


####################################################################
# 			LNX Modules-Style Makefile
####################################################################
modules:
	make -C $(SYS_PATH) M=$(MK_PATH) -f Makefile $(LNX_MAKEOPTS)  modules
	cp Module.symvers ../Module.symvers;
	mkdir -p ../build/bin
	cp qca-ssdk.ko ../build/bin/
	@echo "---Build [SSDK-$(VERSION)] at $(BUILD_DATE) finished."

clean:
	@echo "---Clean [SSDK-$(VERSION)] finished."
