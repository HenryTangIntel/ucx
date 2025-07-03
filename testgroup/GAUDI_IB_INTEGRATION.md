# UCX IB Transport Updates for Gaudi Accelerator Support

## Overview

This document summarizes the updates needed in the UCX InfiniBand (IB) transport layer to support Gaudi devices as accelerators. The integration enables zero-copy RDMA operations between Gaudi device memory and remote endpoints over InfiniBand networks.

## Key Integration Points

### 1. Memory Type Compatibility

**File: `/workspace/ucx/src/uct/ib/base/ib_md.c`**

- **Access Memory Types**: IB MD now advertises it can access Gaudi memory
- **DMA-BUF Support**: IB MD declares DMA-BUF capability for Gaudi memory types
- **Driver Detection**: Enhanced detection for Gaudi hardware and drivers

### 2. Changes Made

#### A. Memory Type Advertisement (Lines 237-240)
```c
// BEFORE:
md_attr->access_mem_types = UCS_BIT(UCS_MEMORY_TYPE_HOST);

// AFTER:  
md_attr->access_mem_types = UCS_BIT(UCS_MEMORY_TYPE_HOST) | 
                           (md->reg_mem_types & UCS_BIT(UCS_MEMORY_TYPE_GAUDI));
```

#### B. DMA-BUF Memory Types Support (Lines 242-244)
```c
// ADDED:
md_attr->dmabuf_mem_types = (md->cap_flags & UCT_MD_FLAG_REG_DMABUF) ? 
                           (md->reg_mem_types & UCS_BIT(UCS_MEMORY_TYPE_GAUDI)) : 0;
```

#### C. Enhanced Gaudi Driver Detection (Lines 1299-1302)
```c
// ENHANCED:
/* Check for Gaudi GPU-direct support */
uct_ib_check_gpudirect_driver(md, "/sys/module/habanalabs/version", UCS_MEMORY_TYPE_GAUDI);
/* Also check for Gaudi device nodes */
uct_ib_check_gpudirect_driver(md, "/dev/accel/accel0", UCS_MEMORY_TYPE_GAUDI);
uct_ib_check_gpudirect_driver(md, "/dev/hl0", UCS_MEMORY_TYPE_GAUDI);
```

## Integration Workflow

### Cross-Registration Process

1. **Gaudi Memory Allocation**:
   ```c
   // Allocate on Gaudi MD with DMA-BUF export
   uct_mem_alloc(gaudi_md, size, UCT_MD_MEM_FLAG_FIXED, &gaudi_mem);
   // -> Internally exports as DMA-BUF via uct_gaudi_export_dmabuf()
   ```

2. **DMA-BUF Extraction**:
   ```c
   // Extract DMA-BUF fd from Gaudi memory handle
   int dmabuf_fd = gaudi_memh->dmabuf_fd;
   ```

3. **IB Registration**:
   ```c
   // Register DMA-BUF with IB MD
   uct_md_mem_reg_params_t params = {
       .field_mask = UCT_MD_MEM_REG_FIELD_DMABUF_FD,
       .dmabuf_fd = dmabuf_fd
   };
   uct_md_mem_reg(ib_md, address, size, &params, &ib_memh);
   // -> Internally calls ibv_reg_dmabuf_mr()
   ```

4. **RDMA Operations**:
   ```c
   // Perform RDMA operations over IB using Gaudi memory
   uct_ep_put_short(ib_ep, buffer, length, remote_addr, rkey);
   ```

## Infrastructure Already Present

### DMA-BUF Support in IB
The IB transport already includes comprehensive DMA-BUF support:

- **Registration Function**: `ibv_reg_dmabuf_mr()` in `uct_ib_reg_mr()`
- **Detection Logic**: `uct_ib_md_check_dmabuf()` checks HCA DMA-BUF capability
- **Configuration**: `UCX_IB_GPU_DIRECT_RDMA` environment variable

### GPUDirect RDMA Detection
Existing detection infrastructure checks for:
- NVIDIA GPUDirect drivers (`/sys/module/nv_peer_mem/version`)
- AMD ROCm drivers (`/dev/kfd`)
- Gaudi drivers (now enhanced with multiple device paths)

## Configuration Requirements

### Environment Variables
```bash
# Enable GPUDirect RDMA
export UCX_IB_GPU_DIRECT_RDMA=yes

# Optional: Force Gaudi memory type
export UCX_MEMTYPE_CACHE=no
```

### Hardware Requirements
- Gaudi device with habanalabs driver
- InfiniBand HCA with DMA-BUF support (modern Mellanox cards)
- Kernel with DMA-BUF infrastructure

### Driver Dependencies
- `habanalabs` module loaded
- `ib_core` and HCA-specific drivers (e.g., `mlx5_ib`)
- DMA-BUF support in kernel

## Memory Type Flow

```
Application Request
       ↓
Gaudi MD (allocate device memory)
       ↓ (export DMA-BUF)
DMA-BUF fd
       ↓ (cross-register)
IB MD (register DMA-BUF)
       ↓ (RDMA operations)
IB Transport (zero-copy RDMA)
```

## Benefits

1. **Zero-Copy Communication**: Direct RDMA between Gaudi memory and network
2. **High Performance**: Eliminates CPU copying between device and host
3. **Compatibility**: Works with existing IB infrastructure and tools
4. **Flexibility**: Supports both peer-to-peer and host-mediated transfers

## Testing

A integration test has been created at `/workspace/ucx/testgroup/test_gaudi_ib_integration.c` that demonstrates:
- Detection of both Gaudi and IB components
- Memory type compatibility verification
- Cross-registration workflow explanation
- Configuration requirements

## Validation

The integration can be validated by:

1. **Component Detection**:
   ```bash
   ucx_info -d | grep -E "(gaudi|ib|mlx)"
   ```

2. **Memory Type Support**:
   ```bash
   ucx_info -d -c gaudi | grep -i "memory types"
   ucx_info -d -c ib | grep -i "memory types"
   ```

3. **DMA-BUF Capability**:
   ```bash
   ucx_info -d -c ib | grep -i dmabuf
   ```

## Summary

The IB transport layer has been successfully updated to support Gaudi as an accelerator through:

- **Memory Type Integration**: IB MD advertises Gaudi memory access capability
- **DMA-BUF Infrastructure**: Leverages existing DMA-BUF registration for cross-device sharing
- **Driver Detection**: Enhanced detection for multiple Gaudi device paths
- **Zero-Copy Operations**: Enables direct RDMA on Gaudi device memory

This integration provides a complete path for high-performance communication between Gaudi accelerators and remote endpoints over InfiniBand networks.
