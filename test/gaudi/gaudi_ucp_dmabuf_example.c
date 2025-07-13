// Compile with: gcc -o gaudi_ucp_dmabuf_example gaudi_ucp_dmabuf_example.c -lucp -luct -lucs -lhlthunk

#include <ucp/api/ucp.h>
#include <uct/api/uct.h>
#include <ucs/memory/memory_type.h>
#include <hlthunk.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <errno.h>
#include <stdint.h>

#define PORT 13337
#define TEST_SIZE (64 * 1024)

typedef struct {
    uint64_t addr;
    size_t   length;
    char     rkey_buf[256];
    size_t   rkey_size;
    int      dmabuf_fd;
    uint8_t  ucp_addr[256];
    size_t   ucp_addr_len;
} mem_info_t;

// Helper: send/recv all bytes
static int send_all(int fd, const void *buf, size_t len) {
    size_t sent = 0;
    while (sent < len) {
        ssize_t n = send(fd, (const char*)buf + sent, len - sent, 0);
        if (n <= 0) return -1;
        sent += n;
    }
    return 0;
}
static int recv_all(int fd, void *buf, size_t len) {
    size_t recvd = 0;
    while (recvd < len) {
        ssize_t n = recv(fd, (char*)buf + recvd, len - recvd, 0);
        if (n <= 0) return -1;
        recvd += n;
    }
    return 0;
}

