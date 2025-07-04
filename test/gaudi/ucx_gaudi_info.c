/**
 * @file ucx_gaudi_info.c
 * @brief UCX Gaudi information and capability query tool
 * 
 * This program queries and displays UCX Gaudi transport information,
 * capabilities, and configuration options.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sched.h>

#include <uct/api/uct.h>
#include <ucp/api/ucp.h>
#include <ucs/memory/memory_type.h>
#include <ucs/config/parser.h>

static void print_usage(const char *prog_name)
{
    printf("Usage: %s [options]\n", prog_name);
    printf("Options:\n");
    printf("  -h        Show this help\n");
    printf("  -v        Verbose output\n");
    printf("  -c        Show configuration options\n");
    printf("  -t        Show transport information\n");
    printf("  -m        Show memory domain information\n");
    printf("  -a        Show all information (default)\n");
}

static void print_memory_types(const char *prefix, uint64_t mem_types)
{
    printf("%s", prefix);
    int first = 1;
    for (int i = 0; i < UCS_MEMORY_TYPE_LAST; i++) {
        if (mem_types & UCS_BIT(i)) {
            if (!first) printf(", ");
            printf("%s", ucs_memory_type_names[i]);
            first = 0;
        }
    }
    if (first) printf("None");
    printf("\n");
}

static void print_md_flags(const char *prefix, uint64_t flags)
{
    printf("%s", prefix);
    int first = 1;
    
    struct {
        uint64_t flag;
        const char *name;
    } flag_names[] = {
        {UCT_MD_FLAG_ALLOC, "ALLOC"},
        {UCT_MD_FLAG_REG, "REG"},
        {UCT_MD_FLAG_NEED_RKEY, "NEED_RKEY"},
        {UCT_MD_FLAG_NEED_MEMH, "NEED_MEMH"},
        {UCT_MD_FLAG_ADVISE, "ADVISE"},
        {UCT_MD_FLAG_FIXED, "FIXED"},
        {UCT_MD_FLAG_RKEY_PTR, "RKEY_PTR"},
        {UCT_MD_FLAG_INVALIDATE, "INVALIDATE"},
        {UCT_MD_FLAG_REG_DMABUF, "REG_DMABUF"},
        {0, NULL}
    };
    
    for (int i = 0; flag_names[i].name != NULL; i++) {
        if (flags & flag_names[i].flag) {
            if (!first) printf(", ");
            printf("%s", flag_names[i].name);
            first = 0;
        }
    }
    if (first) printf("None");
    printf("\n");
}

static int show_md_info(uct_component_h component, int verbose)
{
    ucs_status_t status;
    uct_md_config_t *md_config;
    uct_md_h md;
    uct_md_attr_t md_attr;

    printf("\n=== Memory Domain Information: %s ===\n", component->name);

    /* Read MD configuration */
    status = uct_md_config_read(component, NULL, NULL, &md_config);
    if (status != UCS_OK) {
        printf("ERROR: Failed to read MD config: %s\n", ucs_status_string(status));
        return -1;
    }

    /* Open memory domain */
    status = component->md_open(component, NULL, md_config, &md);
    if (status != UCS_OK) {
        printf("ERROR: Failed to open MD: %s\n", ucs_status_string(status));
        uct_config_release(md_config);
        return -1;
    }

    /* Query MD attributes */
    status = uct_md_query(md, &md_attr);
    if (status != UCS_OK) {
        printf("ERROR: Failed to query MD: %s\n", ucs_status_string(status));
        goto cleanup;
    }

    /* Print MD information */
    printf("Memory Domain: %s\n", component->name);
    print_md_flags("  Capabilities: ", md_attr.cap.flags);
    print_memory_types("  Allocation types: ", md_attr.cap.alloc_mem_types);
    print_memory_types("  Registration types: ", md_attr.cap.reg_mem_types);
    print_memory_types("  Access types: ", md_attr.cap.access_mem_types);
    print_memory_types("  Detection types: ", md_attr.cap.detect_mem_types);
    
    printf("  Max allocation: ");
    if (md_attr.cap.max_alloc == UINT64_MAX) {
        printf("Unlimited\n");
    } else {
        printf("%lu bytes (%.2f MB)\n", md_attr.cap.max_alloc, 
               (double)md_attr.cap.max_alloc / (1024 * 1024));
    }
    
    printf("  Max registration: ");
    if (md_attr.cap.max_reg == SIZE_MAX) {
        printf("Unlimited\n");
    } else {
        printf("%zu bytes (%.2f MB)\n", md_attr.cap.max_reg, 
               (double)md_attr.cap.max_reg / (1024 * 1024));
    }
    
    printf("  Remote key size: %zu bytes\n", md_attr.rkey_packed_size);

    if (verbose) {
        printf("  Local CPUs: ");
        int found_cpu = 0;
        for (int i = 0; i < CPU_SETSIZE && i < 64; i++) {
            if (CPU_ISSET(i, &md_attr.local_cpus)) {
                if (found_cpu) printf(", ");
                printf("%d", i);
                found_cpu++;
                if (found_cpu > 8) { /* Limit output */
                    printf("...");
                    break;
                }
            }
        }
        if (!found_cpu) printf("All");
        printf("\n");
    }

