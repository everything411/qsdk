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

ifeq (HMSPPE, $(CHIP_TYPE))
    SUPPORT_CHIP = HPPE APPE MPPE MRPPE JHPPE HMSPPE
endif

ifeq (JHPPE, $(CHIP_TYPE))
    SUPPORT_CHIP = HPPE APPE MPPE MRPPE JHPPE
endif

ifeq ($(ISISC_ENABLE), enable)
    SUPPORT_CHIP += ISISC
endif

ifeq ($(MHT_ENABLE), enable)
    SUPPORT_CHIP += MHT ISISC
endif

#SoC platform plus HTTPPE combination compiling
ifeq ($(HTTPPE_ENABLE), enable)
    SUPPORT_CHIP += HTTPPE
endif

#standalone HTTPPE compiling
ifeq (HTTPPE, $(CHIP_TYPE))
    SUPPORT_CHIP += HTTPPE
endif

ifndef SUPPORT_CHIP
    $(error defined CHIP_TYPE isn't supported!)
endif

export SUPPORT_CHIP

