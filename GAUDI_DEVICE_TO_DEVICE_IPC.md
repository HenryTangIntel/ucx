# Gaudi Device-to-Device IPC Implementation Guide

## Overview

Based on analysis of the hl-thunk-open repository, device-to-device IPC in Gaudi is implemented using **DMA-BUF (DMA Buffer Sharing)** mechanism, which is the standard Linux approach for sharing memory buffers between devices and subsystems.

## Core IPC Mechanism: DMA-BUF Based Sharing

### How Gaudi Device-to-Device IPC Works

Unlike traditional GPU IPC handles (like CUDA IPC or ROCm IPC), Gaudi uses **DMA-BUF file descriptors** for inter-process and inter-device communication:

```
Device A (Exporter)     Device B (Importer)
     │                       │
     ├─ Allocate Memory      │
     ├─ Export DMA-BUF FD ──────┐
     │                       │   │
     │                       ├─ Register DMA-BUF FD
     │                       ├─ Get Device VA
     ├─ DMA Operations       ├─ DMA Operations
     │  (source)             │  (destination)
```

### Key APIs from hlthunk

#### 1. Export Memory as DMA-BUF (Exporter Side)
```c
// Legacy API (all Gaudi versions)
int hlthunk_device_memory_export_dmabuf_fd(int fd, uint64_t addr, 
                                          uint64_t size, uint32_t flags);

// Enhanced API (Gaudi2+) - with offset support
int hlthunk_device_mapped_memory_export_dmabuf_fd(int fd, uint64_t addr,
                                                 uint64_t size, uint64_t offset,
                                                 uint32_t flags);
```

#### 2. Register DMA-BUF (Importer Side)
```c
// Direct ioctl call to register external DMA-BUF
// Returns device virtual address for the imported memory
union hl_mem_args args;
args.in.reg_dmabuf_fd.fd = dmabuf_fd;
args.in.reg_dmabuf_fd.length = size;
args.in.op = HL_MEM_OP_REG_DMABUF_FD;
ioctl(device_fd, DRM_IOCTL_HL_MEMORY, &args);
uint64_t device_va = args.out.device_virt_addr;
```

#### 3. Unregister DMA-BUF
```c
union hl_mem_args args;
args.in.unmap.device_virt_addr = device_va;
args.in.op = HL_MEM_OP_UNMAP;
ioctl(device_fd, DRM_IOCTL_HL_MEMORY, &args);
```

## UCX Implementation Strategy

### Current UCX Gaudi IPC vs hlthunk Reality

**UCX Current Approach (Custom Channels):**
- UCX Gaudi IPC uses custom channel-based communication
- Implemented in `src/uct/gaudi/ipc/` with custom channel management
- Innovative but not leveraging hlthunk's standard DMA-BUF mechanism

**hlthunk Reality (DMA-BUF Based):**
- Standard Linux DMA-BUF sharing mechanism
- Cross-device memory access via file descriptors
- Supported across all Gaudi generations with enhancements in Gaudi2+

### Recommended UCX Integration

#### 1. Enhance Existing IPC Implementation

Update the UCX Gaudi IPC to use DMA-BUF as the underlying mechanism while maintaining the channel abstraction:

```c
// Enhanced UCX Gaudi IPC Handle
typedef struct uct_gaudi_ipc_md_handle {
    uint64_t handle;            /* Original handle */
    uint32_t channel_id;        /* Channel ID for abstraction */
    uint32_t src_device_id;     /* Source device */
    uint32_t dst_device_id;     /* Destination device */
    int dmabuf_fd;              /* NEW: DMA-BUF file descriptor */
    uint64_t dmabuf_size;       /* NEW: Size of shared region */
    uint64_t dmabuf_offset;     /* NEW: Offset (Gaudi2+) */
} uct_gaudi_ipc_md_handle_t;
```

#### 2. DMA-BUF Integration Functions

