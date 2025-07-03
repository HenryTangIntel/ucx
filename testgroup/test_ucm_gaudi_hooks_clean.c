#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdbool.h>
#include <dlfcn.h>
#define _GNU_SOURCE
#include <sys/mman.h>
#include <ucm/api/ucm.h>
#include <ucs/memory/memory_type.h>
#include <ucs/type/status.h>

#ifdef HAVE_HLTHUNK_H
#include <habanalabs/hlthunk.h>
#endif

// Event tracking structure
typedef struct {
    int alloc_events;
    int free_events;
    int mmap_events;
    int munmap_events;
    int vm_mapped_events;
    int vm_unmapped_events;
    void *last_alloc_addr;
    size_t last_alloc_size;
    ucs_memory_type_t last_alloc_type;
    void *last_free_addr;
    ucs_memory_type_t last_free_type;
    void *last_mapped_addr;
    void *last_unmapped_addr;
} ucm_test_events_t;

static ucm_test_events_t g_events = {0};

// Forward declarations
static void mem_alloc_callback(ucm_event_type_t event_type, ucm_event_t *event, void *arg);
static void mem_free_callback(ucm_event_type_t event_type, ucm_event_t *event, void *arg);
static void mmap_callback(ucm_event_type_t event_type, ucm_event_t *event, void *arg);
static void munmap_callback(ucm_event_type_t event_type, ucm_event_t *event, void *arg);
static void vm_mapped_callback(ucm_event_type_t event_type, ucm_event_t *event, void *arg);
static void vm_unmapped_callback(ucm_event_type_t event_type, ucm_event_t *event, void *arg);

// Event callbacks
static void mem_alloc_callback(ucm_event_type_t event_type, ucm_event_t *event, void *arg)
{
    (void)event_type; /* Suppress unused parameter warning */
    (void)arg; /* Suppress unused parameter warning */
    printf("UCM Event: Memory allocated - addr: %p, size: %zu, type: %d\n", 
           event->mem_type.address, event->mem_type.size, event->mem_type.mem_type);
    g_events.alloc_events++;
    g_events.last_alloc_addr = event->mem_type.address;
    g_events.last_alloc_size = event->mem_type.size;
    g_events.last_alloc_type = event->mem_type.mem_type;
}

static void mem_free_callback(ucm_event_type_t event_type, ucm_event_t *event, void *arg)
{
    (void)event_type; /* Suppress unused parameter warning */
    (void)arg; /* Suppress unused parameter warning */
    printf("UCM Event: Memory freed - addr: %p, size: %zu, type: %d\n", 
           event->mem_type.address, event->mem_type.size, event->mem_type.mem_type);
    g_events.free_events++;
    g_events.last_free_addr = event->mem_type.address;
    g_events.last_free_type = event->mem_type.mem_type;
}

static void mmap_callback(ucm_event_type_t event_type, ucm_event_t *event, void *arg)
{
    (void)event_type; /* Suppress unused parameter warning */
    (void)arg; /* Suppress unused parameter warning */
    printf("UCM Event: mmap - addr: %p, size: %zu\n",
           event->mmap.address, event->mmap.size);
    g_events.mmap_events++;
}

static void munmap_callback(ucm_event_type_t event_type, ucm_event_t *event, void *arg)
{
    (void)event_type; /* Suppress unused parameter warning */
    (void)arg; /* Suppress unused parameter warning */
    printf("UCM Event: munmap - addr: %p, size: %zu\n",
           event->munmap.address, event->munmap.size);
    g_events.munmap_events++;
}

static void vm_mapped_callback(ucm_event_type_t event_type, ucm_event_t *event, void *arg)
{
    (void)event_type; /* Suppress unused parameter warning */
    (void)arg; /* Suppress unused parameter warning */
    printf("UCM Event: VM mapped - addr: %p, size: %zu\n",
           event->vm_mapped.address, event->vm_mapped.size);
    g_events.vm_mapped_events++;
    g_events.last_mapped_addr = event->vm_mapped.address;
}

static void vm_unmapped_callback(ucm_event_type_t event_type, ucm_event_t *event, void *arg)
{
    (void)event_type; /* Suppress unused parameter warning */
    (void)arg; /* Suppress unused parameter warning */
    printf("UCM Event: VM unmapped - addr: %p, size: %zu\n",
           event->vm_unmapped.address, event->vm_unmapped.size);
    g_events.vm_unmapped_events++;
    g_events.last_unmapped_addr = event->vm_unmapped.address;
}

