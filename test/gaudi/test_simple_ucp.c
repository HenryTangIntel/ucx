/**
 * Simple UCP test to trigger memory allocation and see debug output
 */

#include <ucp/api/ucp.h>
#include <ucs/memory/memory_type.h>
#include <stdio.h>
#include <stdlib.h>

int main()
{
    ucp_context_h context;
    ucp_config_t *config;
    ucp_params_t params;
    ucs_status_t status;
    
    printf("Starting UCP test...\n");
    
    /* Read UCP configuration */
    status = ucp_config_read(NULL, NULL, &config);
    if (status != UCS_OK) {
        printf("ucp_config_read failed\n");
        return -1;
    }
    
    /* Initialize UCP context */
    params.field_mask = UCP_PARAM_FIELD_FEATURES;
    params.features   = UCP_FEATURE_RMA;
    
    printf("Initializing UCP context...\n");
    status = ucp_init(&params, config, &context);
    ucp_config_release(config);
    if (status != UCS_OK) {
        printf("ucp_init failed: %d\n", status);
        return -1;
    }
    
    printf("UCP context initialized successfully\n");
    
    /* Try allocating Gaudi memory */
    ucp_mem_h memh;
    ucp_mem_map_params_t mem_params;
    
    mem_params.field_mask = UCP_MEM_MAP_PARAM_FIELD_ADDRESS |
                            UCP_MEM_MAP_PARAM_FIELD_LENGTH |
                            UCP_MEM_MAP_PARAM_FIELD_FLAGS |
                            UCP_MEM_MAP_PARAM_FIELD_MEMORY_TYPE;
    mem_params.address     = NULL;
    mem_params.length      = 4096;
    mem_params.flags       = UCP_MEM_MAP_ALLOCATE;
    mem_params.memory_type = UCS_MEMORY_TYPE_GAUDI;
    
    printf("Trying to allocate Gaudi memory...\n");
    status = ucp_mem_map(context, &mem_params, &memh);
    if (status == UCS_OK) {
        printf("SUCCESS: Gaudi memory allocated!\n");
        ucp_mem_unmap(context, memh);
    } else {
        printf("FAILED: Gaudi memory allocation failed with status %d\n", status);
    }
    
    printf("Cleaning up...\n");
    ucp_cleanup(context);
    
    return 0;
}
