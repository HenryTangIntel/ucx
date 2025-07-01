#include <ucp/api/ucp.h>
#include <uct/api/uct.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void check_status(ucs_status_t status, const char *msg) {
    if (status != UCS_OK) {
        fprintf(stderr, "%s: %s\n", msg, ucs_status_string(status));
        exit(1);
    }
}

int main() {
    ucp_params_t ucp_params;
    ucp_config_t *config;
    ucp_context_h ucp_context;
    ucp_worker_params_t worker_params;
    ucp_worker_h worker1, worker2;
    ucp_address_t *addr1, *addr2;
    size_t addr1_len, addr2_len;
    ucp_ep_params_t ep_params;
    ucp_ep_h ep1to2, ep2to1;

    // 1. Read UCX config
    check_status(ucp_config_read(NULL, NULL, &config), "ucp_config_read");

    // 2. Set up UCP context (enable both gaudi and rc for IB)
    setenv("UCX_TLS", "gaudi,rc", 1);
    memset(&ucp_params, 0, sizeof(ucp_params));
    ucp_params.field_mask = UCP_PARAM_FIELD_FEATURES;
    ucp_params.features = UCP_FEATURE_TAG | UCP_FEATURE_RMA;
    check_status(ucp_init(&ucp_params, config, &ucp_context), "ucp_init");

    // 3. Create two workers
    memset(&worker_params, 0, sizeof(worker_params));
    worker_params.field_mask = UCP_WORKER_PARAM_FIELD_THREAD_MODE;
    worker_params.thread_mode = UCS_THREAD_MODE_SINGLE;
    check_status(ucp_worker_create(ucp_context, &worker_params, &worker1), "ucp_worker_create worker1");
    check_status(ucp_worker_create(ucp_context, &worker_params, &worker2), "ucp_worker_create worker2");

    // 4. Get worker addresses
    check_status(ucp_worker_get_address(worker1, &addr1, &addr1_len), "ucp_worker_get_address worker1");
    check_status(ucp_worker_get_address(worker2, &addr2, &addr2_len), "ucp_worker_get_address worker2");

    // 5. Create endpoints
    memset(&ep_params, 0, sizeof(ep_params));
    ep_params.field_mask = UCP_EP_PARAM_FIELD_REMOTE_ADDRESS;
    ep_params.address = addr2;
    check_status(ucp_ep_create(worker1, &ep_params, &ep1to2), "ucp_ep_create worker1->worker2");

    memset(&ep_params, 0, sizeof(ep_params));
    ep_params.field_mask = UCP_EP_PARAM_FIELD_REMOTE_ADDRESS;
    ep_params.address = addr1;
    check_status(ucp_ep_create(worker2, &ep_params, &ep2to1), "ucp_ep_create worker2->worker1");

    printf("Endpoints created successfully!\n");

    // 6. Allocate memory on Gaudi using UCT MD API
    uct_component_h *uct_components;
    unsigned num_uct_components;
    check_status(uct_query_components(&uct_components, &num_uct_components), "uct_query_components");

    uct_md_config_t *md_config = NULL;
    uct_md_h gaudi_md = NULL;
    int found_gaudi = 0;
    for (unsigned i = 0; i < num_uct_components; ++i) {
        if (strcmp(uct_components[i]->name, "gaudi") == 0) {
            found_gaudi = 1;
            check_status(uct_md_config_read(uct_components[i], NULL, NULL, &md_config), "uct_md_config_read");
            check_status(uct_md_open(uct_components[i], "gaudi", md_config, &gaudi_md), "uct_md_open");
            uct_config_release(md_config);
            break;
        }
    }
    if (!found_gaudi) {
        fprintf(stderr, "Gaudi MD not found!\n");
        exit(1);
    }

    // 7. Allocate and register memory on Gaudi
    void *gaudi_addr = NULL;
    size_t length = 4096;
    uct_mem_h memh = NULL;
    ucs_status_t status = uct_md_mem_alloc(gaudi_md, &length, &gaudi_addr, UCS_MEMORY_TYPE_GAUDI, 0, "gaudi_alloc", &memh);
    check_status(status, "uct_md_mem_alloc (Gaudi)");

    printf("Allocated %zu bytes on Gaudi at %p\n", length, gaudi_addr);

    // 8. (Optional) Register the same memory with IB MD if you want to test cross-MD registration
    // This step is only possible if IB MD supports dma-buf and your system allows it.

    // 9. Clean up
    uct_md_mem_free(gaudi_md, memh);
    uct_md_close(gaudi_md);
    uct_release_component_list(uct_components);

    ucp_ep_destroy(ep1to2);
    ucp_ep_destroy(ep2to1);
    ucp_worker_release_address(worker1, addr1);
    ucp_worker_release_address(worker2, addr2);
    ucp_worker_destroy(worker1);
    ucp_worker_destroy(worker2);
    ucp_cleanup(ucp_context);
    ucp_config_release(config);

    printf("DMA-BUF test completed.\n");
    return 0;
}