cleanup:
    uct_md_close(md);
    uct_config_release(md_config);
    return (status == UCS_OK) ? 0 : -1;
}

static int show_transport_info(uct_component_h component, int verbose)
{
    ucs_status_t status;
    uct_md_config_t *md_config;
    uct_md_h md;
    uct_tl_resource_desc_t *tl_resources;
    unsigned num_tl_resources;

    printf("\n=== Transport Information: %s ===\n", component->name);

    /* Open MD first to query transport resources */
    status = uct_md_config_read(component, NULL, NULL, &md_config);
    if (status != UCS_OK) {
        printf("ERROR: Failed to read MD config: %s\n", ucs_status_string(status));
        return -1;
    }

    status = component->md_open(component, NULL, md_config, &md);
    if (status != UCS_OK) {
        printf("ERROR: Failed to open MD: %s\n", ucs_status_string(status));
        uct_config_release(md_config);
        return -1;
    }

    /* Query transport resources */
    status = uct_md_query_tl_resources(md, &tl_resources, &num_tl_resources);
    if (status != UCS_OK) {
        printf("ERROR: Failed to query transport resources: %s\n", ucs_status_string(status));
        uct_md_close(md);
        uct_config_release(md_config);
        return -1;
    }

    printf("Number of transports: %u\n", num_tl_resources);
    
    for (unsigned i = 0; i < num_tl_resources; i++) {
        printf("  Transport %u:\n", i);
        printf("    Name: %s\n", tl_resources[i].tl_name);
        printf("    Device: %s\n", tl_resources[i].dev_name);
        printf("    Type: %s\n", uct_device_type_names[tl_resources[i].dev_type]);
        printf("    System device: %u\n", tl_resources[i].sys_device);
        
        if (verbose) {
            /* Additional transport information */
            printf("    Full name: " UCT_TL_RESOURCE_DESC_FMT "\n", 
                   UCT_TL_RESOURCE_DESC_ARG(&tl_resources[i]));
        }
    }

    uct_release_tl_resource_list(tl_resources);
    uct_md_close(md);
    uct_config_release(md_config);
    return 0;
}

static int show_config_info(uct_component_h component, int verbose)
{
    printf("\n=== Configuration Information: %s ===\n", component->name);
    
    printf("MD Config:\n");
    printf("  Name: %s\n", component->md_config.name);
    printf("  Prefix: %s\n", component->md_config.prefix);
    printf("  Size: %zu bytes\n", component->md_config.size);
    
    if (verbose && component->md_config.table) {
        printf("  Configuration options:\n");
        ucs_config_field_t *field = component->md_config.table;
        while (field && field->name) {
            printf("    %s%s: %s\n", 
                   component->md_config.prefix, 
                   field->name,
                   field->dfl_value ? field->dfl_value : "N/A");
            if (field->doc) {
                printf("      %s\n", field->doc);
            }
            field++;
        }
    }
    
    return 0;
}

int main(int argc, char *argv[])
{
    int show_all = 1;
    int show_config = 0, show_transport = 0, show_md = 0;
    int verbose = 0;
    int opt;

    /* Parse command line arguments */
    while ((opt = getopt(argc, argv, "hvcta m")) != -1) {
        switch (opt) {
        case 'h':
            print_usage(argv[0]);
            return 0;
        case 'v':
            verbose = 1;
            break;
        case 'c':
            show_config = 1;
            show_all = 0;
            break;
        case 't':
            show_transport = 1;
            show_all = 0;
            break;
        case 'm':
            show_md = 1;
            show_all = 0;
            break;
        case 'a':
            show_all = 1;
            break;
        default:
            print_usage(argv[0]);
            return 1;
        }
    }

    printf("UCX Gaudi Information Tool\n");
    printf("==========================\n");

    /* Query UCX components */
    uct_component_h *components;
    unsigned num_components;
    ucs_status_t status = uct_query_components(&components, &num_components);
    if (status != UCS_OK) {
        printf("ERROR: Failed to query UCX components: %s\n", ucs_status_string(status));
        return 1;
    }

    printf("Total UCX components: %u\n", num_components);

    /* Find and process Gaudi components */
    int found_gaudi = 0;
    for (unsigned i = 0; i < num_components; i++) {
        if (strstr(components[i]->name, "gaudi")) {
            found_gaudi = 1;
            
            printf("\n*** Gaudi Component Found: %s ***\n", components[i]->name);
            
            if (show_all || show_config) {
                show_config_info(components[i], verbose);
            }
            
            if (show_all || show_transport) {
                show_transport_info(components[i], verbose);
            }
            
            if (show_all || show_md) {
                show_md_info(components[i], verbose);
            }
        } else if (verbose) {
            printf("Component %u: %s (not Gaudi)\n", i, components[i]->name);
        }
    }

    if (!found_gaudi) {
        printf("\n❌ No Gaudi components found!\n");
        printf("   Make sure UCX is built with Gaudi support and the transport is loaded.\n");
        uct_release_component_list(components);
        return 1;
    }

    printf("\n✓ Gaudi component analysis completed\n");
    
    uct_release_component_list(components);
    return 0;
}