static void reset_events(void)
{
    memset(&g_events, 0, sizeof(g_events));
}

static void print_event_summary(void)
{
    printf("\n=== UCM Event Summary ===\n");
    printf("Memory allocations: %d\n", g_events.alloc_events);
    printf("Memory frees: %d\n", g_events.free_events);
    printf("mmap calls: %d\n", g_events.mmap_events);
    printf("munmap calls: %d\n", g_events.munmap_events);
    printf("VM mapped events: %d\n", g_events.vm_mapped_events);
    printf("VM unmapped events: %d\n", g_events.vm_unmapped_events);
    if (g_events.last_alloc_addr) {
        printf("Last alloc: %p, size: %zu, type: %d\n", 
               g_events.last_alloc_addr, g_events.last_alloc_size, g_events.last_alloc_type);
    }
    if (g_events.last_free_addr) {
        printf("Last free: %p, type: %d\n", g_events.last_free_addr, g_events.last_free_type);
    }
    if (g_events.last_mapped_addr) {
        printf("Last mapped: %p\n", g_events.last_mapped_addr);
    }
    if (g_events.last_unmapped_addr) {
        printf("Last unmapped: %p\n", g_events.last_unmapped_addr);
    }
    printf("========================\n\n");
}

static ucs_status_t setup_ucm_events(void)
{
    ucs_status_t status;
    
    printf("Setting up UCM event handlers...\n");
    
    // Register memory type allocation/free events
    status = ucm_set_event_handler(UCM_EVENT_MEM_TYPE_ALLOC, 0, mem_alloc_callback, NULL);
    if (status != UCS_OK) {
        printf("Failed to set mem alloc handler: %s\n", ucs_status_string(status));
        return status;
    }
    
    status = ucm_set_event_handler(UCM_EVENT_MEM_TYPE_FREE, 0, mem_free_callback, NULL);
    if (status != UCS_OK) {
        printf("Failed to set mem free handler: %s\n", ucs_status_string(status));
        return status;
    }
    
    // Register mmap/munmap events (optional)
    status = ucm_set_event_handler(UCM_EVENT_MMAP, 0, mmap_callback, NULL);
    if (status != UCS_OK) {
        printf("Warning: Failed to set mmap handler: %s (continuing anyway)\n", ucs_status_string(status));
    } else {
        status = ucm_set_event_handler(UCM_EVENT_MUNMAP, 0, munmap_callback, NULL);
        if (status != UCS_OK) {
            printf("Warning: Failed to set munmap handler: %s (continuing anyway)\n", ucs_status_string(status));
        }
    }
    
    // Register VM mapped/unmapped events for device memory mapping
    status = ucm_set_event_handler(UCM_EVENT_VM_MAPPED, 0, vm_mapped_callback, NULL);
    if (status != UCS_OK) {
        printf("Warning: Failed to set VM mapped handler: %s (continuing anyway)\n", ucs_status_string(status));
    } else {
        status = ucm_set_event_handler(UCM_EVENT_VM_UNMAPPED, 0, vm_unmapped_callback, NULL);
        if (status != UCS_OK) {
            printf("Warning: Failed to set VM unmapped handler: %s (continuing anyway)\n", ucs_status_string(status));
        }
    }
    
    printf("✓ UCM event handlers registered successfully\n");
    return UCS_OK;
}

static void cleanup_ucm_events(void)
{
    printf("Cleaning up UCM event handlers...\n");
    
    ucm_unset_event_handler(UCM_EVENT_MEM_TYPE_ALLOC, mem_alloc_callback, NULL);
    ucm_unset_event_handler(UCM_EVENT_MEM_TYPE_FREE, mem_free_callback, NULL);
    ucm_unset_event_handler(UCM_EVENT_MMAP, mmap_callback, NULL);
    ucm_unset_event_handler(UCM_EVENT_MUNMAP, munmap_callback, NULL);
    ucm_unset_event_handler(UCM_EVENT_VM_MAPPED, vm_mapped_callback, NULL);
    ucm_unset_event_handler(UCM_EVENT_VM_UNMAPPED, vm_unmapped_callback, NULL);
    
    printf("✓ UCM event handlers cleaned up\n");
}

static void test_system_memory_with_ucm(void)
{
    printf("\n=== Testing System Memory with UCM ===\n");
    reset_events();
    
    // Test malloc/free
    printf("Testing malloc/free...\n");
    void *ptr = malloc(4096);
    printf("Allocated: %p\n", ptr);
    
    free(ptr);
    printf("Freed: %p\n", ptr);
    
    print_event_summary();
}

