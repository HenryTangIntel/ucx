#include <ucs/type/status.h>
#include <uct/api/uct.h>
#include <uct/api/uct_def.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define TEST_BUFFER_SIZE 1024

/* Test memory registration with the Gaudi MD */
static int test_mem_reg(uct_md_h md) {
    void *buffer;
    uct_md_mem_attr_t mem_attr;
    ucs_status_t status;
    uct_mem_h memh;
    
    printf("\n=== Testing Memory Registration ===\n");
    
    /* Allocate a buffer aligned to page size */
    buffer = aligned_alloc(4096, TEST_BUFFER_SIZE);
    if (buffer == NULL) {
        printf("Failed to allocate aligned memory\n");
        return -1;
    }
    
    /* Initialize the buffer with some data */
    memset(buffer, 0xAB, TEST_BUFFER_SIZE);
    
    /* Try to register the memory */
    status = uct_md_mem_reg(md, buffer, TEST_BUFFER_SIZE, 
                           UCT_MD_MEM_ACCESS_ALL, &memh);
    
    if (status != UCS_OK) {
        printf("Memory registration failed: %s\n", ucs_status_string(status));
        free(buffer);
        return -1;
    }
    
    printf("Successfully registered memory with Gaudi MD\n");
    
    /* Query the memory attributes */
    memset(&mem_attr, 0, sizeof(mem_attr));
    mem_attr.field_mask = UCT_MD_MEM_ATTR_FIELD_MEM_TYPE;
    
    status = uct_md_mem_query(md, memh, &mem_attr);
    if (status == UCS_OK) {
        printf("Memory type: %d\n", mem_attr.mem_type);
    } else {
        printf("Memory query failed: %s\n", ucs_status_string(status));
    }
    
    /* Deregister the memory */
    status = uct_md_mem_dereg(md, memh);
    if (status != UCS_OK) {
        printf("Memory deregistration failed: %s\n", ucs_status_string(status));
        free(buffer);
        return -1;
    }
    
    printf("Successfully deregistered memory\n");
    free(buffer);
    return 0;
}

/* Test memory allocation with the Gaudi MD */
static int test_mem_alloc(uct_md_h md) {
    void *address;
    uct_allocated_memory_t mem;
    ucs_status_t status;
    
    printf("\n=== Testing Memory Allocation ===\n");
    
    /* Try to allocate memory using the MD */
    status = uct_md_mem_alloc(md, TEST_BUFFER_SIZE, &mem, 
                             UCT_MD_MEM_ACCESS_ALL, "test_allocation");
    
    if (status != UCS_OK) {
        printf("Memory allocation failed: %s\n", ucs_status_string(status));
        return -1;
    }
    
    printf("Successfully allocated memory with Gaudi MD\n");
    printf("Address: %p, Length: %zu\n", mem.address, mem.length);
    
    /* Initialize and verify the allocated memory */
    memset(mem.address, 0xCD, TEST_BUFFER_SIZE);
    
    /* Free the allocated memory */
    status = uct_md_mem_free(&mem);
    if (status != UCS_OK) {
        printf("Memory free failed: %s\n", ucs_status_string(status));
        return -1;
    }
    
    printf("Successfully freed allocated memory\n");
    return 0;
}

int main(int argc, char **argv) {
    ucs_status_t status;
    uct_md_resource_desc_t *md_resources;
    unsigned num_md_resources;
    uct_component_h *components;
    unsigned num_components;
    uct_component_h gaudi_component = NULL;
    uct_md_config_t *md_config;
    uct_md_h md;
    int result = 0;
    
    /* Initialize UCT */
    status = uct_init();
    if (status != UCS_OK) {
        printf("Failed to initialize UCT: %s\n", ucs_status_string(status));
        return -1;
    }
    
    /* Query the available components */
    status = uct_query_components(&components, &num_components);
    if (status != UCS_OK) {
        printf("Failed to query UCT components: %s\n", ucs_status_string(status));
        uct_cleanup();
        return -1;
    }
    
    /* Find the Gaudi component */
    for (unsigned i = 0; i < num_components; i++) {
        if (strcmp(components[i]->name, "gaudi") == 0) {
            gaudi_component = components[i];
            printf("Found Gaudi component\n");
            break;
        }
    }
    
    if (gaudi_component == NULL) {
        printf("Gaudi component not found\n");
        uct_release_component_list(components);
        uct_cleanup();
        return -1;
    }
    
    /* Get memory domain resources for the component */
    status = uct_component_query_md_resources(gaudi_component, &md_resources, &num_md_resources);
    if (status != UCS_OK) {
        printf("Failed to query MD resources: %s\n", ucs_status_string(status));
        uct_release_component_list(components);
        uct_cleanup();
        return -1;
    }
    
    printf("Found %u MD resources for Gaudi\n", num_md_resources);
    
    if (num_md_resources == 0) {
        printf("No memory domains found for Gaudi component\n");
        uct_release_md_resource_list(md_resources);
        uct_release_component_list(components);
        uct_cleanup();
        return -1;
    }
    
    /* Read Gaudi MD configuration */
    status = uct_md_config_read(gaudi_component, NULL, NULL, &md_config);
    if (status != UCS_OK) {
        printf("Failed to read Gaudi MD config: %s\n", ucs_status_string(status));
        uct_release_md_resource_list(md_resources);
        uct_release_component_list(components);
        uct_cleanup();
        return -1;
    }
    
    /* Open the Gaudi memory domain */
    status = uct_md_open(gaudi_component, md_resources[0].md_name, md_config, &md);
    uct_config_release(md_config);
    
    if (status != UCS_OK) {
        printf("Failed to open Gaudi memory domain: %s\n", ucs_status_string(status));
        uct_release_md_resource_list(md_resources);
        uct_release_component_list(components);
        uct_cleanup();
        return -1;
    }
    
    printf("Successfully opened Gaudi memory domain\n");
    
    /* Test memory registration */
    if (test_mem_reg(md) != 0) {
        printf("Memory registration test failed\n");
        result = -1;
    }
    
    /* Test memory allocation */
    if (test_mem_alloc(md) != 0) {
        printf("Memory allocation test failed\n");
        result = -1;
    }
    
    /* Cleanup */
    uct_md_close(md);
    uct_release_md_resource_list(md_resources);
    uct_release_component_list(components);
    uct_cleanup();
    
    if (result == 0) {
        printf("\n=== All hardware tests completed successfully! ===\n");
        return 0;
    } else {
        printf("\n=== Some hardware tests failed. See errors above. ===\n");
        return 1;
    }
}
