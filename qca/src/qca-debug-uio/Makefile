#
# Makefile for DEBUG UIO
#
obj ?= .

ifeq ($(CONFIG_UIO),y)
obj-m += debug_uio.o
endif

ccflags-y += -Werror -Wall -fno-stack-protector -I$(obj) -I$(obj)/exports -DDEBUG_LEVEL=3