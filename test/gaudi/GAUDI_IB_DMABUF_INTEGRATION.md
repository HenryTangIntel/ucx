# Gaudi-InfiniBand DMA-BUF Integration Guide

This document explains how to connect Habana Gaudi accelerators with InfiniBand using DMA-BUF for zero-copy data transfers in UCX.

## Overview

The Gaudi UCX transport now supports DMA-BUF export/import functionality that enables zero-copy data sharing between Gaudi device memory and InfiniBand HCAs. This allows for efficient GPU-Direct-like transfers without CPU involvement.

## Architecture

```
┌─────────────┐    DMA-BUF     ┌─────────────┐    InfiniBand    ┌─────────────┐
│   Gaudi     │◄──────────────►│     IB      │◄────────────────►│   Remote    │
│   Device    │   (zero-copy)  │     HCA     │   (RDMA/Send)    │   Node      │
└─────────────┘                └─────────────┘                  └─────────────┘
```

## Implementation Details

### 1. DMA-BUF Export from Gaudi

The Gaudi transport exports device memory as DMA-BUF file descriptors using the hlthunk API:

```c
// For allocated device memory
int dmabuf_fd = hlthunk_device_memory_export_dmabuf_fd(
    gaudi_fd,           // Gaudi device file descriptor
    device_addr,        // Device memory address (uint64_t)
    memory_size,        // Memory size in bytes
    0                   // Flags
);

// For mapped host memory
int dmabuf_fd = hlthunk_device_mapped_memory_export_dmabuf_fd(
    gaudi_fd,           // Gaudi device file descriptor
    host_addr,          // Host memory address (uint64_t)
    memory_size,        // Memory size in bytes
    0,                  // Offset within memory
    0                   // Flags
);
```

### 2. Memory Registration Flow

When memory is allocated or registered in UCX:

1. **Gaudi Memory Allocation**: UCX calls `uct_gaudi_copy_mem_alloc()`
2. **DMA-BUF Export**: Gaudi memory is exported as DMA-BUF fd
3. **Memory Handle Creation**: UCX creates `uct_gaudi_mem_t` with DMA-BUF info
4. **Memory Key Packing**: DMA-BUF fd is included in packed memory keys

### 3. InfiniBand Integration

The DMA-BUF file descriptor is passed to InfiniBand transport through UCX memory keys:

```c
typedef struct {
    uint64_t vaddr;      // Virtual address
    uint64_t length;     // Memory length
    int      dmabuf_fd;  // DMA-BUF file descriptor
} uct_gaudi_key_t;
```

### 4. UCX Memory Key Flow

```
┌─────────────┐    mem_alloc()     ┌─────────────┐    mkey_pack()     ┌─────────────┐
│    User     │───────────────────►│   Gaudi     │───────────────────►│ Memory Key  │
│ Application │                    │ Transport   │                    │ (w/ dmabuf) │
└─────────────┘                    └─────────────┘                    └─────────────┘
                                          │
                                          ▼
                                   ┌─────────────┐
                                   │  DMA-BUF    │
                                   │  Export     │
                                   └─────────────┘
```

## Usage Example

### Basic Memory Allocation and Registration

```c
#include <uct/api/uct.h>

// Allocate Gaudi device memory with DMA-BUF support
uct_mem_h gaudi_memh;
void *gaudi_ptr;
size_t size = 1024 * 1024; // 1MB

ucs_status_t status = uct_md_mem_alloc(gaudi_md, &size, &gaudi_ptr, 
                                      UCS_MEMORY_TYPE_GAUDI, 
                                      UCS_SYS_DEVICE_ID_UNKNOWN,
                                      UCT_MD_MEM_FLAG_FIXED, // Request DMA-BUF export
                                      "gaudi_buffer", &gaudi_memh);

// Pack memory key for remote access (includes DMA-BUF fd)
void *packed_key;
uct_md_mkey_pack_params_t pack_params = {0};
status = uct_md_mkey_pack(gaudi_md, gaudi_memh, gaudi_ptr, size, 
                         &pack_params, packed_key);

// The packed key now contains DMA-BUF fd for IB integration
```

### InfiniBand Integration

```c
// On InfiniBand side, import DMA-BUF and register for RDMA
uct_gaudi_key_t *gaudi_key = (uct_gaudi_key_t *)packed_key;

if (gaudi_key->dmabuf_fd >= 0) {
    // Import DMA-BUF on IB device
    void *ib_mapped_ptr = ib_import_dmabuf(ib_device, gaudi_key->dmabuf_fd);
    
    // Register imported memory with IB
    uct_mem_h ib_memh;
    status = uct_md_mem_reg(ib_md, ib_mapped_ptr, gaudi_key->length, 
                           UCT_MD_MEM_FLAG_FIXED, &ib_memh);
    
    // Now IB can directly access Gaudi memory for zero-copy transfers
    uct_ep_put(ib_ep, data, size, ib_memh, gaudi_key->vaddr, gaudi_rkey);
}
```

