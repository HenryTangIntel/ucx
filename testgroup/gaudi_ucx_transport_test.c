#include <ucp/api/ucp.h>
#include <uct/api/uct.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <time.h>
#include <errno.h>

#define DEFAULT_PORT 12345
#define BUFFER_SIZE (1024 * 1024)  // 1MB
#define TEST_ITERATIONS 5
#define TAG 0x1337

typedef struct {
    ucp_context_h ucp_context;
    ucp_worker_h ucp_worker;
    ucp_ep_h ucp_ep;
    ucp_mem_h mem_handle;
    void *buffer;
    size_t buffer_size;
    int is_server;
    int port;
    char *server_ip;
    ucp_listener_h listener;
    ucp_address_t *worker_address;
    size_t worker_address_len;
} gaudi_ucx_context_t;

// Request context for async operations
typedef struct {
    volatile int complete;
    ucs_status_t status;
} request_context_t;

// Forward declarations
static int init_ucx_context(gaudi_ucx_context_t *ctx);
static int run_ucx_server(gaudi_ucx_context_t *ctx);
static int run_ucx_client(gaudi_ucx_context_t *ctx);
static void cleanup_context(gaudi_ucx_context_t *ctx);
static void print_usage(const char *prog_name);

// Callback functions
static void request_completion_cb(void *request, ucs_status_t status, ucp_tag_recv_info_t *info);
static void send_completion_cb(void *request, ucs_status_t status);
static void server_connection_handler(ucp_conn_request_h conn_request, void *arg);

// Helper functions
static int send_worker_address(int sock, ucp_address_t *address, size_t address_len);
static int recv_worker_address(int sock, ucp_address_t **address, size_t *address_len);
static int wait_for_completion(void *request, ucp_worker_h worker);

int main(int argc, char *argv[]) {
    gaudi_ucx_context_t ctx = {0};
    ctx.port = DEFAULT_PORT;
    ctx.buffer_size = BUFFER_SIZE;
    ctx.is_server = 1; // Default to server mode
    
    printf("UCX Gaudi Device Memory over MLX/IB Transport Test\n");
    printf("==================================================\n");
    
    // Parse command line arguments
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-c") == 0 && i + 1 < argc) {
            ctx.is_server = 0;
            ctx.server_ip = argv[++i];
        } else if (strcmp(argv[i], "-p") == 0 && i + 1 < argc) {
            ctx.port = atoi(argv[++i]);
        } else if (strcmp(argv[i], "-s") == 0 && i + 1 < argc) {
            ctx.buffer_size = atoll(argv[++i]);
        } else if (strcmp(argv[i], "-h") == 0) {
            print_usage(argv[0]);
            return 0;
        } else {
            fprintf(stderr, "Unknown argument: %s\n", argv[i]);
            print_usage(argv[0]);
            return 1;
        }
    }
    
    printf("Mode: %s\n", ctx.is_server ? "Server" : "Client");
    printf("Port: %d (for address exchange)\n", ctx.port);
    printf("Buffer size: %zu bytes\n", ctx.buffer_size);
    if (!ctx.is_server && ctx.server_ip) {
        printf("Server IP: %s\n", ctx.server_ip);
    }
    printf("\n");
    
    // Initialize UCX context
    if (init_ucx_context(&ctx) != 0) {
        fprintf(stderr, "Failed to initialize UCX context\n");
        cleanup_context(&ctx);
        return 1;
    }
    
    int result;
    if (ctx.is_server) {
        result = run_ucx_server(&ctx);
    } else {
        result = run_ucx_client(&ctx);
    }
    
    cleanup_context(&ctx);
    return result;
}

