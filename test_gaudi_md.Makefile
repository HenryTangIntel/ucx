CC = gcc
UCX_DIR = /workspace/ucx
# Use the local build directories for includes and libs
CFLAGS = -Wall -g -I$(UCX_DIR)/src -I$(UCX_DIR)
LDFLAGS = -L$(UCX_DIR)/src/ucp/.libs -L$(UCX_DIR)/src/uct/.libs -L$(UCX_DIR)/src/ucs/.libs -L$(UCX_DIR)/src/ucm/.libs
LIBS = -lucp -luct -lucs -lucm
HLTHUNK_LIBS = -lhlthunk

TARGET = test_gaudi_md

all: $(TARGET)

$(TARGET): test_gaudi_md.c
	$(CC) $(CFLAGS) -o $@ $< $(LDFLAGS) $(LIBS) $(HLTHUNK_LIBS)

clean:
	rm -f $(TARGET)
