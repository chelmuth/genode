LX_CONTRIB_DIR := $(call select_from_ports,dde_linux)/src/drivers/nic/cpsw
SRC_DIR        := $(REP_DIR)/src/drivers/nic/cpsw
INC_DIR        += $(LX_CONTRIB_DIR)/drivers/net/ethernet/cpsw
INC_DIR        += $(LX_CONTRIB_DIR)/include
INC_DIR        += $(LIB_CACHE_DIR)/cpsw_nic_include/include/include/include
CC_OPT         += -U__linux__ -D__KERNEL__
