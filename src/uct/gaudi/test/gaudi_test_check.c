/**
* Copyright (c) 2023-2024 Intel Corporation
*
* See file LICENSE for terms.
*/

#include <uct/api/uct.h>
#include <ucs/api/ucs.h>
#include <stdio.h>
#include <string.h>

#define GAUDI_MD_NAME "gaudi"
#define GAUDI_COPY_TL_NAME "gaudi_copy"

int main(int argc, char **argv) {
    ucs_status_t status;
    uct_md_h md;
    uct_worker_h worker;
    uct_iface_h iface;
    uct_context_h context;
    uct_md_config_t *md_config;
    uct_iface_config_t *iface_config;
    unsigned i, k;
    int ret = 1; // Default to error
    int gaudi_md_found = 0;
    int gaudi_copy_tl_found = 0;

    printf("=== Gaudi UCT Module Basic Check ===\n");

    /* Initialize context */
    uct_context_config_t *context_config;
    status = uct_config_read(NULL, NULL, &context_config);
    if (status != UCS_OK) {
        fprintf(stderr, "Failed to read UCT context config: %s\n", ucs_status_string(status));
        return 1;
    }

    status = uct_init_context(&context, context_config);
    uct_config_release(context_config);
    if (status != UCS_OK) {
        fprintf(stderr, "Failed to initialize UCT context: %s\n", ucs_status_string(status));
        return 1;
    }
    printf("UCT context initialized.\n");

    /* Create a worker */
    uct_worker_params_t worker_params = {
        .field_mask = UCT_WORKER_PARAM_FIELD_THREAD_MODE,
        .thread_mode = UCS_THREAD_MODE_SINGLE
    };
    status = uct_worker_create(context, &worker_params, &worker);
    if (status != UCS_OK) {
        fprintf(stderr, "Failed to create UCT worker: %s\n", ucs_status_string(status));
        uct_cleanup_context(context);
        return 1;
    }
    printf("UCT worker created.\n");

    /* Query for MD resources */
    uct_md_resource_desc_t *md_resources;
    unsigned num_md_resources;
    status = uct_query_md_resources(context, &md_resources, &num_md_resources);
    if (status != UCS_OK) {
        fprintf(stderr, "Failed to query MD resources: %s\n", ucs_status_string(status));
        goto cleanup_worker;
    }
    printf("Found %u MD resource(s):\n", num_md_resources);

    for (i = 0; i < num_md_resources; ++i) {
        printf("  MD[%u]: %s\n", i, md_resources[i].md_name);
        if (strcmp(md_resources[i].md_name, GAUDI_MD_NAME) == 0) {
            gaudi_md_found = 1;
            printf("    Found '%s' MD.\n", GAUDI_MD_NAME);

            /* Open the Gaudi MD */
            status = uct_md_config_read(md_resources[i].md_name, NULL, NULL, &md_config);
            if (status != UCS_OK) {
                fprintf(stderr, "Failed to read config for MD %s: %s\n",
                        md_resources[i].md_name, ucs_status_string(status));
                continue;
            }
            status = uct_md_open(context, md_resources[i].md_name, md_config, &md);
            uct_config_release(md_config);
            if (status != UCS_OK) {
                fprintf(stderr, "Failed to open MD %s: %s\n",
                        md_resources[i].md_name, ucs_status_string(status));
                continue;
            }
            printf("    Successfully opened '%s' MD.\n", GAUDI_MD_NAME);

            /* Query for TL resources on this MD */
            uct_tl_resource_desc_t *tl_resources;
            unsigned num_tl_resources;
            status = uct_md_query_tl_resources(md, &tl_resources, &num_tl_resources);
            if (status != UCS_OK) {
                fprintf(stderr, "Failed to query TL resources for MD %s: %s\n",
                        md_resources[i].md_name, ucs_status_string(status));
                uct_md_close(md);
                continue;
            }
            printf("    Found %u TL resource(s) on MD '%s':\n", num_tl_resources, md_resources[i].md_name);

            for (k = 0; k < num_tl_resources; ++k) {
                printf("      TL[%u] on MD '%s': %s (device: %s)\n", k, md_resources[i].md_name,
                       tl_resources[k].tl_name, tl_resources[k].dev_name);
                // Note: The gaudi_copy TL is associated with uct_gaudi_copy_component,
                // which uses uct_gaudi_query_md_resources. This means the MD name found
                // by uct_query_md_resources might be "gaudi0" (from uct_gaudi_md), and
                // the TL resource found on *that* MD could be "gaudi_copy".
                // Or, uct_query_md_resources might also list "gaudi_copy0" if
                // uct_gaudi_copy_component also registers its own MD resource names.
                // The current implementation of uct_gaudi_copy_component's query_md_resources
                // reuses uct_gaudi_query_md_resources, so it will also report "gaudi0".
                // We need to check if this "gaudi0" MD supports the "gaudi_copy" transport.
                if (strcmp(tl_resources[k].tl_name, GAUDI_COPY_TL_NAME) == 0) {
                    gaudi_copy_tl_found = 1;
                    printf("        Found '%s' TL on MD '%s'.\n", GAUDI_COPY_TL_NAME, md_resources[i].md_name);

                    // Try to open an interface on this TL
                    uct_iface_params_t iface_params;
                    memset(&iface_params, 0, sizeof(iface_params));
                    iface_params.field_mask      = UCT_IFACE_PARAM_FIELD_OPEN_MODE |
                                                   UCT_IFACE_PARAM_FIELD_DEVICE;
                    iface_params.open_mode       = UCT_IFACE_OPEN_MODE_DEVICE;
                    strncpy(iface_params.device.name, tl_resources[k].dev_name,
                            sizeof(iface_params.device.name) - 1);

                    status = uct_iface_config_read(tl_resources[k].tl_name, NULL, NULL, &iface_config);
                    if (status != UCS_OK) {
                        fprintf(stderr, "Failed to read config for TL %s: %s\n",
                                tl_resources[k].tl_name, ucs_status_string(status));
                        continue;
                    }

                    status = uct_iface_open(md, worker, &iface_params, iface_config, &iface);
                    uct_config_release(iface_config);
                    if (status != UCS_OK) {
                        fprintf(stderr, "Failed to open iface for TL %s on MD %s, device %s: %s\n",
                                tl_resources[k].tl_name, md_resources[i].md_name,
                                tl_resources[k].dev_name, ucs_status_string(status));
                    } else {
                        printf("        Successfully opened iface for '%s' TL.\n", GAUDI_COPY_TL_NAME);
                        uct_iface_close(iface);
                    }
                }
            }
            uct_release_tl_resource_list(tl_resources);
            uct_md_close(md);
        }
    }
    uct_release_md_resource_list(md_resources);

    if (gaudi_md_found && gaudi_copy_tl_found) {
        printf("\nSUCCESS: Found '%s' MD and '%s' TL.\n", GAUDI_MD_NAME, GAUDI_COPY_TL_NAME);
        ret = 0; // Success
    } else {
        if (!gaudi_md_found) {
            fprintf(stderr, "\nERROR: Did not find '%s' MD.\n", GAUDI_MD_NAME);
        }
        if (!gaudi_copy_tl_found) {
            fprintf(stderr, "\nERROR: Did not find '%s' TL on a Gaudi MD.\n", GAUDI_COPY_TL_NAME);
        }
        ret = 1; // Error
    }

cleanup_worker:
    uct_worker_destroy(worker);
    printf("UCT worker destroyed.\n");
//cleanup_context: // Label not strictly needed due to structure
    uct_cleanup_context(context);
    printf("UCT context cleaned up.\n");

    printf("=== Check Complete (exit code %d) ===\n", ret);
    return ret;
}