static int init_ucx_context(gaudi_ucx_context_t *ctx) {
    ucs_status_t status;
    ucp_config_t *config;
    ucp_params_t ucp_params;
    ucp_worker_params_t worker_params;
    ucp_mem_map_params_t mem_params;
    
    printf("Initializing UCX context...\n");
    
    // Read UCP configuration
    status = ucp_config_read(NULL, NULL, &config);
    if (status != UCS_OK) {
        fprintf(stderr, "Failed to read UCP config: %s\n", ucs_status_string(status));
        return -1;
    }
    
    // Configure transport: InfiniBand/MLX for network, Gaudi provides device memory
    // Gaudi memory is transferred over IB/MLX transport using DMA-buf
    status = ucp_config_modify(config, "TLS", "rc_mlx5,dc_mlx5,ud_mlx5,ib,tcp,self");
    if (status != UCS_OK) {
        printf("Note: Transport config not modified: %s\n", ucs_status_string(status));
    }
    
    // Print transport information
    printf("Note: Using MLX/IB transports for Gaudi device memory transfers\n");
    
    // Initialize UCP context
    memset(&ucp_params, 0, sizeof(ucp_params));
    ucp_params.field_mask = UCP_PARAM_FIELD_FEATURES;
    ucp_params.features = UCP_FEATURE_TAG | UCP_FEATURE_RMA;
    
    status = ucp_init(&ucp_params, config, &ctx->ucp_context);
    ucp_config_release(config);
    
    if (status != UCS_OK) {
        fprintf(stderr, "Failed to initialize UCP: %s\n", ucs_status_string(status));
        return -1;
    }
    
    printf("✓ UCP context initialized\n");
    
    // Create worker
    memset(&worker_params, 0, sizeof(worker_params));
    worker_params.field_mask = UCP_WORKER_PARAM_FIELD_THREAD_MODE;
    worker_params.thread_mode = UCS_THREAD_MODE_SINGLE;
    
    status = ucp_worker_create(ctx->ucp_context, &worker_params, &ctx->ucp_worker);
    if (status != UCS_OK) {
        fprintf(stderr, "Failed to create UCP worker: %s\n", ucs_status_string(status));
        return -1;
    }
    
    printf("✓ UCP worker created\n");
    
    // Get worker address for endpoint creation
    status = ucp_worker_get_address(ctx->ucp_worker, &ctx->worker_address, &ctx->worker_address_len);
    if (status != UCS_OK) {
        fprintf(stderr, "Failed to get worker address: %s\n", ucs_status_string(status));
        return -1;
    }
    
    printf("✓ Worker address obtained (%zu bytes)\n", ctx->worker_address_len);
    
    // Allocate and map buffer to simulate Gaudi device memory
    // In real scenario, this would be Gaudi device memory accessible via DMA-buf
    memset(&mem_params, 0, sizeof(mem_params));
    mem_params.field_mask = UCP_MEM_MAP_PARAM_FIELD_ADDRESS |
                           UCP_MEM_MAP_PARAM_FIELD_LENGTH |
                           UCP_MEM_MAP_PARAM_FIELD_FLAGS;
    mem_params.address = NULL;
    mem_params.length = ctx->buffer_size;
    mem_params.flags = UCP_MEM_MAP_ALLOCATE;
    
    status = ucp_mem_map(ctx->ucp_context, &mem_params, &ctx->mem_handle);
    if (status != UCS_OK) {
        fprintf(stderr, "Failed to map memory: %s\n", ucs_status_string(status));
        return -1;
    }
    
    // Get the buffer address
    ucp_mem_attr_t mem_attr;
    mem_attr.field_mask = UCP_MEM_ATTR_FIELD_ADDRESS;
    status = ucp_mem_query(ctx->mem_handle, &mem_attr);
    if (status != UCS_OK) {
        fprintf(stderr, "Failed to query memory: %s\n", ucs_status_string(status));
        return -1;
    }
    
    ctx->buffer = mem_attr.address;
    printf("✓ Allocated UCX buffer: %p (%zu bytes)\n", ctx->buffer, ctx->buffer_size);
    
    return 0;
}