// Helper: TCP connect/accept
static int tcp_connect(const char *ip, int port) {
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    struct sockaddr_in addr = {0};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    inet_pton(AF_INET, ip, &addr.sin_addr);
    if (connect(fd, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        perror("connect");
        exit(1);
    }
    return fd;
}
static int tcp_accept(int port) {
    int listenfd = socket(AF_INET, SOCK_STREAM, 0);
    int opt = 1;
    setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    struct sockaddr_in addr = {0};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    addr.sin_addr.s_addr = INADDR_ANY;
    bind(listenfd, (struct sockaddr*)&addr, sizeof(addr));
    listen(listenfd, 1);
    int fd = accept(listenfd, NULL, NULL);
    close(listenfd);
    return fd;
}

int main(int argc, char **argv) {
    int is_server = 0;
    const char *peer_ip = NULL;
    size_t size = TEST_SIZE;
    if (argc > 1 && strcmp(argv[1], "server") == 0) {
        is_server = 1;
    } else if (argc > 2 && strcmp(argv[1], "client") == 0) {
        peer_ip = argv[2];
    } else {
        printf("Usage:\n  %s server\n  %s client <server_ip>\n", argv[0], argv[0]);
        return 1;
    }

    // 1. UCP context and worker
    ucp_params_t ucp_params = {0};
    ucp_context_h ucp_context;
    ucp_worker_h worker;
    ucp_config_t *config;
    ucp_params.field_mask = UCP_PARAM_FIELD_FEATURES;
    ucp_params.features = UCP_FEATURE_RMA | UCP_FEATURE_AM;
    ucp_config_read(NULL, NULL, &config);
    ucp_init(&ucp_params, config, &ucp_context);
    ucp_worker_params_t worker_params = {0};
    worker_params.field_mask = UCP_WORKER_PARAM_FIELD_THREAD_MODE;
    worker_params.thread_mode = UCS_THREAD_MODE_SINGLE;
    ucp_worker_create(ucp_context, &worker_params, &worker);

    // 2. Allocate Gaudi memory using UCT Gaudi MD
    uct_md_config_t *md_config = NULL;
    uct_md_h gaudi_md = NULL;
    uct_component_h *components = NULL;
    unsigned num_components = 0;
    ucs_status_t status;
    void *gaudi_addr = NULL;
    uct_mem_h memh_uct = NULL;

    status = uct_query_components(&components, &num_components);
    if (status != UCS_OK) {
        printf("Failed to query UCX components\n");
        return 1;
    }
    uct_component_h gaudi_comp = NULL;
    for (unsigned i = 0; i < num_components; ++i) {
        uct_component_attr_t attr = {.field_mask = UCT_COMPONENT_ATTR_FIELD_NAME};
        status = uct_component_query(components[i], &attr);
        if (status == UCS_OK && strcmp(attr.name, "gaudi_cpy") == 0) {
            gaudi_comp = components[i];
            break;
        }
    }
    if (!gaudi_comp) {
        printf("Gaudi component not found\n");
        uct_release_component_list(components);
        return 1;
    }
    status = uct_md_config_read(gaudi_comp, NULL, NULL, &md_config);
    if (status != UCS_OK) {
        printf("Failed to read Gaudi MD config\n");
        uct_release_component_list(components);
        return 1;
    }
    status = uct_md_open(gaudi_comp, NULL, md_config, &gaudi_md);
    uct_config_release(md_config);
    if (status != UCS_OK) {
        printf("Failed to open Gaudi MD\n");
        uct_release_component_list(components);
        return 1;
    }
    status = uct_md_mem_alloc(gaudi_md, &size, &gaudi_addr, UCS_MEMORY_TYPE_GAUDI, UCS_SYS_DEVICE_ID_UNKNOWN, 0, "gaudi_buf", &memh_uct);
    if (status != UCS_OK || !gaudi_addr) {
        printf("Failed to allocate Gaudi memory\n");
        uct_md_close(gaudi_md);
        uct_release_component_list(components);
        return 1;
    }
    memset(gaudi_addr, is_server ? 0 : 0xAB, size);
    uct_release_component_list(components);

    // 3. Register memory with UCP (for RMA)
    ucp_mem_map_params_t mem_params = {0};
    mem_params.field_mask = UCP_MEM_MAP_PARAM_FIELD_ADDRESS |
                            UCP_MEM_MAP_PARAM_FIELD_LENGTH |
                            UCP_MEM_MAP_PARAM_FIELD_MEMORY_TYPE;
    mem_params.address = gaudi_addr;
    mem_params.length = size;
    mem_params.memory_type = UCS_MEMORY_TYPE_HOST; // Use UCS_MEMORY_TYPE_GAUDI for real Gaudi
    ucp_mem_h memh;
    ucp_mem_map(ucp_context, &mem_params, &memh);

    // 4. Pack rkey
    mem_info_t local_info = {0};
    local_info.addr = (uint64_t)gaudi_addr;
    local_info.length = size;
    void *rkey_buf;
    size_t rkey_size;
    ucp_rkey_pack(ucp_context, memh, &rkey_buf, &rkey_size);
    memcpy(local_info.rkey_buf, rkey_buf, rkey_size);
    local_info.rkey_size = rkey_size;
    ucp_rkey_buffer_release(rkey_buf);

    // 5. Get UCP worker address
    ucp_address_t *ucp_addr;
    size_t ucp_addr_len;
    ucp_worker_get_address(worker, (ucp_address_t **)&ucp_addr, &ucp_addr_len);
    memcpy(local_info.ucp_addr, ucp_addr, ucp_addr_len);
    local_info.ucp_addr_len = ucp_addr_len;
    ucp_worker_release_address(worker, ucp_addr);

    // 6. Exchange info over TCP
    int sock = is_server ? tcp_accept(PORT) : tcp_connect(peer_ip, PORT);
    mem_info_t remote_info = {0};
    if (is_server) {
        recv_all(sock, &remote_info, sizeof(remote_info));
        send_all(sock, &local_info, sizeof(local_info));
    } else {
        send_all(sock, &local_info, sizeof(local_info));
        recv_all(sock, &remote_info, sizeof(remote_info));
    }

    // 7. Create UCP endpoint to peer
    ucp_ep_params_t ep_params = {0};
    ep_params.field_mask = UCP_EP_PARAM_FIELD_REMOTE_ADDRESS;
    ep_params.address = (const ucp_address_t *)remote_info.ucp_addr;
    ucp_ep_h ep;
    ucp_ep_create(worker, &ep_params, &ep);

    // 8. Unpack remote rkey
    ucp_rkey_h remote_rkey;
    ucp_ep_rkey_unpack(ep, remote_info.rkey_buf, &remote_rkey);

    // 9. RMA operation: client puts to server
    if (!is_server) {
        printf("Client: sending data to server...\n");
        ucp_request_param_t req_param = {0};
        void *req = ucp_put_nbx(ep, gaudi_addr, size, remote_info.addr, remote_rkey, &req_param);
        while (req != NULL && ucp_request_check_status(req) == UCS_INPROGRESS) {
            ucp_worker_progress(worker);
        }
        if (req != NULL) ucp_request_free(req);
        printf("Client: put complete.\n");
    } else {
        printf("Server: waiting for data...\n");
        sleep(2); // Give client time to send
        printf("Server: first 8 bytes: ");
        for (int i = 0; i < 8; ++i) printf("%02x ", ((unsigned char*)gaudi_addr)[i]);
        printf("\n");
    }

    // 10. Cleanup
    ucp_rkey_destroy(remote_rkey);
    ucp_ep_destroy(ep);
    ucp_mem_unmap(ucp_context, memh);
    ucp_worker_destroy(worker);
    ucp_cleanup(ucp_context);
    close(sock);
    free(gaudi_addr);

    printf("%s done.\n", is_server ? "Server" : "Client");
    return 0;
}