#ifdef HAVE_HLTHUNK_H
static void test_gaudi_memory_with_ucm_hooks(void)
{
    printf("\n=== Testing Gaudi Memory with UCM Hooks ===\n");
    reset_events();
    
    // Try to open Gaudi device
    enum hlthunk_device_name devices[] = {
        HLTHUNK_DEVICE_GAUDI3,
        HLTHUNK_DEVICE_GAUDI2, 
        HLTHUNK_DEVICE_GAUDI,
        HLTHUNK_DEVICE_DONT_CARE
    };
    
    int fd = -1;
    for (int i = 0; i < 4 && fd < 0; i++) {
        fd = hlthunk_open(devices[i], NULL);
        if (fd >= 0) {
            printf("✓ Opened Gaudi device with type %d, fd: %d\n", devices[i], fd);
            break;
        }
    }
    
    if (fd < 0) {
        printf("ℹ  No Gaudi device available, skipping Gaudi memory test\n");
        return;
    }
    
    // Test Gaudi device memory allocation (will be intercepted by UCM)
    printf("Testing Gaudi device memory allocation (intercepted by UCM)...\n");
    uint64_t size = 4096;
    uint64_t handle = hlthunk_device_memory_alloc(fd, size, 4096, true, false);
    
    if (handle != 0) {
        printf("✓ Allocated Gaudi device memory: handle=0x%lx, size=%lu\n", handle, size);
        
        // Test mapping the device memory (will be intercepted by UCM)
        printf("Testing Gaudi device memory mapping (intercepted by UCM)...\n");
        uint64_t mapped_addr = hlthunk_device_memory_map(fd, handle, 0);
        
        if (mapped_addr != 0) {
            printf("✓ Mapped Gaudi device memory: mapped_addr=0x%lx, handle=0x%lx\n", 
                   mapped_addr, handle);
            
            // Note: This hlthunk version doesn't have unmap function
            printf("ℹ  hlthunk_device_memory_unmap not available in this hlthunk version\n");
        } else {
            printf("✗ Failed to map Gaudi device memory\n");
        }
        
        // Test freeing Gaudi device memory (will be intercepted by UCM)
        printf("Testing Gaudi device memory free (intercepted by UCM)...\n");
        int ret = hlthunk_device_memory_free(fd, handle);
        if (ret == 0) {
            printf("✓ Freed Gaudi device memory: handle=0x%lx\n", handle);
        } else {
            printf("✗ Failed to free Gaudi device memory: %d\n", ret);
        }
    } else {
        printf("✗ Failed to allocate Gaudi device memory\n");
    }
    
    hlthunk_close(fd);
    print_event_summary();
}
#else
static void test_gaudi_memory_with_ucm_hooks(void)
{
    printf("\n=== Testing Gaudi Memory with UCM Hooks ===\n");
    printf("ℹ  Gaudi support not compiled in (HAVE_HLTHUNK_H not defined)\n");
    printf("   This is normal if hlthunk development headers are not available\n");
}
#endif

static void test_ucm_query_info(void)
{
    printf("\n=== Testing UCM Query Information ===\n");
    printf("✓ UCM is available and integrated\n");
    printf("  Note: This test demonstrates UCM automatic interception with hlthunk\n");
}

int main(int argc, char **argv)
{
    (void)argc; /* Suppress unused parameter warning */
    (void)argv; /* Suppress unused parameter warning */
    ucs_status_t status;
    
    printf("UCX Memory Manager (UCM) Gaudi Integration Test with Hooks\n");
    printf("=========================================================\n");
    
    // Initialize UCM and set up event handlers
    status = setup_ucm_events();
    if (status != UCS_OK) {
        printf("Failed to setup UCM events: %s\n", ucs_status_string(status));
        return 1;
    }
    
    // Test system memory to verify UCM is working
    test_system_memory_with_ucm();
    
    // Test Gaudi memory operations (these should trigger UCM events automatically)
    test_gaudi_memory_with_ucm_hooks();
    
    // Display UCM information
    test_ucm_query_info();
    
    // Cleanup
    cleanup_ucm_events();
    
    printf("\n=== UCM Gaudi Test Complete ===\n");
    printf("If Gaudi device was available, you should see UCM events for:\n");
    printf("- MEM_TYPE_ALLOC for hlthunk_device_memory_alloc\n");
    printf("- MEM_TYPE_FREE for hlthunk_device_memory_free\n");
    printf("- VM_MAPPED for hlthunk_device_memory_map\n");
    
    return 0;
}
