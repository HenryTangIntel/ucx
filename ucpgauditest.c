#include <ucp/api/ucp.h>

int main() {
    ucs_status_t status;
    ucp_config_t *config;
    ucp_params_t ucp_params = {0};
    ucp_context_h context;
    ucp_worker_h worker;
    ucp_worker_params_t worker_params = {0};

    // Load UCP config explicitly setting transport
    status = ucp_config_read(NULL, NULL, &config);
    if (status != UCS_OK) return -1;

    ucp_config_modify(config, "TLS", "gaudi");

    ucp_params.field_mask = UCP_PARAM_FIELD_FEATURES;
    ucp_params.features = UCP_FEATURE_TAG;

    // Create UCP context
    status = ucp_init(&ucp_params, config, &context);
    if (status != UCS_OK) return -1;

    ucp_config_release(config);

    // Create UCP worker
    worker_params.field_mask  = UCP_WORKER_PARAM_FIELD_THREAD_MODE;
    worker_params.thread_mode = UCS_THREAD_MODE_SINGLE;

    status = ucp_worker_create(context, &worker_params, &worker);
    if (status != UCS_OK) return -1;

    printf("UCP initialized successfully using Gaudi transport.\n");

    ucp_worker_destroy(worker);
    ucp_cleanup(context);
    return 0;
}

