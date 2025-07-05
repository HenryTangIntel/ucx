#include <ucp/api/ucp.h>
#include <ucs/memory/memory_type.h>
#include <stdio.h>
#include <stdlib.h>

static void print_error(ucs_status_t status, const char *operation) {
    printf("%s failed: %s\n", operation, ucs_status_string(status));
}

int main(int argc, char **argv) {
    ucp_context_h context;
    ucp_config_t *config;
    ucp_params_t params;
    ucp_mem_h memh;
    ucp_mem_map_params_t mem_params;
    ucs_status_t status;
    
    printf("=== UCP Gaudi Memory Allocation Debug Test ===\n");
    
    /* Initialize UCP */
    status = ucp_config_read(NULL, NULL, &config);
    if (status != UCS_OK) {
        print_error(status, "ucp_config_read");
        return -1;
    }
    
    params.field_mask = UCP_PARAM_FIELD_FEATURES;
    params.features   = UCP_FEATURE_RMA;
    
    status = ucp_init(&params, config, &context);
    ucp_config_release(config);
    if (status != UCS_OK) {
        print_error(status, "ucp_init");
        return -1;
    }
    
    printf("UCP context initialized successfully\n");
    
    /* Try to allocate Gaudi memory */
    printf("\nAttempting to allocate Gaudi memory...\n");
    
    mem_params.field_mask = UCP_MEM_MAP_PARAM_FIELD_ADDRESS |
                            UCP_MEM_MAP_PARAM_FIELD_LENGTH |
                            UCP_MEM_MAP_PARAM_FIELD_FLAGS |
                            UCP_MEM_MAP_PARAM_FIELD_MEMORY_TYPE;
    mem_params.address     = NULL;
    mem_params.length      = 4096;
    mem_params.flags       = UCP_MEM_MAP_ALLOCATE;
    mem_params.memory_type = UCS_MEMORY_TYPE_GAUDI;
    
    status = ucp_mem_map(context, &mem_params, &memh);
    if (status != UCS_OK) {
        printf("FAILED: Gaudi memory allocation failed: %s\n", ucs_status_string(status));
    } else {
        printf("SUCCESS: Gaudi memory allocated successfully!\n");
        printf("Memory handle: %p\n", memh);
        ucp_mem_unmap(context, memh);
    }
    
    /* Try to allocate host memory for comparison */
    printf("\nAttempting to allocate host memory for comparison...\n");
    
    mem_params.memory_type = UCS_MEMORY_TYPE_HOST;
    
    status = ucp_mem_map(context, &mem_params, &memh);
    if (status != UCS_OK) {
        printf("FAILED: Host memory allocation failed: %s\n", ucs_status_string(status));
    } else {
        printf("SUCCESS: Host memory allocated successfully!\n");
        ucp_mem_unmap(context, memh);
    }
    
    ucp_cleanup(context);
    
    return 0;
}
