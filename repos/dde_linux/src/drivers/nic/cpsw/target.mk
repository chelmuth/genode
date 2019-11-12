TARGET   = cpsw_nic_drv
REQUIRES = arm_v7
LIBS     = base lx_kit_setjmp cpsw_nic_include

SRC_CC = 
SRC_C  = 

INC_DIR += $(PRG_DIR)

# lx_kit
SRC_CC  += env.cc irq.cc malloc.cc scheduler.cc timer.cc work.cc printf.cc
INC_DIR += $(REP_DIR)/src/include
INC_DIR += $(REP_DIR)/src/include/spec/arm

# contrib code
LX_CONTRIB_DIR := $(call select_from_ports,dde_linux)/src/drivers/nic/fec
SRC_C          += $(notdir $(wildcard $(LX_CONTRIB_DIR)/drivers/net/ethernet/freescale/*.c))
SRC_C          += $(notdir $(wildcard $(LX_CONTRIB_DIR)/drivers/net/phy/*.c))
SRC_C          += $(notdir $(wildcard $(LX_CONTRIB_DIR)/net/core/*.c))
SRC_C          += $(notdir $(wildcard $(LX_CONTRIB_DIR)/net/ethernet/*.c))

#
# Standard C flags from kernel's Makefile (C89 with GNU extensions, prevent warnings)
#
CC_C_OPT += -Wundef -Wstrict-prototypes -Wno-trigraphs \
            -Werror-implicit-function-declaration -Wno-format-security \
            -std=gnu89

vpath %.c  $(PRG_DIR)
vpath %.cc $(PRG_DIR)
vpath %.cc $(REP_DIR)/src/lx_kit

CC_CXX_WARN_STRICT =
