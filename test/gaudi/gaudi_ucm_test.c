/**
 * Copyright (c) 2024 Habana Labs, Ltd. ALL RIGHTS RESERVED.
 * Test Gaudi UCM memory hooks functionality
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <dlfcn.h>

#include <ucm/api/ucm.h>
#include <ucs/memory/memory_type.h>
#include <habanalabs/hlthunk.h>

static int alloc_events_received = 0;
static int free_events_received = 0;
static void *last_alloc_addr = NULL;
static size_t last_alloc_size = 0;

static void memory_event_callback(ucm_event_type_t event_type,
                                  ucm_event_t *event, void *arg)
{
    switch (event_type) {
    case UCM_EVENT_MEM_TYPE_ALLOC:
        alloc_events_received++;
        last_alloc_addr = event->mem_type.address;
        last_alloc_size = event->mem_type.size;
        printf("UCM alloc event: addr=%p, size=%zu, mem_type=%s\n",
               event->mem_type.address, event->mem_type.size,
               ucs_memory_type_names[event->mem_type.mem_type]);
        break;
        
    case UCM_EVENT_MEM_TYPE_FREE:
        free_events_received++;
        printf("UCM free event: addr=%p, size=%zu, mem_type=%s\n",
               event->mem_type.address, event->mem_type.size,
               ucs_memory_type_names[event->mem_type.mem_type]);
        break;
        
    default:
        break;
    }
}

static void test_ucm_event_handler_registration()
{
    printf("Testing UCM event handler registration...\n");
    
    ucs_status_t status = ucm_set_event_handler(UCM_EVENT_MEM_TYPE_ALLOC | UCM_EVENT_MEM_TYPE_FREE,
                                                0, memory_event_callback, NULL);
    if (status == UCS_OK) {
        printf("✓ UCM event handler registered successfully\n");
    } else {
        printf("✗ Failed to register UCM event handler: %s\n", ucs_status_string(status));
    }
}

static void test_gaudi_malloc_hooks()
{
    printf("\nTesting Gaudi malloc hooks...\n");
    
    // Reset counters
    alloc_events_received = 0;
    free_events_received = 0;
    
    // Test hlthunk_malloc if available
    void *(*hlthunk_malloc_ptr)(size_t) = dlsym(RTLD_DEFAULT, "hlthunk_malloc");
    int (*hlthunk_free_ptr)(void*) = dlsym(RTLD_DEFAULT, "hlthunk_free");
    
    if (hlthunk_malloc_ptr && hlthunk_free_ptr) {
        printf("  Testing hlthunk_malloc/free hooks...\n");
        
        void *ptr = hlthunk_malloc_ptr(1024);
        if (ptr) {
            printf("  Allocated %p with hlthunk_malloc\n", ptr);
            
            // Small delay to allow UCM processing
            usleep(1000);
            
            int ret = hlthunk_free_ptr(ptr);
            printf("  Freed %p with hlthunk_free (ret=%d)\n", ptr, ret);
            
            // Small delay to allow UCM processing
            usleep(1000);
            
            printf("  Alloc events received: %d\n", alloc_events_received);
            printf("  Free events received: %d\n", free_events_received);
        } else {
            printf("  hlthunk_malloc returned NULL\n");
        }
    } else {
        printf("  hlthunk_malloc/free functions not found (expected if UCM not loaded)\n");
    }
}

static void test_gaudi_device_memory_hooks()
{
    printf("\nTesting Gaudi device memory hooks...\n");
    
    int device_count = hlthunk_get_device_count();
    if (device_count <= 0) {
        printf("  No Gaudi devices available, skipping device memory test\n");
        return;
    }
    
    int fd = hlthunk_open(HLTHUNK_DEVICE_TYPE_GAUDI, 0);
    if (fd < 0) {
        printf("  Failed to open Gaudi device, skipping device memory test\n");
        return;
    }
    
    // Reset counters
    alloc_events_received = 0;
    free_events_received = 0;
    
    printf("  Testing hlthunk_device_memory_alloc/free hooks...\n");
    
    size_t size = 4096;
    uint64_t handle = hlthunk_device_memory_alloc(fd, size, 0, false, false);
    if (handle != 0) {
        printf("  Allocated device memory handle: 0x%lx\n", handle);
        
        // Small delay to allow UCM processing
        usleep(1000);
        
        int ret = hlthunk_device_memory_free(fd, handle, size);
        printf("  Freed device memory handle (ret=%d)\n", ret);
        
        // Small delay to allow UCM processing
        usleep(1000);
        
        printf("  Alloc events received: %d\n", alloc_events_received);
        printf("  Free events received: %d\n", free_events_received);
    } else {
        printf("  Device memory allocation failed\n");
    }
    
    hlthunk_close(fd);
}

static void test_ucm_library_loading()
{
    printf("\nTesting UCM library loading...\n");
    
    // Check if UCM Gaudi module is loaded
    void *ucm_gaudi_lib = dlopen("libucm_gaudi.so", RTLD_NOW | RTLD_NOLOAD);
    if (ucm_gaudi_lib) {
        printf("✓ UCM Gaudi library is loaded\n");
        dlclose(ucm_gaudi_lib);
    } else {
        printf("⚠ UCM Gaudi library not loaded: %s\n", dlerror());
        
        // Try to load it explicitly
        ucm_gaudi_lib = dlopen("libucm_gaudi.so", RTLD_NOW);
        if (ucm_gaudi_lib) {
            printf("✓ Successfully loaded UCM Gaudi library\n");
            dlclose(ucm_gaudi_lib);
        } else {
            printf("✗ Failed to load UCM Gaudi library: %s\n", dlerror());
        }
    }
}

int main()
{
    printf("=== Gaudi UCM Memory Hooks Test ===\n");
    
    test_ucm_library_loading();
    test_ucm_event_handler_registration();
    test_gaudi_malloc_hooks();
    test_gaudi_device_memory_hooks();
    
    printf("\n=== UCM test completed ===\n");
    return 0;
}
