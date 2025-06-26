# Makefile for Gaudi hardware test

CC = gcc
CFLAGS = -g -Wall -Werror
UCX_INSTALL_DIR = /workspace/ucx/install
INCLUDES = -I$(UCX_INSTALL_DIR)/include
LDFLAGS = -L$(UCX_INSTALL_DIR)/lib
LIBS = -luct -lucp -lucs

TARGET = gaudi_hardware_test

.PHONY: all clean

all: $(TARGET)

$(TARGET): gaudi_hardware_test.c
	$(CC) $(CFLAGS) $(INCLUDES) -o $@ $< $(LDFLAGS) $(LIBS)

clean:
	rm -f $(TARGET)
