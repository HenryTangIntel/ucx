/**
 * Real Gaudi DMA-BUF Integration Test
 * 
 * This test checks for missing pieces in the current Gaudi DMA-BUF implementation
 * to work with real hardware and IB integration.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/mman.h>
#include <errno.h>

#include <uct/api/uct.h>
#include <ucs/type/status.h>
#include <ucs/memory/memory_type.h>

#ifdef HAVE_HLTHUNK_H
#include <hlthunk.h>
#endif

static void test_real_gaudi_dmabuf_issues()
{
    printf("=== Testing Real Gaudi DMA-BUF Implementation Issues ===\n");
    
    uct_component_h *components;
    unsigned num_components;
    uct_md_h gaudi_md = NULL;
    ucs_status_t status;
    
    /* Find Gaudi component */
    status = uct_query_components(&components, &num_components);
    if (status != UCS_OK) {
        printf("✗ Failed to query components\n");
        return;
    }
    
    for (unsigned i = 0; i < num_components; i++) {
        uct_component_attr_t comp_attr;
        comp_attr.field_mask = UCT_COMPONENT_ATTR_FIELD_NAME;
        status = uct_component_query(components[i], &comp_attr);
        
        if (status == UCS_OK && strstr(comp_attr.name, "gaudi")) {
            uct_md_config_t *md_config;
            status = uct_md_config_read(components[i], NULL, NULL, &md_config);
            if (status == UCS_OK) {
                status = uct_md_open(components[i], "gaudi_copy", md_config, &gaudi_md);
                uct_config_release(md_config);
                if (status == UCS_OK) {
                    printf("✓ Opened Gaudi MD\n");
                    break;
                }
            }
        }
    }
    
    uct_release_component_list(components);
    
    if (!gaudi_md) {
        printf("⚠ Gaudi MD not available\n");
        return;
    }
    
    printf("\n--- Issue 1: Check DMA-BUF Export API Usage ---\n");
    
    /* Test memory allocation and DMA-BUF export */
    size_t alloc_size = 4096;
    void *allocated_addr;
    uct_mem_h memh;
    
    status = uct_md_mem_alloc(gaudi_md, &alloc_size, &allocated_addr, 
                             UCS_MEMORY_TYPE_GAUDI, 
                             UCT_MD_MEM_FLAG_FIXED, /* Request DMA-BUF */
                             "test_dmabuf", &memh);
    
    if (status == UCS_OK) {
        printf("✓ Allocated Gaudi memory: %p, size: %zu\n", allocated_addr, alloc_size);
        
        /* Check what type of address we got */
        printf("→ Address analysis:\n");
        printf("  Virtual address: %p (0x%lx)\n", allocated_addr, (uintptr_t)allocated_addr);
        
        /* Check if it's in user space (likely issue) */
        if ((uintptr_t)allocated_addr < 0x100000000000ULL) {
            printf("  ⚠ Address looks like host virtual memory, not device memory\n");
            printf("  → This might be the issue: using host VA instead of device PA\n");
        } else {
            printf("  ✓ Address looks like device memory range\n");
        }
        
        /* Try to query DMA-BUF */
        uct_md_mem_attr_t mem_attr;
        mem_attr.field_mask = UCT_MD_MEM_ATTR_FIELD_DMABUF_FD;
        
        status = uct_md_mem_query(gaudi_md, allocated_addr, alloc_size, &mem_attr);
        
        if (status == UCS_OK && mem_attr.dmabuf_fd != UCT_DMABUF_FD_INVALID) {
            printf("✓ DMA-BUF export successful: fd=%d\n", mem_attr.dmabuf_fd);
            
            /* Validate the DMA-BUF file descriptor */
            printf("→ DMA-BUF validation:\n");
            
            struct stat dmabuf_stat;
            if (fstat(mem_attr.dmabuf_fd, &dmabuf_stat) == 0) {
                printf("  ✓ DMA-BUF fd %d is valid\n", mem_attr.dmabuf_fd);
                printf("    Size: %ld bytes\n", dmabuf_stat.st_size);
                printf("    Device: %ld\n", dmabuf_stat.st_dev);
                printf("    Inode: %ld\n", dmabuf_stat.st_ino);
                
                /* Check if size matches */
                if (dmabuf_stat.st_size == (long)alloc_size) {
                    printf("  ✓ DMA-BUF size matches allocated size\n");
                } else if (dmabuf_stat.st_size == 0) {
                    printf("  ⚠ DMA-BUF size is 0 - might be a driver issue\n");
                } else {
                    printf("  ⚠ DMA-BUF size mismatch: %ld vs %zu\n", 
                           dmabuf_stat.st_size, alloc_size);
                }
            } else {
                printf("  ✗ DMA-BUF fd %d is invalid: %m\n", mem_attr.dmabuf_fd);
            }
            
            /* Test memory mapping through DMA-BUF */
            printf("→ Testing DMA-BUF memory mapping:\n");
            void *mapped_addr = mmap(NULL, alloc_size, PROT_READ | PROT_WRITE, 
                                    MAP_SHARED, mem_attr.dmabuf_fd, 0);
            
            if (mapped_addr != MAP_FAILED) {
                printf("  ✓ DMA-BUF mmap successful: %p\n", mapped_addr);
                
                /* Test memory access */
                printf("→ Testing cross-device memory access:\n");
                uint32_t test_pattern = 0xDEADBEEF;
                
                /* Write through original allocation */
                *(uint32_t*)allocated_addr = test_pattern;
                
                /* Read through DMA-BUF mapping */
                uint32_t read_pattern = *(uint32_t*)mapped_addr;
                
                if (read_pattern == test_pattern) {
                    printf("  ✓ Memory coherency works: 0x%08X\n", read_pattern);
                } else {
                    printf("  ✗ Memory coherency FAILED: wrote 0x%08X, read 0x%08X\n",
                           test_pattern, read_pattern);
                    printf("    → This indicates DMA-BUF is not properly mapped to device memory\n");
                }
                
                munmap(mapped_addr, alloc_size);
            } else {
                printf("  ✗ DMA-BUF mmap failed: %m\n");
                printf("    → This is a critical issue for IB integration\n");
            }
            
            close(mem_attr.dmabuf_fd);
        } else {
            printf("✗ DMA-BUF export failed: %s\n", ucs_status_string(status));
            printf("→ Possible issues:\n");
            printf("  • hlthunk_device_mapped_memory_export_dmabuf_fd() API misuse\n");
            printf("  • Wrong device address type (host VA vs device PA)\n");
            printf("  • Memory not properly pinned for DMA-BUF export\n");
            printf("  • Missing device driver support\n");
        }
        
        /* Free memory */
        uct_allocated_memory_t allocated_mem;
        allocated_mem.address = allocated_addr;
        allocated_mem.memh = memh;
        allocated_mem.md = gaudi_md;
        allocated_mem.method = UCT_ALLOC_METHOD_MD;
        uct_mem_free(&allocated_mem);
        
    } else {
        printf("⚠ Memory allocation failed: %s\n", ucs_status_string(status));
    }
    
    printf("\n--- Issue 2: Check Device Address vs Host Address ---\n");
    
