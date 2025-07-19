# Host-to-Device Memory Copy with UCX

This document explains how to perform a host-to-device memory copy on a single node using the two main layers of UCX: UCP (Unified Communication Protocol) and UCT (Unified Communication Transport).

## The Role of Memory Allocation

A fundamental design principle in UCX is the **separation of concerns**. UCX's responsibility is **communication**, not **memory allocation**.

-   **User's Responsibility:** The application developer allocates memory buffers using the appropriate tools for the target hardware (e.g., `malloc` for the CPU, `cudaMalloc` for NVIDIA GPUs).
-   **UCX's Responsibility:** The application then **registers** these pre-allocated buffers with UCX (`ucp_mem_map` or `uct_mem_reg`). This registration step informs UCX about the memory, allowing it to determine the most efficient way to access it for communication.

This design gives the developer maximum flexibility to use memory from any source while allowing UCX to focus on optimizing data transfer.

---

## Method 1: Using the UCP Layer (Recommended)

Using the UCP layer is simpler because it abstracts away low-level details. UCP automatically selects the best underlying transport (`cuda_copy`, `sm`, etc.) for the operation.

### Core Concepts of UCP

*   **Abstraction:** You do not need to manually discover or select memory domains or transports.
*   **Worker/Endpoint Model:** Communication is organized around a `ucp_worker_h` (progress engine) and a `ucp_ep_h` (a connection). For a single-node copy, the worker's endpoint connects to itself.
*   **Non-Blocking Operations:** Data transfers are non-blocking and return a request handle that must be progressed to completion.

### Steps and Call Flow with UCP

#### Step 1: Initialization

1.  **Set UCP Parameters:** Use `ucp_config_read()` and `ucp_params_t` to define required features, such as `UCP_FEATURE_RMA`.
2.  **Initialize UCP Context:** Call `ucp_init()`.
3.  **Create a UCP Worker:** Call `ucp_worker_create()`.

#### Step 2: Memory Management

1.  **Allocate Memory (Non-UCP):**
    *   `malloc()`: Allocate the source buffer in host memory.
    *   `cudaMalloc()`: Allocate the destination buffer in GPU device memory.
2.  **Register Memory with UCP:**
    *   `ucp_mem_map()`: Call this for both buffers, specifying the memory type (`UCP_MEMORY_TYPE_HOST` or `UCP_MEMORY_TYPE_CUDA`). This returns a `ucp_mem_h` for each.
3.  **Pack Remote Key:**
    *   `ucp_rkey_pack()`: Pack the destination buffer's `ucp_mem_h` into a `ucp_rkey_bundle_h`.

#### Step 3: Communication Setup

1.  **Get Worker Address:** Call `ucp_worker_get_address()`.
2.  **Create an Endpoint:** Call `ucp_ep_create()` to connect the worker to its own address.

#### Step 4: Data Transfer

1.  **Initiate Transfer:** Call `ucp_put_nbi()`, providing the endpoint, host buffer, remote device address, and the packed remote key bundle.
2.  **Progress and Wait:**
    *   The call returns a request handle.
    *   Repeatedly call `ucp_worker_progress()` until `ucp_request_check_status()` indicates the operation is complete.
    *   Free the handle with `ucp_request_free()`.

#### Step 5: Teardown

Clean up resources in reverse order: `ucp_rkey_buffer_release()`, `ucp_ep_destroy()`, `ucp_worker_destroy()`, `ucp_mem_unmap()`, `ucp_cleanup()`, and finally `free()`/`cudaFree()`.

