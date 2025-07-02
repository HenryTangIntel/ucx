#include <ucp/api/ucp.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main() {
    ucs_status_t status;
    ucp_context_h ucp_context;
    ucp_config_t *config;
    ucp_params_t ucp_params;
    ucp_mem_h gaudi_memh;
    ucp_mem_map_params_t mem_params;
    ucp_mem_attr_t mem_attr;
    void *gaudi_ptr;
    size_t gaudi_size = 2 * 1024 * 1024; // 2MB
    
    printf("UCP Gaudi Integration Example\n");
    
    // Read UCP configuration
    status = ucp_config_read(NULL, NULL, &config);
    if (status != UCS_OK) {
        printf("Failed to read UCP config: %s\n", ucs_status_string(status));
        return 1;
    }
    
    // Set Gaudi-specific configuration
    status = ucp_config_modify(config, "TLS", "gaudi,self,tcp");
    if (status != UCS_OK) {
        printf("Failed to set TLS config: %s\n", ucs_status_string(status));
        goto cleanup_config;
    }
    
    // Initialize UCP context
    memset(&ucp_params, 0, sizeof(ucp_params));
    ucp_params.field_mask = UCP_PARAM_FIELD_FEATURES;
    ucp_params.features = UCP_FEATURE_RMA;  // Use RMA features only
    
    status = ucp_init(&ucp_params, config, &ucp_context);
    if (status != UCS_OK) {
        printf("Failed to initialize UCP: %s\n", ucs_status_string(status));
        goto cleanup_config;
    }
    
    printf("UCP context initialized with Gaudi support\n");
    
    // Allocate memory that could be on Gaudi
    memset(&mem_params, 0, sizeof(mem_params));
    mem_params.field_mask = UCP_MEM_MAP_PARAM_FIELD_LENGTH |
                           UCP_MEM_MAP_PARAM_FIELD_MEMORY_TYPE |
                           UCP_MEM_MAP_PARAM_FIELD_FLAGS;
    mem_params.address = NULL;
    mem_params.length = gaudi_size;
    mem_params.memory_type = UCS_MEMORY_TYPE_HOST; // Start with host memory
    mem_params.flags = UCP_MEM_MAP_ALLOCATE;
    
    status = ucp_mem_map(ucp_context, &mem_params, &gaudi_memh);
    if (status != UCS_OK) {
        printf("Failed to map memory: %s\n", ucs_status_string(status));
        goto cleanup_context;
    }
    
    // Get the memory address
    mem_attr.field_mask = UCP_MEM_ATTR_FIELD_ADDRESS;
    status = ucp_mem_query(gaudi_memh, &mem_attr);
    if (status == UCS_OK) {
        gaudi_ptr = mem_attr.address;
        printf("Mapped %zu bytes at address %p\n", gaudi_size, gaudi_ptr);
        
        // Initialize memory
        memset(gaudi_ptr, 0xAB, gaudi_size);
        printf("Initialized memory with pattern\n");
    }
    
    // Unmap memory
    status = ucp_mem_unmap(ucp_context, gaudi_memh);
    if (status != UCS_OK) {
        printf("Failed to unmap memory: %s\n", ucs_status_string(status));
    } else {
        printf("Successfully unmapped memory\n");
    }
    
cleanup_context:
    ucp_cleanup(ucp_context);
cleanup_config:
    ucp_config_release(config);
    
    printf("UCP example completed\n");
    return 0;
}
