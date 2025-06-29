#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <uct/api/uct.h>
#include <ucs/api/ucs.h>

void test_memory_operations(uct_md_h md) {
    void *alloc_addr = NULL;
    void *reg_buf = NULL;
    size_t length = 4096;
    uct_mem_h memh_alloc = NULL, memh_reg = NULL;
    ucs_status_t status;

    /* Test memory allocation */
    printf("\n--- Testing Memory Allocation ---\n");
    status = uct_md_mem_alloc(md, &length, &alloc_addr, UCS_MEMORY_TYPE_UNKNOWN,
                              0, "gaudi_alloc", &memh_alloc);
    if (status != UCS_OK) {
        printf("Failed to allocate memory: %s\n", ucs_status_string(status));
    } else if (alloc_addr == NULL) {
        printf("Allocation succeeded but address is NULL\n");
        uct_md_mem_free(md, memh_alloc);
    } else {
        printf("Successfully allocated %zu bytes at %p\n", length, alloc_addr);
        memset(alloc_addr, 0xAA, length);
        printf("Successfully wrote to allocated memory\n");
        uct_md_mem_free(md, memh_alloc);
        printf("Successfully freed allocated memory\n");
    }

    /* Test memory registration */
    printf("\n--- Testing Memory Registration ---\n");
    reg_buf = malloc(length);
    if (!reg_buf) {
        printf("Failed to allocate host buffer for registration\n");
        return;
    }
    memset(reg_buf, 0xBB, length);

    status = uct_md_mem_reg(md, reg_buf, length, 0, &memh_reg);
    if (status != UCS_OK) {
        printf("Failed to register memory: %s\n", ucs_status_string(status));
    } else {
        printf("Successfully registered memory at %p\n", reg_buf);
        memset(reg_buf, 0xCC, length);
        printf("Successfully wrote to registered memory\n");

        /* Verification */
        int valid = 1;
        for (size_t i = 0; i < length; ++i) {
            if (((unsigned char*)reg_buf)[i] != 0xCC) {
                valid = 0;
                break;
            }
        }
        if (valid) {
            printf("Memory verification passed\n");
        } else {
            printf("Memory verification failed\n");
        }
        uct_md_mem_dereg(md, memh_reg);
        printf("Successfully deregistered memory\n");
    }
    free(reg_buf);
}

int main() {
    uct_component_h *components;
    uct_md_h md;
    uct_md_config_t *md_config;
    uct_md_attr_v2_t md_attr;
    unsigned num_components;
    ucs_status_t status;
    int found_gaudi = 0;

    status = uct_query_components(&components, &num_components);
    if (status != UCS_OK) {
        fprintf(stderr, "Failed to query UCT components: %s\n", ucs_status_string(status));
        return 1;
    }

    printf("Found %u UCT components\n", num_components);
    for (unsigned i = 0; i < num_components; ++i) {
        if (strcmp(components[i]->name, "gaudi") == 0) {
            found_gaudi = 1;
            printf("\n=== Found Gaudi Component ===\n");

            status = uct_md_config_read(components[i], NULL, NULL, &md_config);
            if (status != UCS_OK) {
                fprintf(stderr, "Failed to read MD config: %s\n", ucs_status_string(status));
                continue;
            }

            status = uct_md_open(components[i], "gaudi", md_config, &md);
            uct_config_release(md_config);
            if (status != UCS_OK) {
                fprintf(stderr, "Failed to open Gaudi MD: %s\n", ucs_status_string(status));
                continue;
            }

            md_attr.field_mask = UCT_MD_ATTR_FIELD_FLAGS |
                                 UCT_MD_ATTR_FIELD_MAX_ALLOC |
                                 UCT_MD_ATTR_FIELD_MAX_REG |
                                 UCT_MD_ATTR_FIELD_MEM_TYPES;

            status = uct_md_query_v2(md, &md_attr);
            if (status != UCS_OK) {
                fprintf(stderr, "Failed to query MD attributes: %s\n", ucs_status_string(status));
                uct_md_close(md);
                continue;
            }

            printf("MD Capabilities: 0x%lx\n", md_attr.flags);
            printf("Max Allocation: %zu\n", (size_t)md_attr.max_alloc);
            printf("Max Registration: %zu\n", (size_t)md_attr.max_reg);
            printf("Supported Memory Types: 0x%lx\n", md_attr.reg_mem_types);

            if (md_attr.flags & UCT_MD_FLAG_ALLOC) {
                test_memory_operations(md);
            }

            uct_md_close(md);
        }
    }

    if (!found_gaudi) {
        printf("\nGaudi component not found.\n");
    }

    uct_release_component_list(components);
    return 0;
}