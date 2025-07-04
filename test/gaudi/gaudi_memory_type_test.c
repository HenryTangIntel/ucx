/**
 * Copyright (c) 2024 Habana Labs, Ltd. ALL RIGHTS RESERVED.
 * Test Gaudi memory type detection and UCS integration
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include <ucs/memory/memory_type.h>
#include <ucp/api/ucp.h>
#include <habanalabs/hlthunk.h>

static void test_memory_type_names()
{
    printf("Testing memory type names...\n");
    
    // Test that Gaudi memory type is properly defined
    assert(UCS_MEMORY_TYPE_GAUDI < UCS_MEMORY_TYPE_LAST);
    
    // Test memory type name
    const char *gaudi_name = ucs_memory_type_names[UCS_MEMORY_TYPE_GAUDI];
    assert(gaudi_name != NULL);
    assert(strcmp(gaudi_name, "gaudi") == 0);
    
    printf("✓ Gaudi memory type name: %s\n", gaudi_name);
    
    // Test memory type description
    extern const char *ucs_memory_type_descs[];
    const char *gaudi_desc = ucs_memory_type_descs[UCS_MEMORY_TYPE_GAUDI];
    assert(gaudi_desc != NULL);
    printf("✓ Gaudi memory type description: %s\n", gaudi_desc);
}

static void test_memory_type_iteration()
{
    printf("\nTesting memory type iteration...\n");
    
    ucs_memory_type_t mem_type;
    int found_gaudi = 0;
    
    ucs_memory_type_for_each(mem_type) {
        printf("  Memory type %d: %s\n", mem_type, ucs_memory_type_names[mem_type]);
        if (mem_type == UCS_MEMORY_TYPE_GAUDI) {
            found_gaudi = 1;
        }
    }
    
    assert(found_gaudi);
    printf("✓ Gaudi memory type found in iteration\n");
}

static void test_ucp_memory_helpers()
{
    printf("\nTesting UCP memory helper macros...\n");
    
    // Test Gaudi-specific helpers
    assert(UCP_MEM_IS_GAUDI(UCS_MEMORY_TYPE_GAUDI));
    assert(!UCP_MEM_IS_GAUDI(UCS_MEMORY_TYPE_HOST));
    assert(!UCP_MEM_IS_GAUDI(UCS_MEMORY_TYPE_CUDA));
    assert(!UCP_MEM_IS_GAUDI(UCS_MEMORY_TYPE_ROCM));
    printf("✓ UCP_MEM_IS_GAUDI macro works correctly\n");
    
    // Test GPU detection
    assert(UCP_MEM_IS_GPU(UCS_MEMORY_TYPE_GAUDI));
    assert(UCP_MEM_IS_GPU(UCS_MEMORY_TYPE_CUDA));
    assert(UCP_MEM_IS_GPU(UCS_MEMORY_TYPE_ROCM));
    assert(!UCP_MEM_IS_GPU(UCS_MEMORY_TYPE_HOST));
    printf("✓ UCP_MEM_IS_GPU includes Gaudi correctly\n");
    
    // Test CPU accessibility (Gaudi should not be CPU accessible)
    assert(!UCP_MEM_IS_ACCESSIBLE_FROM_CPU(UCS_MEMORY_TYPE_GAUDI));
    assert(UCP_MEM_IS_ACCESSIBLE_FROM_CPU(UCS_MEMORY_TYPE_HOST));
    printf("✓ Gaudi memory correctly marked as not CPU accessible\n");
}

static void test_gaudi_device_availability()
{
    printf("\nTesting Gaudi device availability...\n");
    
    int device_count = hlthunk_get_device_count();
    printf("  Found %d Gaudi devices\n", device_count);
    
    if (device_count > 0) {
        int fd = hlthunk_open(HLTHUNK_DEVICE_TYPE_GAUDI, 0);
        if (fd >= 0) {
            printf("✓ Successfully opened Gaudi device 0\n");
            hlthunk_close(fd);
        } else {
            printf("✗ Failed to open Gaudi device 0\n");
        }
    } else {
        printf("⚠ No Gaudi devices available for testing\n");
    }
}

static void test_ucp_context_with_gaudi()
{
    printf("\nTesting UCP context initialization with Gaudi support...\n");
    
    ucp_config_t *config;
    ucp_context_h context;
    ucp_params_t params;
    ucs_status_t status;
    
    status = ucp_config_read(NULL, NULL, &config);
    assert(status == UCS_OK);
    
    params.field_mask = UCP_PARAM_FIELD_FEATURES;
    params.features = UCP_FEATURE_TAG | UCP_FEATURE_RMA;
    
    status = ucp_init(&params, config, &context);
    if (status == UCS_OK) {
        printf("✓ UCP context initialized successfully\n");
        
        // Query supported memory types
        ucp_context_attr_t attr;
        attr.field_mask = UCP_ATTR_FIELD_MEMORY_TYPES;
        status = ucp_context_query(context, &attr);
        if (status == UCS_OK) {
            printf("  Supported memory types: 0x%lx\n", attr.memory_types);
            if (attr.memory_types & UCS_BIT(UCS_MEMORY_TYPE_GAUDI)) {
                printf("✓ Gaudi memory type supported by UCP context\n");
            } else {
                printf("⚠ Gaudi memory type not supported by UCP context\n");
            }
        }
        
        ucp_cleanup(context);
    } else {
        printf("✗ Failed to initialize UCP context: %s\n", ucs_status_string(status));
    }
    
    ucp_config_release(config);
}

int main()
{
    printf("=== Gaudi Memory Type Detection Test ===\n");
    
    test_memory_type_names();
    test_memory_type_iteration();
    test_ucp_memory_helpers();
    test_gaudi_device_availability();
    test_ucp_context_with_gaudi();
    
    printf("\n=== All tests completed ===\n");
    return 0;
}
