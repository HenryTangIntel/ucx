# Gaudi UCX Integration

## Overview
The Gaudi UCX integration enables high-performance communication and memory operations between host and Gaudi accelerator devices using the Unified Communication X (UCX) framework. This integration provides a seamless interface for applications to utilize Gaudi's DMA engines and memory management capabilities within the UCX ecosystem, similar to existing CUDA and ROCm support.

## Features

- **Memory Domain (MD) Support:**
  - Gaudi is registered as a memory domain (`gaudi_copy`), allowing UCX to manage and query Gaudi device memory.
  - Supports memory allocation, registration, and deregistration for both host and Gaudi device memory.

- **Transport Layer (TL) Support:**
  - Implements the `gaudi_cpy` transport for efficient memory copy operations between host and Gaudi device, and device-to-device transfers.
  - Utilizes Gaudi's DMA engines for offloaded memory copy, reducing CPU overhead and improving throughput.

- **DMA-BUF Integration:**
  - Supports exporting and importing memory as DMA-BUF file descriptors, enabling zero-copy sharing with other subsystems (e.g., InfiniBand, RDMA).
  - Allows interoperability with other devices and transports that support DMA-BUF.

- **Device Discovery and Mapping:**
  - Uses the `GAUDI_MAPPING_TABLE` environment variable to map logical device indices to physical bus IDs, supporting multi-device systems.
  - Automatically detects available Gaudi devices and selects the appropriate device for operations.

- **Memory Type Detection:**
  - UCX can detect whether a pointer refers to host or Gaudi device memory, enabling correct selection of transports and memory operations.

- **Error Handling and Resource Management:**
  - Robust error handling for device discovery, memory allocation, and DMA operations.
  - Automatic cleanup of resources, including memory handles and DMA-BUF file descriptors.

## Functionalities

### 1. Memory Allocation and Registration
- Applications can allocate memory on Gaudi devices via UCX APIs.
- Host memory can be registered for use with Gaudi DMA engines.
- Both host and device memory can be exported as DMA-BUF for sharing.

### 2. High-Performance Memory Copy
- The `gaudi_cpy` transport enables fast memory copy between host and device, and device-to-device, using Gaudi's hardware DMA.
- Host-to-host copies are not supported by the Gaudi DMA engine and will fallback or return an error.

### 3. Device Selection and Mapping
- Device selection is managed via the `GAUDI_MAPPING_TABLE` environment variable, which provides a JSON array mapping logical indices to PCIe bus IDs.
- The integration queries this mapping to open the correct device for each operation.

### 4. DMA-BUF Export/Import
- Memory allocated or registered with Gaudi can be exported as a DMA-BUF file descriptor.
- This enables zero-copy data sharing with other UCX transports (e.g., InfiniBand) and external subsystems.

### 5. UCX API Compatibility
- The integration implements all required UCX memory domain and transport operations, including `mem_alloc`, `mem_free`, `mem_reg`, `mem_dereg`, `mem_query`, and `mkey_pack`.
- Compatible with UCX's device and transport selection mechanisms (e.g., `UCX_TLS`, `UCX_NET_DEVICES`).

## Usage

1. **Set up the environment:**
   - Install the Gaudi driver and `libhlthunk`.
   - Set the `GAUDI_MAPPING_TABLE` environment variable to map device indices to bus IDs.
   - Ensure `LD_LIBRARY_PATH` includes the directory with Gaudi libraries.

2. **Build UCX with Gaudi support:**
   - Install `libcjson-dev` and other dependencies.
   - Configure and build UCX as usual. The build system will detect and enable Gaudi support if dependencies are present.

3. **Run applications:**
   - Use UCX APIs as with other accelerators (CUDA, ROCm).
   - Select the `gaudi_copy` device and `gaudi_cpy` transport as needed.

## Example GAUDI_MAPPING_TABLE

```
export GAUDI_MAPPING_TABLE='[
  {"index":0,"module_id":2,"bus_id":"0000:33:00.0"},
  {"index":1,"module_id":0,"bus_id":"0000:4d:00.0"},
  {"index":2,"module_id":1,"bus_id":"0000:4e:00.0"},
  {"index":3,"module_id":3,"bus_id":"0000:34:00.0"}
]'
```

## Notes
- The Gaudi UCX integration is designed to be consistent with CUDA and ROCm support in UCX, following similar naming and registration conventions.
- Ensure that the Gaudi driver and libraries are compatible with your UCX version.
- For advanced features or troubleshooting, consult the UCX and Gaudi documentation.