#ifdef HAVE_HLTHUNK_H
    /* Direct hlthunk testing to understand the issue */
    printf("→ Testing direct hlthunk API usage:\n");
    
    /* This should use the actual device file descriptor if available */
    int test_fd = hlthunk_open(HLTHUNK_DEVICE_GAUDI, NULL);
    if (test_fd >= 0) {
        printf("  ✓ Opened hlthunk device directly: fd=%d\n", test_fd);
        
        /* Allocate device memory directly */
        uint64_t handle = hlthunk_device_memory_alloc(test_fd, 4096, 0, true, true);
        if (handle != 0) {
            printf("  ✓ Allocated device memory: handle=0x%lx\n", handle);
            
            /* Map to get host virtual address */
            uint64_t host_addr = hlthunk_device_memory_map(test_fd, handle, 0);
            if (host_addr != 0) {
                printf("  ✓ Mapped to host address: 0x%lx\n", host_addr);
                
                /* Try DMA-BUF export with mapped address */
                printf("  → Testing DMA-BUF export with mapped address:\n");
                int dmabuf_fd = hlthunk_device_mapped_memory_export_dmabuf_fd(
                    test_fd, host_addr, 4096, 0, O_RDWR | O_CLOEXEC);
                
                if (dmabuf_fd >= 0) {
                    printf("    ✓ DMA-BUF export successful: fd=%d\n", dmabuf_fd);
                    
                    /* Test if this DMA-BUF actually works */
                    void *dmabuf_map = mmap(NULL, 4096, PROT_READ | PROT_WRITE,
                                           MAP_SHARED, dmabuf_fd, 0);
                    
                    if (dmabuf_map != MAP_FAILED) {
                        printf("    ✓ DMA-BUF mapping successful\n");
                        
                        /* Test coherency */
                        *(uint32_t*)((void*)host_addr) = 0x12345678;
                        uint32_t read_val = *(uint32_t*)dmabuf_map;
                        
                        if (read_val == 0x12345678) {
                            printf("    ✓ Real DMA-BUF coherency verified!\n");
                        } else {
                            printf("    ✗ DMA-BUF coherency failed\n");
                        }
                        
                        munmap(dmabuf_map, 4096);
                    } else {
                        printf("    ✗ DMA-BUF mapping failed: %m\n");
                    }
                    
                    close(dmabuf_fd);
                } else {
                    printf("    ✗ DMA-BUF export failed: %m\n");
                    printf("      → Error code: %d (%s)\n", errno, strerror(errno));
                    printf("      → This indicates a real implementation issue\n");
                }
                
                hlthunk_device_memory_free(test_fd, handle);
            } else {
                printf("  ✗ Failed to map device memory\n");
            }
        } else {
            printf("  ✗ Failed to allocate device memory\n");
        }
        
        hlthunk_close(test_fd);
    } else {
        printf("  ⚠ No hlthunk device available (expected without hardware)\n");
    }
#else
    printf("  ⚠ hlthunk not available at compile time\n");
#endif
    
    uct_md_close(gaudi_md);
    
    printf("\n--- Summary of Potential Issues ---\n");
    printf("1. Address Type Issue:\n");
    printf("   • Current: Using hlthunk_device_memory_map() result (host VA)\n");
    printf("   • Needed: Use device physical address or handle for DMA-BUF\n");
    printf("\n");
    printf("2. API Usage Issue:\n");
    printf("   • Current: hlthunk_device_mapped_memory_export_dmabuf_fd()\n");
    printf("   • Check: Might need different hlthunk API for device memory\n");
    printf("\n");
    printf("3. Memory Pinning:\n");
    printf("   • Current: May not be properly pinning memory for DMA access\n");
    printf("   • Needed: Ensure memory is pinned and DMA-coherent\n");
    printf("\n");
    printf("4. Driver Support:\n");
    printf("   • Check: Habana driver version and DMA-BUF support level\n");
    printf("   • Required: Recent driver with full DMA-BUF export capability\n");
}

int main()
{
    printf("Real Gaudi DMA-BUF Implementation Analysis\n");
    printf("==========================================\n");
    printf("Checking for missing pieces in Gaudi → IB DMA-BUF integration\n\n");
    
    test_real_gaudi_dmabuf_issues();
    
    return 0;
}
