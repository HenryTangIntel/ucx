/*
 * Unit test for gaudi_dma.c functions
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <assert.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <hlthunk.h>
#include "../../src/uct/gaudi/base/gaudi_dma.h"
#include <stdlib.h>



int test_dma_copy(int hlthunk_fd, struct hlthunk_hw_ip_info *hw, uint64_t dev_addr, void *host_buf, size_t len) {
    ucs_status_t st = uct_gaudi_dma_execute_copy(hlthunk_fd, (void*)dev_addr, host_buf, len, hw);
    if (st == UCS_OK) {
        printf("test_dma_copy: PASSED\n");
        return 0;
    } else {
        printf("test_dma_copy: FAILED (%d)\n", st);
        return 1;
    }
}


int main() {
    int hlthunk_fd = -1, rc = 0;
    struct hlthunk_hw_ip_info hw;
    uint64_t handle = 0, dev_addr = 0;
    size_t len = 4096;
    void *host_buf = NULL;

    hlthunk_fd = hlthunk_open(HLTHUNK_DEVICE_DONT_CARE, NULL);
    if (hlthunk_fd < 0) {
        fprintf(stderr, "Failed to open Gaudi device!\n");
        return 1;
    }
    if (hlthunk_get_hw_ip_info(hlthunk_fd, &hw) != 0) {
        fprintf(stderr, "Failed to get hw_ip_info!\n");
        hlthunk_close(hlthunk_fd);
        return 1;
    }
    handle = hlthunk_device_memory_alloc(hlthunk_fd, len, 4096, 1, 1);
    if (!handle) {
        fprintf(stderr, "Failed to alloc device memory!\n");
        hlthunk_close(hlthunk_fd);
        return 1;
    }
    dev_addr = hlthunk_device_memory_map(hlthunk_fd, handle, 0);
    if (!dev_addr) {
        fprintf(stderr, "Failed to map device memory!\n");
        hlthunk_close(hlthunk_fd);
        return 1;
    }
    host_buf = aligned_alloc(4096, len);
    if (!host_buf) {
        fprintf(stderr, "Failed to alloc host buffer!\n");
        hlthunk_close(hlthunk_fd);
        return 1;
    }
    memset(host_buf, 0xA5, len);

    rc |= test_dma_copy(hlthunk_fd, &hw, dev_addr, host_buf, len);

    hlthunk_memory_unmap(hlthunk_fd, dev_addr);
    free(host_buf);
    hlthunk_close(hlthunk_fd);

    if (rc == 0) {
        printf("All gaudi_dma tests PASSED.\n");
    } else {
        printf("Some gaudi_dma tests FAILED.\n");
    }
    return rc;
}
