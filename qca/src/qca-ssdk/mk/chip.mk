ifeq (ISISC, $(CHIP_TYPE))
    SUPPORT_CHIP = ISISC
endif

ifeq (HPPE, $(CHIP_TYPE))
    SUPPORT_CHIP = HPPE
endif

ifeq (CPPE, $(CHIP_TYPE))
    SUPPORT_CHIP = HPPE CPPE
endif

ifeq (MP, $(CHIP_TYPE))
    SUPPORT_CHIP = SCOMPHY MP
endif

ifeq (APPE, $(CHIP_TYPE))
    SUPPORT_CHIP = HPPE APPE
endif

ifeq (MPPE, $(CHIP_TYPE))
    SUPPORT_CHIP = HPPE APPE MPPE
endif

ifeq (MRPPE, $(CHIP_TYPE))
    SUPPORT_CHIP = HPPE APPE MPPE MRPPE
endif

ifeq ($(ISISC_ENABLE), enable)
    SUPPORT_CHIP += ISISC
endif

ifeq ($(MHT_ENABLE), enable)
    SUPPORT_CHIP += MHT ISISC
endif

ifndef SUPPORT_CHIP
    $(error defined CHIP_TYPE isn't supported!)
endif

export SUPPORT_CHIP