## Memory Management Functions

The Gaudi transport implements complete memory management:

- `uct_gaudi_copy_mem_alloc()` - Allocate device memory with DMA-BUF export
- `uct_gaudi_copy_mem_free()` - Free device memory and close DMA-BUF fd
- `uct_gaudi_copy_mem_reg()` - Register host/device memory with DMA-BUF export
- `uct_gaudi_copy_mem_dereg()` - Deregister memory and clean up DMA-BUF
- `uct_gaudi_copy_mkey_pack()` - Pack memory keys with DMA-BUF info
- `uct_gaudi_copy_mem_attach()` - Attach to remote memory using DMA-BUF

## Helper Functions

DMA-BUF integration helper functions:

```c
// Check if memory handle has DMA-BUF for IB sharing
static int uct_gaudi_copy_has_dmabuf_for_ib(uct_mem_h memh);

// Get DMA-BUF file descriptor for IB integration  
static int uct_gaudi_copy_get_dmabuf_fd(uct_mem_h memh);
```

## Configuration

DMA-BUF support can be controlled via UCX configuration:

```bash
# Enable DMA-BUF support (default: try)
export UCX_GAUDI_COPY_DMABUF=yes

# Check if DMA-BUF is enabled
ucx_info -c | grep GAUDI_COPY_DMABUF
```

## Testing DMA-BUF Integration

### Verify Gaudi Transport Detection

```bash
# Check if Gaudi transport is available
ucx_info -d | grep gaudi

# Expected output:
# Memory domain: gaudi_copy
#     Component: gaudi_copy  
#      Transport: gaudi_cpy
#         Device: gaudi_copy
```

### Test Memory Allocation with DMA-BUF

```bash
# Run UCX perftest with Gaudi memory
ucx_perftest -t ucp_put -m gaudi -s 1048576

# Use environment variables to force DMA-BUF usage
UCX_GAUDI_COPY_DMABUF=yes ucx_perftest -t ucp_put -m gaudi -s 1048576
```

## Performance Benefits

DMA-BUF integration provides several performance advantages:

1. **Zero-Copy Transfers**: Direct memory access between Gaudi and IB without CPU involvement
2. **Reduced Latency**: Eliminates memory copies through host buffers
3. **Higher Bandwidth**: Direct device-to-device data paths
4. **Lower CPU Utilization**: Offloads data movement from CPU

## Supported Operations

With DMA-BUF integration, the following UCX operations support zero-copy:

- `ucp_put()` - Put data from Gaudi to remote memory via IB
- `ucp_get()` - Get data from remote memory to Gaudi via IB  
- `ucp_send()` - Send Gaudi memory contents via IB
- `ucp_recv()` - Receive data directly into Gaudi memory via IB

## Requirements

- Habana Gaudi device with hl-thunk library
- InfiniBand HCA with DMA-BUF support
- Linux kernel with DMA-BUF subsystem enabled
- UCX compiled with Gaudi transport support

## Troubleshooting

### DMA-BUF Export Fails

```bash
# Check if hlthunk supports DMA-BUF export
grep dmabuf /usr/include/habanalabs/hlthunk.h

# Verify device supports DMA-BUF
dmesg | grep dmabuf
```

### InfiniBand DMA-BUF Import Issues

```bash
# Check IB device capabilities
ibv_devinfo -v | grep dmabuf

# Verify kernel DMA-BUF support
ls /sys/kernel/debug/dma_buf/
```

### Performance Issues

```bash
# Enable debug logging
export UCX_LOG_LEVEL=debug
export UCX_GAUDI_COPY_LOG_LEVEL=debug

# Check memory registration overhead
ucx_perftest -t ucp_put -m gaudi -w 100 -n 1000
```

## Future Enhancements

Planned improvements for Gaudi-IB DMA-BUF integration:

1. **P2P Direct Transfers**: Direct Gaudi-to-Gaudi transfers over IB
2. **Multi-Device Support**: Support for multiple Gaudi devices
3. **Memory Pool Integration**: Efficient memory pool with DMA-BUF caching
4. **NVIDIA GPUDirect Integration**: Interoperability with CUDA devices

## Contact

For issues or questions about Gaudi-IB DMA-BUF integration:

- UCX GitHub Issues: https://github.com/openucx/ucx/issues
- UCX Mailing List: ucx-group@googlegroups.com
- Habana Developer Support: https://developer.habana.ai
