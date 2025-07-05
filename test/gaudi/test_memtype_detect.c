#include <ucs/memory/memory_type.h>
#include <ucs/memory/memtype_cache.h>
#include <hlthunk.h>
#include <stdio.h>
#include <stdlib.h>

int main() {
    int hlthunk_fd;
    uint64_t handle;
    uint64_t addr;
    ucs_memory_info_t mem_info;
    ucs_status_t status;
    
    printf("=== Memory Type Detection Test ===\n");
    
    /* Open Gaudi device */
    hlthunk_fd = hlthunk_open(HLTHUNK_DEVICE_DONT_CARE, NULL);
    if (hlthunk_fd < 0) {
        printf("Failed to open Gaudi device\n");
        return -1;
    }
    
    printf("Opened Gaudi device (fd=%d)\n", hlthunk_fd);
    
    /* Allocate device memory */
    handle = hlthunk_device_memory_alloc(hlthunk_fd, 4096, 0, true, true);
    if (handle == 0) {
        printf("Failed to allocate device memory\n");
        hlthunk_close(hlthunk_fd);
        return -1;
    }
    
    printf("Allocated device memory (handle=0x%lx)\n", handle);
    
    /* Map to host address space */
    addr = hlthunk_device_memory_map(hlthunk_fd, handle, 0);
    if (addr == 0) {
        printf("Failed to map device memory\n");
        hlthunk_device_memory_free(hlthunk_fd, handle);
        hlthunk_close(hlthunk_fd);
        return -1;
    }
    
    printf("Mapped device memory to address 0x%lx\n", addr);
    
    /* Test memory type detection */
    printf("Testing memory type detection...\n");
    status = ucs_memtype_cache_lookup((void*)addr, 4096, &mem_info);
    if (status != UCS_OK) {
        printf("Memory type detection failed: %s\n", ucs_status_string(status));
    } else {
        printf("SUCCESS: Memory type = %d\n", mem_info.type);
        printf("Address = %p, length = %zu\n", mem_info.base_address, mem_info.alloc_length);
    }
    
    /* Cleanup */
    hlthunk_device_memory_free(hlthunk_fd, handle);
    hlthunk_close(hlthunk_fd);
    
    return 0;
}
