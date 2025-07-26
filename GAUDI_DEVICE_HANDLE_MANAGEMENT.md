# Gaudi Device Handle Management in UCX

## The Challenge: Exclusive Device Handles

The Habana Labs `hlthunk` library presents a unique challenge for device management within UCX. A call to `hlthunk_open()` for a specific Gaudi device will fail if that same device has already been opened within the same process. This behavior is unlike other accelerator drivers (e.g., CUDA, ROCm) which typically allow multiple non-exclusive opens.

This creates two critical problems:

1.  **Internal Conflict:** Multiple components within UCX need to interact with the same device. For example, the `gaudi_copy` transport (for memory operations) and the `gaudi_ipc` transport (for peer-to-peer communication) both require a handle to the same device. If they both try to call `hlthunk_open()`, the second call will fail, breaking UCX's initialization.

2.  **External Conflict:** An application using UCX (e.g., PyTorch) may open a Gaudi device for its own computational purposes *before* UCX is initialized. When UCX then tries to open the same device, the call will fail, making it impossible for the application and UCX to coexist.

## The Solution: A Centralized Device Manager

To solve these problems, UCX implements a centralized, thread-safe device manager for the Gaudi transports. This manager ensures that `hlthunk_open()` is only called once per device and provides a mechanism for the application to pass in a pre-existing device handle.

The manager is implemented in `src/uct/gaudi/base/uct_gaudi_device_manager.c`.

### Public API

The manager exposes two functions in `src/uct/gaudi/base/uct_gaudi_device_manager.h`:

-   `ucs_status_t uct_gaudi_device_get_handle(int device_index, int *fd_p);`
-   `ucs_status_t uct_gaudi_device_put_handle(int device_index);`

All UCX components that need to interact with a Gaudi device **must** use these functions instead of calling `hlthunk_open()` or `hlthunk_close()` directly.

### Internal Sharing: Reference Counting

The manager uses an internal reference counter to manage the lifetime of a device handle within UCX.

-   When `uct_gaudi_device_get_handle()` is called for the first time for a given device, the manager opens the device and sets its reference count to 1.
-   Subsequent calls for the same device simply increment the reference count and return the cached handle.
-   `uct_gaudi_device_put_handle()` decrements the reference count. When the count reaches zero, the manager calls `hlthunk_close()` on the device handle.

This mechanism solves the internal conflict between different UCX components.

### External Sharing: Importing an Application's Handle

To resolve the conflict with the application, the manager can import a handle that was opened by the application.

#### 1. Environment Variable

The application must pass its file descriptor to UCX via an environment variable. The variable name is constructed based on the device index:

```bash
# Example for passing a handle for Gaudi device 0
export UCX_GAUDI_DEVICE_FD_0=<file_descriptor>

# Example for passing a handle for Gaudi device 1
export UCX_GAUDI_DEVICE_FD_1=<file_descriptor>
```

#### 2. Using `dup()` for Resource Safety

When `uct_gaudi_device_get_handle()` is called and the corresponding environment variable is set:
1.  The manager reads the file descriptor from the environment variable.
2.  It uses the `dup()` system call to create a **new, duplicate file descriptor**.
3.  This new handle is stored in the manager's internal cache.

Using `dup()` is critical for resource safety. When UCX is finalized, the manager will call `close()` on its duplicated handle. This does **not** affect the application's original handle, which remains open and valid. This prevents UCX from accidentally closing a resource that the application is still using.

### Complete Workflow Example

1.  An application opens Gaudi device 0 and receives file descriptor `5`.
2.  The application sets `export UCX_GAUDI_DEVICE_FD_0=5`.
3.  The application initializes UCX.
4.  A component within UCX calls `uct_gaudi_device_get_handle(0, &fd)`.
5.  The manager sees the environment variable, calls `dup(5)`, and gets a new handle, `10`. It caches handle `10` and sets the reference count to 1.
6.  Another UCX component calls `uct_gaudi_device_get_handle(0, &fd)`. The manager sees the reference count is > 0, increments it to 2, and returns the cached handle `10`.
7.  When UCX finalizes, both components call `uct_gaudi_device_put_handle(0)`. The reference count is decremented twice. When it reaches 0, the manager calls `close(10)`.
8.  The application's original handle, `5`, remains unaffected and can continue to be used.

This design ensures that UCX can function correctly both as the primary owner of the device and as a library within a larger application that manages the device's lifetime.
