# Gaudi Device Handle Management in UCX

## The Challenge: Exclusive Device Handles

The Habana Labs `hlthunk` library presents a unique challenge for device management within UCX. Unlike other accelerator drivers (like CUDA or ROCm), a call to `hlthunk_open()` for a specific Gaudi device will fail if that same device has already been opened within the same process.

This creates two critical problems:

1.  **Internal Conflict:** Multiple components within UCX need to interact with the same device. For example, the `gaudi_copy` transport (for memory operations) and the `gaudi_ipc` transport (for peer-to-peer communication) both require a handle to the same device. If they both try to call `hlthunk_open()`, the second call will fail, breaking UCX's initialization.

2.  **External Conflict:** An application using UCX (e.g., PyTorch) may open a Gaudi device for its own computational purposes *before* UCX is initialized. When UCX then tries to open the same device, the call will fail, making it impossible for the application and UCX to coexist.

## The Solution: A Unified Device Management Strategy

To solve these problems, we must implement a robust, centralized device management strategy that handles both internal and external conflicts gracefully. The solution has two main parts: a central, reference-counted device manager for internal sharing, and a mechanism to import handles from the application for external sharing.

### 1. Internal Sharing: The Reference-Counted Device Manager

To solve the internal conflict, we will ensure that `hlthunk_open()` is only called once per device within UCX.

-   **Central Manager:** A thread-safe manager will be created in `src/uct/gaudi/base/`. This manager will maintain a global cache of device handles and their corresponding reference counts.
-   **Wrapper Functions:** All direct calls to `hlthunk_open()` and `hlthunk_close()` in the Gaudi transports will be replaced with calls to new wrapper functions:
    -   `gaudi_device_get_handle(device_index)`: When called, it checks the cache.
        -   If the reference count for the device is 0, it calls the real `hlthunk_open()`, stores the handle, and sets the count to 1.
        -   If the count is > 0, it simply increments the count and returns the cached handle.
    -   `gaudi_device_put_handle(device_index)`: This function decrements the reference count. When the count reaches 0, it calls the real `hlthunk_close()`.

This ensures that multiple UCX components can share a single device handle without conflict.

### 2. External Sharing: Importing the Application's Handle

To solve the conflict with the application, UCX must provide a way for the application to pass in its already-opened device handle.

-   **Environment Variable:** The application can pass its file descriptor to UCX using a new environment variable:
    ```bash
    # Example for Gaudi device 0
    export UCX_GAUDI_DEVICE_FD_0=<file_descriptor>
    ```
-   **Using `dup()` for Safety:** The central device manager's `gaudi_device_get_handle()` logic will be enhanced:
    1.  Before attempting to open a device itself, it will first check if the `UCX_GAUDI_DEVICE_FD_...` environment variable is set for that device.
    2.  If the variable exists, the manager will use the `dup()` system call on the file descriptor provided by the application. `dup()` creates a new, independent file descriptor that points to the same underlying device.
    3.  This new, duplicated handle is then stored in the UCX internal cache.
-   **Why `dup()` is Critical:** Using `dup()` is essential for resource safety. When UCX is finalized and calls `gaudi_device_put_handle()`, it will eventually `close()` its duplicated handle. This does **not** affect the application's original handle, which remains open and valid. This prevents UCX from accidentally closing a resource that the application is still using.

### Complete Workflow

1.  An application opens Gaudi device 0 and gets file descriptor `5`.
2.  The application sets `export UCX_GAUDI_DEVICE_FD_0=5`.
3.  The application initializes UCX.
4.  A component within UCX calls `gaudi_device_get_handle(0)`.
5.  The manager sees the environment variable, calls `dup(5)`, and gets a new handle, `10`. It caches handle `10` and sets the reference count to 1.
6.  Another UCX component calls `gaudi_device_get_handle(0)`. The manager sees the reference count is > 0, increments it to 2, and returns the cached handle `10`.
7.  When UCX finalizes, both components call `gaudi_device_put_handle(0)`. The reference count is decremented twice. When it reaches 0, the manager calls `close(10)`.
8.  The application's original handle, `5`, remains unaffected and can continue to be used.

This design ensures that UCX can function correctly both as the primary owner of the device and as a library within a larger application that manages the device's lifetime.
