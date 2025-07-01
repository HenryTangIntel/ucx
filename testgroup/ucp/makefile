CC = gcc
UCX_DIR = /workspace/ucx
CFLAGS = -Wall -g -I$(UCX_DIR)/src -I$(UCX_DIR) -I/usr/include
LDFLAGS = -L$(UCX_DIR)/src/ucp/.libs -L$(UCX_DIR)/src/uct/.libs -L$(UCX_DIR)/src/ucs/.libs -L$(UCX_DIR)/src/ucm/.libs
LIBS = -lucp -luct -lucs -lucm
RPATH = -Wl,-rpath,$(UCX_DIR)/src/ucp/.libs:$(UCX_DIR)/src/uct/.libs:$(UCX_DIR)/src/ucs/.libs:$(UCX_DIR)/src/ucm/.libs


TARGETS = ucp_gaudi_uct_test endpoint_test

all: $(TARGETS)

ucp_gaudi_uct_test: ucp_gaudi_uct_test.c
	$(CC) $(CFLAGS) -o $@ $< $(LDFLAGS) $(LIBS) $(RPATH)

endpoint_test: endpoint_test.c
	$(CC) $(CFLAGS) -o $@ $< $(LDFLAGS) $(LIBS) $(RPATH)


clean:
	rm -f $(TARGETS)


run: ucp_gaudi_uct_test
	LD_LIBRARY_PATH=$(UCX_DIR)/src/ucp/.libs:$(UCX_DIR)/src/uct/.libs:$(UCX_DIR)/src/ucs/.libs:$(UCX_DIR)/src/ucm/.libs:/usr/lib/habanalabs ./ucp_gaudi_uct_test
