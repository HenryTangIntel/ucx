#include <uct/api/uct.h>
#include <ucs/memory/memory_type.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

static void print_test_status(const char *test_name, ucs_status_t status)
{
    printf("%-40s: %s\n", test_name, 
           (status == UCS_OK) ? "PASS" : ucs_status_string(status));
}

int main() {
    uct_component_h *components;
    unsigned num_components;
    uct_md_h md = NULL;
    uct_md_config_t *md_config;
    uct_md_attr_t md_attr;
    ucs_status_t status;
    bool found_gaudi = false;
    
    printf("=== UCT Gaudi Integration Test ===\n");
    
    /* Query components */
    status = uct_query_components(&components, &num_components);
    print_test_status("Query components", status);
    if (status != UCS_OK) {
        return -1;
    }
    
    printf("Found %u UCT components\n", num_components);
    
    /* Find Gaudi component and MD */
    for (unsigned i = 0; i < num_components && !found_gaudi; i++) {
        uct_component_attr_t component_attr;
        status = uct_component_query(components[i], &component_attr);
        if (status != UCS_OK) continue;
        
        for (unsigned j = 0; j < component_attr.md_resource_count; j++) {
            printf("Found MD: %s\n", component_attr.md_resources[j].md_name);
            
            if (strstr(component_attr.md_resources[j].md_name, "gaudi")) {
                printf("=== Testing Gaudi MD: %s ===\n", component_attr.md_resources[j].md_name);
                found_gaudi = true;
                
                /* Open the MD */
                status = uct_md_config_read(components[i], NULL, NULL, &md_config);
                print_test_status("Read MD config", status);
                if (status != UCS_OK) continue;
                
                status = uct_md_open(components[i], component_attr.md_resources[j].md_name, md_config, &md);
                uct_config_release(md_config);
                print_test_status("Open MD", status);
                if (status != UCS_OK) continue;
                
                /* Query MD attributes */
                status = uct_md_query(md, &md_attr);
                print_test_status("Query MD attributes", status);
                if (status == UCS_OK) {
                    printf("  Reg memory types: 0x%lx\n", md_attr.cap.reg_mem_types);
                    printf("  Alloc memory types: 0x%lx\n", md_attr.cap.alloc_mem_types);
                    printf("  Access memory types: 0x%lx\n", md_attr.cap.access_mem_types);
                    printf("  Detect memory types: 0x%lx\n", md_attr.cap.detect_mem_types);
                    printf("  Flags: 0x%x\n", md_attr.cap.flags);
                    printf("  Max alloc: %lu\n", md_attr.cap.max_alloc);
                    printf("  RKey packed size: %zu\n", md_attr.rkey_packed_size);
                }
                
                /* Test memory detection */
                char test_buffer[1024];
                ucs_memory_type_t mem_type;
                status = uct_md_detect_memory_type(md, test_buffer, sizeof(test_buffer), &mem_type);
                print_test_status("Detect host memory type", status);
                if (status == UCS_OK) {
                    printf("  Host buffer memory type: %d (expected: %d)\n", 
                           mem_type, UCS_MEMORY_TYPE_HOST);
                }
                
                /* Test memory allocation if supported */
                if (md_attr.cap.alloc_mem_types & UCS_BIT(UCS_MEMORY_TYPE_GAUDI)) {
                    printf("=== Testing Gaudi Memory Allocation ===\n");
                    void *gaudi_ptr = NULL;
                    size_t alloc_length = 4096;
                    uct_mem_h gaudi_memh;
                    
                    status = uct_mem_alloc(md, &alloc_length, &gaudi_ptr, 
                                         UCS_MEMORY_TYPE_GAUDI, UCS_SYS_DEVICE_ID_UNKNOWN, 
                                         "test_alloc", &gaudi_memh);
                    print_test_status("Allocate Gaudi memory", status);
                    
                    if (status == UCS_OK && gaudi_ptr != NULL) {
                        printf("  Allocated %zu bytes at %p\n", alloc_length, gaudi_ptr);
                        
                        /* Test memory detection on allocated memory */
                        status = uct_md_detect_memory_type(md, gaudi_ptr, alloc_length, &mem_type);
                        print_test_status("Detect allocated memory type", status);
                        if (status == UCS_OK) {
                            printf("  Allocated buffer memory type: %d (expected: %d)\n", 
                                   mem_type, UCS_MEMORY_TYPE_GAUDI);
                        }
                        
                        /* Test memory query */
                        uct_md_mem_attr_t mem_attr;
                        mem_attr.field_mask = UCT_MD_MEM_ATTR_FIELD_MEM_TYPE | 
                                            UCT_MD_MEM_ATTR_FIELD_SYS_DEV;
                        status = uct_md_mem_query(md, gaudi_ptr, alloc_length, &mem_attr);
                        print_test_status("Query allocated memory", status);
                        if (status == UCS_OK) {
                            printf("  Queried memory type: %d\n", mem_attr.mem_type);
                            printf("  Queried sys device: %d\n", mem_attr.sys_dev);
                        }
                        
                        /* Free the allocated memory */
                        status = uct_mem_free(gaudi_memh);
                        print_test_status("Free Gaudi memory", status);
                    }
                }
                
                /* Test memory registration */
                printf("=== Testing Memory Registration ===\n");
                char reg_buffer[4096];
                uct_mem_h reg_memh;
                
                status = uct_md_mem_reg(md, reg_buffer, sizeof(reg_buffer), 0, &reg_memh);
                print_test_status("Register host memory", status);
                
                if (status == UCS_OK) {
                    /* Test memory key packing */
                    char mkey_buffer[256];
                    
                    status = uct_md_mkey_pack(md, reg_memh, mkey_buffer);
                    print_test_status("Pack memory key", status);
                    
                    /* Deregister memory */
                    status = uct_md_mem_dereg(md, reg_memh);
                    print_test_status("Deregister host memory", status);
                }
                
                /* Close MD */
                uct_md_close(md);
                md = NULL;
                break;
            }
        }
    }
    
    if (!found_gaudi) {
        printf("No Gaudi MD found - this may be expected if no Gaudi device is available\n");
    }
    
    uct_release_component_list(components);
    printf("=== Test completed ===\n");
    
    return found_gaudi ? 0 : 1;
}
