# Summary of the `hl-thunk` Library Public API

This document provides a concise list of the public functions that the `hl-thunk` library would expose for consumption by a high-performance library like UCX. The API is categorized by functionality.

---

### 1. Device Management

These functions are for discovering and managing the connection to the Gaudi hardware.

- **`hlthunk_open()`**
  - **Purpose:** Opens a specific Gaudi device and returns a file descriptor handle for all subsequent operations.

- **`hlthunk_close()`**
  - **Purpose:** Closes the device file descriptor, releasing the connection to the hardware.

- **`hlthunk_get_hw_ip_info()`**
  - **Purpose:** Queries the device for its static hardware properties, such as memory size, device ID, and other capabilities.

---

### 2. Memory Management

These functions handle the allocation and mapping of memory on the Gaudi device.

- **`hlthunk_device_memory_alloc()`**
  - **Purpose:** Allocates a block of memory on the Gaudi device's High Bandwidth Memory (HBM).

- **`hlthunk_device_memory_free()`**
  - **Purpose:** Frees a previously allocated block of memory on the device.

- **`hlthunk_device_memory_map()`**
  - **Purpose:** Maps a device memory handle into the application's virtual address space, returning a pointer that can be used in DMA commands.

- **`hlthunk_memory_unmap()`**
  - **Purpose:** Unmaps a previously mapped device or host memory region from the device's MMU.

---

### 3. Asynchronous Data Movement (DMA)

These functions provide non-blocking control over the on-chip DMA engine.

- **`hlthunk_dma_host_to_device_async()`**
  - **Purpose:** Initiates a non-blocking copy from host RAM to device HBM. Returns an opaque request handle.

- **`hlthunk_dma_device_to_host_async()`**
  - **Purpose:** Initiates a non-blocking copy from device HBM to host RAM. Returns an opaque request handle.

- **`hlthunk_dma_device_to_device_async()`**
  - **Purpose:** Initiates a non-blocking copy between two memory locations within the same device. Returns an opaque request handle.

- **`hlthunk_dma_poll()`**
  - **Purpose:** Polls the status of an asynchronous DMA operation using its request handle to check for completion.

- **`hlthunk_dma_request_free()`**
  - **Purpose:** Frees the resources associated with a completed DMA request handle.

---

### 4. Interoperability and Peer-to-Peer (IPC)

These functions enable communication with other drivers and other Gaudi devices.

- **`hlthunk_device_memory_export_dmabuf_fd()`**
  - **Purpose:** Exports a handle to Gaudi memory as a standard Linux `dmabuf` file descriptor, allowing other device drivers (e.g., a network card) to access it directly.

- **`hlthunk_ipc_wrap_handle()` / `hlthunk_ipc_open_handle()`**
  - **Purpose:** Creates and opens shareable handles to enable direct, peer-to-peer memory access between two Gaudi devices on the same node.