### UCP Call Flow Summary
```
main()
  |
  +-> ucp_init()
  |
  +-> ucp_worker_create()
  |
  +-> malloc()             // Host buffer (non-UCP)
  +-> cudaMalloc()          // Device buffer (non-UCP)
  |
  +-> ucp_mem_map()          // Register host buffer -> host_mem_h
  +-> ucp_mem_map()          // Register device buffer -> device_mem_h
  |   |
  |   +-> ucp_rkey_pack()    // Pack device_mem_h -> rkey_bundle
  |
  +-> ucp_worker_get_address()
  |   |
  |   +-> ucp_ep_create()      // Endpoint to self
  |       |
  |       +-> request = ucp_put_nbi(ep, host_buf, len, device_addr, rkey_bundle)
  |       |
  |       +-> while (ucp_request_check_status(request) == UCS_INPROGRESS) {
  |       |       ucp_worker_progress();
  |       |   }
  |       |
  |       +-> ucp_request_free(request)
  |       |
  |       +-> ucp_ep_destroy()
  |
  +-> ... (cleanup all other resources)
```

---

## Method 2: Using the UCT Layer

Using the UCT layer provides more control but requires manual resource management.

### Core Concepts of UCT

*   **Memory Domain (MD):** Manages a specific type of memory (e.g., host or CUDA).
*   **Transport (TL):** A specific communication method (e.g., `cuda_copy`). You must manually find and select an appropriate MD and TL.
*   **Zero-Copy Operations:** Data transfers are performed with one-sided `uct_ep_put_zcopy` or `uct_ep_get_zcopy` calls.

### Steps and Call Flow with UCT

#### Step 1: Initialization and Resource Discovery

1.  **Create UCT Context:** Call `uct_context_init()`.
2.  **Find a Memory Domain (MD):** Use `uct_query_md_resources()` and `uct_md_query()` to find an MD that supports `UCT_MD_MEM_TYPE_CUDA`.
3.  **Find a Transport (TL):** On the chosen MD, use `uct_md_query_tl_resources()` to find a transport that supports the desired `put`/`get` capabilities (e.g., `cuda_copy`).

#### Step 2: Memory Management

1.  **Allocate Memory (Non-UCT):** Use `malloc()` and `cudaMalloc()`.
2.  **Register Memory with UCT:** Call `uct_mem_reg()` for both buffers to get a `uct_mem_h`.
3.  **Pack Remote Key:** Call `uct_md_mkey_pack()` on the destination buffer's handle to get a packed `rkey`.

#### Step 3: Communication Setup

1.  **Create a Worker:** Call `uct_worker_create()`.
2.  **Create an Interface:** Call `uct_iface_open()` on the chosen transport.
3.  **Create an Endpoint:** Call `uct_ep_create_connected()` to connect the interface to itself.

#### Step 4: Data Transfer

1.  **Initiate Transfer:** Call `uct_ep_put_zcopy()`, providing the endpoint, an I/O vector for the host buffer, the remote device address, the `rkey`, and a completion object.
2.  **Progress and Wait:** Repeatedly call `uct_worker_progress()` until the completion object is signaled.

#### Step 5: Teardown

Clean up resources in reverse order: `uct_ep_destroy()`, `uct_iface_close()`, `uct_rkey_destroy()`, `uct_mem_dereg()`, `uct_worker_destroy()`, `uct_md_close()`, `uct_context_destroy()`, and finally `free()`/`cudaFree()`.

### UCT Call Flow Summary
```
main()
  |
  +-> uct_context_init()
  |
  +-> uct_query_md_resources() -> Find MD with CUDA support
  |   |
  |   +-> uct_md_open()
  |       |
  |       +-> uct_md_query_tl_resources() -> Find 'cuda_copy' or 'sm' transport
  |
  +-> malloc()             // Host buffer
  +-> cudaMalloc()          // Device buffer
  |
  +-> uct_mem_reg()          // Register host buffer -> host_mem_h
  +-> uct_mem_reg()          // Register device buffer -> device_mem_h
  |   |
  |   +-> uct_md_mkey_pack() // Pack device_mem_h -> rkey
  |
  +-> uct_worker_create()
  |
  +-> uct_iface_open()
  |   |
  |   +-> uct_ep_create_connected() // Endpoint to self
  |       |
  |       +-> uct_ep_put_zcopy(ep, host_buf, device_addr, rkey, comp)
  |       |
  |       +-> while (!completed) { uct_worker_progress() }
  |       |
  |       +-> uct_ep_destroy()
  |
  +-> ... (cleanup all other resources)
```