```c
// Export memory for IPC sharing
static int uct_gaudi_ipc_export_memory(uct_gaudi_ipc_md_t *md,
                                      void *address, size_t length,
                                      uct_gaudi_ipc_md_handle_t *handle)
{
    int dmabuf_fd;
    
    // Try enhanced API first (Gaudi2+)
    if (md->mapped_dmabuf_supported) {
        dmabuf_fd = hlthunk_device_mapped_memory_export_dmabuf_fd(
            md->device_fds[src_device], (uint64_t)address, length, 0, 0);
    } else {
        dmabuf_fd = hlthunk_device_memory_export_dmabuf_fd(
            md->device_fds[src_device], (uint64_t)address, length, 0);
    }
    
    if (dmabuf_fd < 0) {
        return UCS_ERR_IO_ERROR;
    }
    
    handle->dmabuf_fd = dmabuf_fd;
    handle->dmabuf_size = length;
    handle->dmabuf_offset = 0;
    
    return UCS_OK;
}

// Import memory from another device
static ucs_status_t uct_gaudi_ipc_import_memory(uct_gaudi_ipc_md_t *md,
                                               int dst_device,
                                               uct_gaudi_ipc_md_handle_t *handle,
                                               uint64_t *device_va)
{
    union hl_mem_args ioctl_args;
    int rc;
    
    memset(&ioctl_args, 0, sizeof(ioctl_args));
    ioctl_args.in.reg_dmabuf_fd.fd = handle->dmabuf_fd;
    ioctl_args.in.reg_dmabuf_fd.length = handle->dmabuf_size;
    ioctl_args.in.op = HL_MEM_OP_REG_DMABUF_FD;
    
    rc = ioctl(md->device_fds[dst_device], DRM_IOCTL_HL_MEMORY, &ioctl_args);
    if (rc) {
        return uct_gaudi_translate_error(rc);
    }
    
    *device_va = ioctl_args.out.device_virt_addr;
    return UCS_OK;
}

// Direct device-to-device copy using imported memory
static ucs_status_t uct_gaudi_ipc_device_copy(uct_gaudi_ipc_md_t *md,
                                             uint32_t channel_id,
                                             void *dst, void *src, size_t length)
{
    // Use the imported device VA for direct DMA operations
    // This replaces the custom channel copy with standard DMA
    return uct_gaudi_dma_copy_async(md, dst, src, length, NULL);
}
```

### 3. Enhanced Channel Management with DMA-BUF

```c
// Create IPC channel with DMA-BUF backing
ucs_status_t uct_gaudi_ipc_channel_create_dmabuf(uct_gaudi_ipc_md_t *md,
                                                uint32_t src_device, 
                                                uint32_t dst_device,
                                                void *memory, size_t size,
                                                uint32_t *channel_id)
{
    uct_gaudi_ipc_channel_t *channel;
    uct_gaudi_ipc_md_handle_t handle;
    ucs_status_t status;
    
    // Export memory from source device
    status = uct_gaudi_ipc_export_memory(md, memory, size, &handle);
    if (status != UCS_OK) {
        return status;
    }
    
    // Import memory to destination device
    status = uct_gaudi_ipc_import_memory(md, dst_device, &handle, 
                                       &channel->dst_device_va);
    if (status != UCS_OK) {
        close(handle.dmabuf_fd);
        return status;
    }
    
    // Store channel information
    channel->handle = handle;
    channel->src_device = src_device;
    channel->dst_device = dst_device;
    channel->size = size;
    
    *channel_id = assign_channel_id(md, channel);
    return UCS_OK;
}
```

## Practical Implementation Example

### Complete Device-to-Device Transfer

