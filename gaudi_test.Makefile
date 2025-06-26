CC = gcc
UCX_DIR = /workspace/ucx

# UCX includes and libraries
CFLAGS = -Wall -g -I$(UCX_DIR) -I$(UCX_DIR)/src

# Library paths
LDFLAGS = -L$(UCX_DIR)/src/ucp/.libs -L$(UCX_DIR)/src/uct/.libs -L$(UCX_DIR)/src/ucs/.libs -L$(UCX_DIR)/src/ucm/.libs

# Libraries to link with
LIBS = -lucp -luct -lucs -lucm 

# Run path for dynamic libraries
RPATH = -Wl,-rpath,$(UCX_DIR)/src/ucp/.libs:$(UCX_DIR)/src/uct/.libs:$(UCX_DIR)/src/ucs/.libs:$(UCX_DIR)/src/ucm/.libs

TARGET = gaudi_test_final

all: $(TARGET)

$(TARGET): gaudi_test_final.c
	$(CC) $(CFLAGS) $< -o $@ $(LDFLAGS) $(LIBS) $(RPATH)

clean:
	rm -f $(TARGET)

run: $(TARGET)
	LD_LIBRARY_PATH=$(UCX_DIR)/src/ucp/.libs:$(UCX_DIR)/src/uct/.libs:$(UCX_DIR)/src/ucs/.libs:$(UCX_DIR)/src/ucm/.libs ./$(TARGET)
