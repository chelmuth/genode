TARGET = test-packet_stream-client
SRC_CC = client.cc
LIBS   = base stdcxx libc

INC_DIR += $(PRG_DIR)/..

vpath %.cc $(PRG_DIR)/..
