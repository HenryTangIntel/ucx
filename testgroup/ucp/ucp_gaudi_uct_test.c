#include <ucp/api/ucp.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MSG "Hello from Gaudi UCT!"
#define MSG_SIZE 64

static void ucp_request_complete(void *request, ucs_status_t status)
{
    int *completed = (int*)request;
    *completed = 1;
}

static void check_status(ucs_status_t status, const char *msg)
{
    if (status != UCS_OK) {
        fprintf(stderr, "%s: %s\n", msg, ucs_status_string(status));
        exit(1);
    }
}

int main(int argc, char **argv)
{
    ucp_params_t ucp_params;
    ucp_config_t *config;
    ucp_context_h ucp_context;
    ucp_worker_params_t worker_params;
    ucp_worker_h ucp_worker;
    ucp_address_t *local_addr, *remote_addr;
    size_t local_addr_len, remote_addr_len;
    ucp_ep_params_t ep_params;
    ucp_ep_h ep;
    char msg[MSG_SIZE] = MSG;
    char recv_buf[MSG_SIZE] = {0};
    int is_sender = (argc > 1 && strcmp(argv[1], "send") == 0);

    // 1. Read UCX config
    check_status(ucp_config_read(NULL, NULL, &config), "ucp_config_read");

    // 2. Set up UCP context (force Gaudi UCT)
    setenv("UCX_TLS", "gaudi_copy", 1); // Use the correct transport name
    memset(&ucp_params, 0, sizeof(ucp_params));
    ucp_params.field_mask = UCP_PARAM_FIELD_FEATURES;
    ucp_params.features = UCP_FEATURE_TAG;
    check_status(ucp_init(&ucp_params, config, &ucp_context), "ucp_init");

    // 3. Create UCP worker
    memset(&worker_params, 0, sizeof(worker_params));
    worker_params.field_mask = UCP_WORKER_PARAM_FIELD_THREAD_MODE;
    worker_params.thread_mode = UCS_THREAD_MODE_SINGLE;
    check_status(ucp_worker_create(ucp_context, &worker_params, &ucp_worker), "ucp_worker_create");

    // 4. Get worker address
    check_status(ucp_worker_get_address(ucp_worker, &local_addr, &local_addr_len), "ucp_worker_get_address");

    // 5. Exchange addresses (simulate loopback for demo)
    remote_addr = local_addr;
    remote_addr_len = local_addr_len;

    // 6. Create endpoint
    memset(&ep_params, 0, sizeof(ep_params));
    ep_params.field_mask = UCP_EP_PARAM_FIELD_REMOTE_ADDRESS;
    ep_params.address = remote_addr;
    check_status(ucp_ep_create(ucp_worker, &ep_params, &ep), "ucp_ep_create");

    // 7. Send/Recv
    if (is_sender) {
        int send_completed = 0;
        void *req = ucp_tag_send_nb(ep, msg, MSG_SIZE, ucp_dt_make_contig(1), 0x1337, ucp_request_complete);
        if (UCS_PTR_IS_ERR(req)) {
            fprintf(stderr, "ucp_tag_send_nb failed\n");
            exit(1);
        } else if (req != NULL) {
            while (!send_completed)
                ucp_worker_progress(ucp_worker);
        }
        printf("Sender: Message sent: %s\n", msg);
    } else {
        int recv_completed = 0;
        void *req = ucp_tag_recv_nb(ucp_worker, recv_buf, MSG_SIZE, ucp_dt_make_contig(1), 0x1337, 0xffff, ucp_request_complete);
        if (UCS_PTR_IS_ERR(req)) {
            fprintf(stderr, "ucp_tag_recv_nb failed\n");
            exit(1);
        } else if (req != NULL) {
            while (!recv_completed)
                ucp_worker_progress(ucp_worker);
        }
        printf("Receiver: Message received: %s\n", recv_buf);
    }

    // 8. Cleanup
    ucp_ep_destroy(ep);
    ucp_worker_release_address(ucp_worker, local_addr);
    ucp_worker_destroy(ucp_worker);
    ucp_cleanup(ucp_context);
    ucp_config_release(config);

    return 0;
}
