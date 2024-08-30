TARGET = test-packet_stream-server
SRC_CC = server.cc
LIBS   = base stdcxx libc

INC_DIR += $(PRG_DIR)/..

vpath %.cc $(PRG_DIR)/..