```c
// Example: Transfer data between two Gaudi devices
void example_device_to_device_transfer()
{
    // Device A: Allocate and export memory
    int fd_a = hlthunk_open(0, NULL);  // Device A
    int fd_b = hlthunk_open(1, NULL);  // Device B
    
    size_t size = 1024 * 1024;  // 1MB
    
    // 1. Allocate memory on Device A
    uint64_t device_a_ptr = hlthunk_device_memory_alloc(fd_a, size, 0, true, true);
    uint64_t host_a_ptr = hlthunk_device_memory_map(fd_a, device_a_ptr, 0);
    
    // 2. Export DMA-BUF from Device A
    int dmabuf_fd = hlthunk_device_memory_export_dmabuf_fd(fd_a, device_a_ptr, size, 0);
    
    // 3. Import DMA-BUF to Device B
    union hl_mem_args args;
    args.in.reg_dmabuf_fd.fd = dmabuf_fd;
    args.in.reg_dmabuf_fd.length = size;
    args.in.op = HL_MEM_OP_REG_DMABUF_FD;
    ioctl(fd_b, DRM_IOCTL_HL_MEMORY, &args);
    uint64_t device_b_ptr = args.out.device_virt_addr;
    
    // 4. Now both devices can access the same physical memory
    // Device A can write, Device B can read (or vice versa)
    
    // 5. Perform DMA operations using the shared memory
    // (Implementation depends on specific DMA engine usage)
    
    // 6. Cleanup
    args.in.unmap.device_virt_addr = device_b_ptr;
    args.in.op = HL_MEM_OP_UNMAP;
    ioctl(fd_b, DRM_IOCTL_HL_MEMORY, &args);
    
    close(dmabuf_fd);
    hlthunk_device_memory_free(fd_a, device_a_ptr);
    hlthunk_close(fd_a);
    hlthunk_close(fd_b);
}
```

## Configuration and Optimization

### UCX Configuration for DMA-BUF IPC

```bash
# Enable DMA-BUF based IPC
export UCT_GAUDI_IPC_ENABLE_DMABUF=yes

# Use enhanced DMA-BUF API when available (Gaudi2+)
export UCT_GAUDI_IPC_ENABLE_MAPPED_DMABUF=yes

# Optimize for multi-device scenarios
export UCT_GAUDI_IPC_DMABUF_CACHE_SIZE=64
export UCT_GAUDI_IPC_MAX_DEVICES=8
```

### Performance Considerations

1. **DMA-BUF File Descriptor Management**: Cache and reuse FDs when possible
2. **Memory Alignment**: Ensure proper alignment for DMA operations
3. **Multi-Device Topology**: Consider NUMA and PCIe topology for optimal placement
4. **Error Handling**: Robust cleanup of DMA-BUF resources

## Advantages of DMA-BUF Approach

1. **Standard Linux Mechanism**: Leverages kernel DMA-BUF subsystem
2. **Cross-Subsystem Compatibility**: Works with InfiniBand, networking, storage
3. **Security**: Kernel-mediated access control
4. **Scalability**: Supports multiple devices and complex topologies
5. **Future-Proof**: Evolves with Linux DMA-BUF improvements

## Integration with Existing UCX Gaudi

### Migration Strategy

1. **Phase 1**: Add DMA-BUF support alongside existing channel mechanism
2. **Phase 2**: Migrate channel implementation to use DMA-BUF internally
3. **Phase 3**: Optimize performance and add advanced features
4. **Phase 4**: Deprecate custom channel approach in favor of standard DMA-BUF

### Backward Compatibility

- Maintain existing UCX Gaudi IPC API
- Add DMA-BUF backend as implementation detail
- Provide configuration options for both approaches during transition

## Conclusion

The hlthunk library provides a robust DMA-BUF based mechanism for device-to-device IPC that should be leveraged in UCX Gaudi implementation. This approach:

- Aligns with Linux kernel standards
- Provides better compatibility with other subsystems
- Offers enhanced performance for cross-device communication
- Supports advanced features like offset-based sharing (Gaudi2+)

The UCX implementation should integrate this mechanism while maintaining the existing channel abstraction for API compatibility.