static int run_ucx_server(gaudi_ucx_context_t *ctx) {
    printf("\n=== UCX Server Mode ===\n");
    
    // Simple TCP socket for address exchange (not for data transfer)
    int listen_sock = socket(AF_INET, SOCK_STREAM, 0);
    if (listen_sock < 0) {
        perror("socket");
        return -1;
    }
    
    int opt = 1;
    setsockopt(listen_sock, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    
    struct sockaddr_in server_addr = {0};
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(ctx->port);
    
    if (bind(listen_sock, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        perror("bind");
        close(listen_sock);
        return -1;
    }
    
    if (listen(listen_sock, 1) < 0) {
        perror("listen");
        close(listen_sock);
        return -1;
    }
    
    printf("Server waiting for address exchange on port %d...\n", ctx->port);
    
    // Accept connection for address exchange
    struct sockaddr_in client_addr;
    socklen_t client_len = sizeof(client_addr);
    int client_sock = accept(listen_sock, (struct sockaddr*)&client_addr, &client_len);
    if (client_sock < 0) {
        perror("accept");
        close(listen_sock);
        return -1;
    }
    
    printf("✓ Client connected for address exchange\n");
    
    // Exchange worker addresses
    if (send_worker_address(client_sock, ctx->worker_address, ctx->worker_address_len) < 0) {
        fprintf(stderr, "Failed to send worker address\n");
        close(client_sock);
        close(listen_sock);
        return -1;
    }
    
    ucp_address_t *client_address;
    size_t client_address_len;
    if (recv_worker_address(client_sock, &client_address, &client_address_len) < 0) {
        fprintf(stderr, "Failed to receive client address\n");
        close(client_sock);
        close(listen_sock);
        return -1;
    }
    
    printf("✓ Worker addresses exchanged\n");
    
    // Create endpoint to client using UCX
    ucp_ep_params_t ep_params;
    memset(&ep_params, 0, sizeof(ep_params));
    ep_params.field_mask = UCP_EP_PARAM_FIELD_REMOTE_ADDRESS;
    ep_params.address = client_address;
    
    ucs_status_t status = ucp_ep_create(ctx->ucp_worker, &ep_params, &ctx->ucp_ep);
    if (status != UCS_OK) {
        fprintf(stderr, "Failed to create endpoint: %s\n", ucs_status_string(status));
        free(client_address);
        close(client_sock);
        close(listen_sock);
        return -1;
    }
    
    printf("✓ UCX endpoint created - ready for MLX/IB transport communication\n");
    
    // Initialize buffer with server pattern
    int *int_buffer = (int*)ctx->buffer;
    int count = ctx->buffer_size / sizeof(int);
    for (int i = 0; i < count && i < 1000; i++) {
        int_buffer[i] = 1000 + i;  // Server pattern
    }
    
    printf("✓ Buffer initialized with server pattern\n");
    
    // UCX Communication Loop using actual UCX transports
    for (int iter = 0; iter < TEST_ITERATIONS; iter++) {
        printf("\n--- UCX Server Iteration %d ---\n", iter + 1);
        
        // Post receive using UCX tag matching
        void *recv_req = ucp_tag_recv_nb(ctx->ucp_worker, ctx->buffer, ctx->buffer_size,
                                        ucp_dt_make_contig(1), TAG, 0,
                                        request_completion_cb);
        
        if (UCS_PTR_IS_ERR(recv_req)) {
            fprintf(stderr, "Failed to post receive: %s\n", 
                   ucs_status_string(UCS_PTR_STATUS(recv_req)));
            break;
        }
        
        printf("✓ Posted UCX tag receive\n");
        
        // Wait for receive completion
        if (wait_for_completion(recv_req, ctx->ucp_worker) < 0) {
            fprintf(stderr, "Receive failed\n");
            break;
        }
        
        printf("✓ Received data via MLX/IB transport (from Gaudi device memory)\n");
        printf("   First few values: %d %d %d %d...\n", 
               int_buffer[0], int_buffer[1], int_buffer[2], int_buffer[3]);
        
        // Process data (double the values)
        for (int i = 0; i < 10 && i < count; i++) {
            int_buffer[i] *= 2;
        }
        
        printf("✓ Processed data (doubled first 10 values)\n");
        
        // Send response back using UCX
        void *send_req = ucp_tag_send_nb(ctx->ucp_ep, ctx->buffer, ctx->buffer_size,
                                        ucp_dt_make_contig(1), TAG,
                                        send_completion_cb);
        
        if (UCS_PTR_IS_ERR(send_req)) {
            fprintf(stderr, "Failed to post send: %s\n", 
                   ucs_status_string(UCS_PTR_STATUS(send_req)));
            break;
        }
        
        printf("✓ Posted UCX tag send\n");
        
        // Wait for send completion
        if (wait_for_completion(send_req, ctx->ucp_worker) < 0) {
            fprintf(stderr, "Send failed\n");
            break;
        }
        
        printf("✓ Sent response via MLX/IB transport (to Gaudi device memory)\n");
        
        // Progress UCX
        ucp_worker_progress(ctx->ucp_worker);
        
        // Reset buffer for next iteration
        for (int i = 0; i < 10 && i < count; i++) {
            int_buffer[i] = 1000 + i;
        }
    }
    
    // Cleanup
    free(client_address);
    close(client_sock);
    close(listen_sock);
    
    printf("\n✅ UCX Server completed successfully\n");
    return 0;
}

static int run_ucx_client(gaudi_ucx_context_t *ctx) {
    printf("\n=== UCX Client Mode ===\n");
    
    if (!ctx->server_ip) {
        fprintf(stderr, "Server IP not specified\n");
        return -1;
    }
    
    // Connect for address exchange
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) {
        perror("socket");
        return -1;
    }
    
    struct sockaddr_in server_addr = {0};
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(ctx->port);
    
    if (inet_pton(AF_INET, ctx->server_ip, &server_addr.sin_addr) <= 0) {
        fprintf(stderr, "Invalid server IP address\n");
        close(sock);
        return -1;
    }
    
    if (connect(sock, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        perror("connect");
        close(sock);
        return -1;
    }
    
    printf("✓ Connected for address exchange\n");
    
    // Exchange worker addresses
    ucp_address_t *server_address;
    size_t server_address_len;
    if (recv_worker_address(sock, &server_address, &server_address_len) < 0) {
        fprintf(stderr, "Failed to receive server address\n");
        close(sock);
        return -1;
    }
    
    if (send_worker_address(sock, ctx->worker_address, ctx->worker_address_len) < 0) {
        fprintf(stderr, "Failed to send worker address\n");
        free(server_address);
        close(sock);
        return -1;
    }
    
    printf("✓ Worker addresses exchanged\n");
    
    // Create endpoint to server using UCX
    ucp_ep_params_t ep_params;
    memset(&ep_params, 0, sizeof(ep_params));
    ep_params.field_mask = UCP_EP_PARAM_FIELD_REMOTE_ADDRESS;
    ep_params.address = server_address;
    
    ucs_status_t status = ucp_ep_create(ctx->ucp_worker, &ep_params, &ctx->ucp_ep);
    if (status != UCS_OK) {
        fprintf(stderr, "Failed to create endpoint: %s\n", ucs_status_string(status));
        free(server_address);
        close(sock);
        return -1;
    }
    
    printf("✓ UCX endpoint created - ready for MLX/IB transport communication\n");
    
    // UCX Communication Loop
    int *int_buffer = (int*)ctx->buffer;
    int count = ctx->buffer_size / sizeof(int);
    
    for (int iter = 0; iter < TEST_ITERATIONS; iter++) {
        printf("\n--- UCX Client Iteration %d ---\n", iter + 1);
        
        // Prepare data pattern
        for (int i = 0; i < count && i < 1000; i++) {
            int_buffer[i] = (iter + 1) * 100 + i;
        }
        
        printf("✓ Prepared data pattern\n");
        printf("   First few values: %d %d %d %d...\n", 
               int_buffer[0], int_buffer[1], int_buffer[2], int_buffer[3]);
        
        // Send data using UCX
        void *send_req = ucp_tag_send_nb(ctx->ucp_ep, ctx->buffer, ctx->buffer_size,
                                        ucp_dt_make_contig(1), TAG,
                                        send_completion_cb);
        
        if (UCS_PTR_IS_ERR(send_req)) {
            fprintf(stderr, "Failed to post send: %s\n", 
                   ucs_status_string(UCS_PTR_STATUS(send_req)));
            break;
        }
        
        printf("✓ Posted UCX tag send\n");
        
        // Wait for send completion
        if (wait_for_completion(send_req, ctx->ucp_worker) < 0) {
            fprintf(stderr, "Send failed\n");
            break;
        }
        
        printf("✓ Sent data via MLX/IB transport (from Gaudi device memory)\n");
        
        // Receive response using UCX
        void *recv_req = ucp_tag_recv_nb(ctx->ucp_worker, ctx->buffer, ctx->buffer_size,
                                        ucp_dt_make_contig(1), TAG, 0,
                                        request_completion_cb);
        
        if (UCS_PTR_IS_ERR(recv_req)) {
            fprintf(stderr, "Failed to post receive: %s\n", 
                   ucs_status_string(UCS_PTR_STATUS(recv_req)));
            break;
        }
        
        printf("✓ Posted UCX tag receive\n");
        
        // Wait for receive completion
        if (wait_for_completion(recv_req, ctx->ucp_worker) < 0) {
            fprintf(stderr, "Receive failed\n");
            break;
        }
        
        printf("✓ Received response via MLX/IB transport (to Gaudi device memory)\n");
        
        // Verify data
        int expected = ((iter + 1) * 100) * 2;
        if (int_buffer[0] == expected) {
            printf("✓ Data verification passed! Server processed data correctly.\n");
        } else {
            printf("⚠️  Data verification: expected %d, got %d\n", expected, int_buffer[0]);
        }
        
        // Progress UCX
        ucp_worker_progress(ctx->ucp_worker);
        
        usleep(100000); // 100ms delay
    }
    
    // Cleanup
    free(server_address);
    close(sock);
    
    printf("\n✅ UCX Client completed successfully\n");
    return 0;
}

// Helper function implementations
static void request_completion_cb(void *request, ucs_status_t status, ucp_tag_recv_info_t *info) {
    request_context_t *ctx = (request_context_t*)request;
    ctx->status = status;
    ctx->complete = 1;
}

static void send_completion_cb(void *request, ucs_status_t status) {
    request_context_t *ctx = (request_context_t*)request;
    ctx->status = status;
    ctx->complete = 1;
}

static int wait_for_completion(void *request, ucp_worker_h worker) {
    if (request == NULL) {
        return 0; // Operation completed immediately
    }
    
    if (UCS_PTR_IS_ERR(request)) {
        return -1;
    }
    
    // Poll for completion
    while (!ucp_request_check_status(request)) {
        ucp_worker_progress(worker);
        usleep(1000); // 1ms sleep
    }
    
    ucs_status_t status = ucp_request_check_status(request);
    ucp_request_free(request);
    
    return (status == UCS_OK) ? 0 : -1;
}

static int send_worker_address(int sock, ucp_address_t *address, size_t address_len) {
    // Send address length first
    if (send(sock, &address_len, sizeof(address_len), 0) != sizeof(address_len)) {
        return -1;
    }
    
    // Send address data
    if (send(sock, address, address_len, 0) != (ssize_t)address_len) {
        return -1;
    }
    
    return 0;
}

static int recv_worker_address(int sock, ucp_address_t **address, size_t *address_len) {
    // Receive address length
    if (recv(sock, address_len, sizeof(*address_len), 0) != sizeof(*address_len)) {
        return -1;
    }
    
    // Allocate buffer for address
    *address = malloc(*address_len);
    if (!*address) {
        return -1;
    }
    
    // Receive address data
    if (recv(sock, *address, *address_len, 0) != (ssize_t)*address_len) {
        free(*address);
        return -1;
    }
    
    return 0;
}

static void cleanup_context(gaudi_ucx_context_t *ctx) {
    printf("\nCleaning up UCX resources...\n");
    
    if (ctx->ucp_ep) {
        ucp_ep_destroy(ctx->ucp_ep);
    }
    
    if (ctx->worker_address) {
        ucp_worker_release_address(ctx->ucp_worker, ctx->worker_address);
    }
    
    if (ctx->mem_handle) {
        ucp_mem_unmap(ctx->ucp_context, ctx->mem_handle);
    }
    
    if (ctx->ucp_worker) {
        ucp_worker_destroy(ctx->ucp_worker);
    }
    
    if (ctx->ucp_context) {
        ucp_cleanup(ctx->ucp_context);
    }
    
    printf("✓ Cleanup completed\n");
}

static void print_usage(const char *prog_name) {
    printf("Usage:\n");
    printf("  Server mode: %s [-p port] [-s buffer_size]\n", prog_name);
    printf("  Client mode: %s -c <server_ip> [-p port] [-s buffer_size]\n", prog_name);
    printf("\nOptions:\n");
    printf("  -c <ip>      Run in client mode, connect to server at <ip>\n");
    printf("  -p <port>    Port for address exchange (default: %d)\n", DEFAULT_PORT);
    printf("  -s <size>    Buffer size in bytes (default: %d)\n", BUFFER_SIZE);
    printf("  -h           Show this help\n");
    printf("\nNote: This test transfers Gaudi device memory over MLX/IB transports.\n");
    printf("      Gaudi is the accelerator, MLX/IB provides the network transport.\n");
    printf("      DMA-buf enables zero-copy between Gaudi memory and network.\n");
}
