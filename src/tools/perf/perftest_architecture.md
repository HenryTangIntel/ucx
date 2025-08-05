# Perftest Architecture: UCP and UCT Test Support

`perftest` is fundamentally designed to support both UCP (Unified Communication Protocol) and UCT (Unified Communication Transport) level tests. This duality is at the core of its architecture, allowing developers to benchmark everything from raw transport performance (UCT) to high-level protocol efficiency (UCP).

This document explains how the architecture works and how a custom memory allocator, such as one for Gaudi dmabuf, fits into both testing scenarios.

### 1. The Core Architectural Abstraction

The entire `perftest` framework is built around a central abstraction: the `ucx_perf_test_t` structure. This can be thought of as a "test plugin" interface. This struct is defined in `perftest.h` and conceptually contains the following fields:

```c
typedef struct ucx_perf_test {
    const char             *name;         // Test name, e.g., "ucp_tag_bw"
    ucx_perf_api_t         api;           // API used: UCP or UCT
    ucx_perf_cmd_t         command;       // Test command: BW, LAT, etc.
    ucx_perf_test_type_t   test_type;     // Test type: SEND_RECV, ATOMIC, etc.

    // Function pointers for test-specific logic
    ucs_status_t (*init)(ucx_perf_context_t *perf);
    void         (*run)(ucx_perf_context_t *perf);
    void         (*cleanup)(ucx_perf_context_t *perf);
    // ... other function pointers
} ucx_perf_test_t;
```

The main test driver in `perftest_run.c` does not have hardcoded calls to UCP or UCT functions like `ucp_tag_send_nb` or `uct_ep_put_zcopy`. Instead, it operates on this abstraction, simply calling `test->run(perf_context)`.

The actual UCP or UCT logic is implemented in separate files that provide the concrete functions for these pointers:

-   **`lib/ucp_tests.cc`**: Contains the implementations for all UCP-based tests (`ucp_tag_bw`, `ucp_put_lat`, etc.).
-   **`lib/uct_tests.cc`**: Contains the implementations for all UCT-based tests (`uct_put_bw`, `uct_get_lat`, etc.).

### 2. How a Test is Selected and Run

When `perftest` is launched, command-line arguments are used to select which test implementation to run.

1.  **API Selection (`-a`)**: Selects the API layer.
    -   `-a ucp`: Use a test from `ucp_tests.cc`.
    -   `-a uct`: Use a test from `uct_tests.cc`.

2.  **Command Selection (`-c`)**: Selects the communication pattern.
    -   `-c bw`: Benchmark bandwidth.
    -   `-c lat`: Benchmark latency.

The framework maintains an array of all available `ucx_perf_test_t` structures and finds the one that matches the user's requested API, command, and other parameters.

### 3. The UCT Test Path (Low-Level)

When a UCT test is run (e.g., `perftest -a uct -c bw -m gaudi`):

1.  **Initialization**: The `init` function from a `uct_tests.cc` implementation is called. It creates low-level UCT objects: `uct_iface_h`, `uct_worker_h`, and a `uct_ep_h` connected directly to the peer's interface.

2.  **Memory Registration**: The custom memory allocator (e.g., `uct_perf_gaudi_alloc()`) is called. It allocates device memory, gets a dmabuf file descriptor, and uses `uct_md_mem_reg_v2()` to register it with the `uct_md_h` associated with the chosen interface. This returns a memory handle (`uct_mem_h`) and an associated remote key (`rkey`) which is exchanged with the peer.

3.  **The `run` Loop**: The `run` function from `uct_tests.cc` executes. Its main loop makes direct UCT calls like `uct_ep_put_zcopy()`, using the local `uct_mem_h` and the peer's `rkey` to perform zero-copy transfers. It drives communication with `uct_worker_progress()`.

This path measures the raw, best-case performance of the hardware and the UCT transport layer with minimal software overhead.

### 4. The UCP Test Path (High-Level)

When a UCP test is run (e.g., `perftest -a ucp -c bw -m gaudi`):

1.  **Initialization**: The `init` function from a `ucp_tests.cc` implementation is called. It creates high-level UCP objects: `ucp_context_h`, `ucp_worker_h`, and a reliable, connected `ucp_ep_h`. UCP handles the underlying UCT endpoint creation and wireup automatically.

2.  **Memory Registration**: The **exact same** custom allocator (`uct_perf_gaudi_alloc()`) is called. The resulting `uct_mem_h` is not used directly by the test logic. Instead, UCP is aware of all memory registered with the underlying UCT MDs it is using. When a pointer is passed to a UCP function, UCP automatically determines if the memory is registered and handles the translation.

3.  **The `run` Loop**: The `run` function from `ucp_tests.cc` executes. It makes high-level UCP calls like `ucp_put_nbi()`. The user provides a memory address, and UCP abstracts away the details of transport selection, `rkey` packing, and data transfer. It drives communication with the more complex `ucp_worker_progress()`.

This path measures the performance of a more realistic application scenario, including the overhead of UCP's reliability and protocol management.

### Summary: How a Custom Allocator Fits In

The memory allocation module (e.g., `gaudi_alloc.c`) is **decoupled** from the UCP vs. UCT test logic.

-   The **Allocator** is responsible for *how* memory is allocated and registered. It interfaces with the hardware driver (e.g., Gaudi) and the UCT Memory Domain (`uct_md_mem_reg_v2`).
-   The **Test Logic** (`ucp_tests.cc` / `uct_tests.cc`) is responsible for *what* is done with that memory (e.g., `put`, `get`, `send`).

By implementing a custom allocator correctly, both UCT and UCP tests can seamlessly use the target device memory. The UCT tests will use the resulting memory handle directly for raw performance measurements, while the UCP tests will leverage UCP's high-level abstractions to use the same memory for more complex, feature-rich communication patterns.
