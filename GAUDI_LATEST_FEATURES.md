# Gaudi UCX Implementation - Latest hl-thunk Features Integration

This document describes the integration of the latest hl-thunk-open repository features into the UCX Gaudi implementation.

## Overview of New Features

Based on analysis of the hl-thunk-open repository, several advanced features have been identified and integrated:

### 1. Enhanced DMA-BUF Support with Offset (`hlthunk_device_mapped_memory_export_dmabuf_fd`)

**Feature Description:**
- Available in Gaudi2+ devices
- Provides DMA-BUF export with offset capability for mapped device memory
- Enables more flexible memory sharing with InfiniBand and other subsystems

**UCX Integration:**
- Added `mapped_dmabuf_supported` capability detection
- Enhanced memory allocation to use new API when available
- Fallback to legacy DMA-BUF API for compatibility
- Configuration parameter: `UCT_GAUDI_COPY_ENABLE_MAPPED_DMABUF`

**Implementation Details:**
```c
// Enhanced DMA-BUF export with offset support
int dmabuf_fd = hlthunk_device_mapped_memory_export_dmabuf_fd(
    fd, device_addr, size, offset, flags);
```

### 2. Hardware Block Access (`hlthunk_get_hw_block`)

**Feature Description:**
- Direct access to hardware configuration blocks
- Enables low-level hardware optimization and debugging
- Provides memory-mapped access to specific hardware components

**UCX Integration:**
- Added capability detection for hardware block access
- Configuration parameter: `UCT_GAUDI_COPY_ENABLE_HW_BLOCK_ACCESS`
- Foundation for advanced performance optimizations

### 3. Network Interface Scale-Out Support

**Feature Description:**
- Detection and utilization of available NIC ports
- Support for scale-out communication across multiple Gaudi devices
- Integration with external network interfaces

**UCX Integration:**
- Added `nic_ports_available` detection from `hw_info.nic_ports_mask`
- Configuration parameter: `UCT_GAUDI_COPY_ENABLE_NIC_SCALE_OUT`
- Foundation for multi-device communication patterns

### 4. Advanced Device Capabilities Detection

**Feature Description:**
- Runtime detection of device-specific features
- Version-aware API usage
- Graceful fallback for older hardware/drivers

**UCX Integration:**
- Enhanced hardware capability probing during MD initialization
- Device ID-based feature detection (Gaudi2, Gaudi3, etc.)
- Automatic API selection based on available features

## Implementation Architecture

### Memory Handle Enhancements

```c
typedef struct uct_gaudi_mem {
    void *vaddr;                /* Virtual address */
    size_t size;                /* Size of the memory region */
    uint64_t handle;            /* Device memory handle */
    uint64_t dev_addr;          /* Device address */
    int dmabuf_fd;              /* DMA-BUF file descriptor */
    uint64_t dmabuf_offset;     /* NEW: Offset within DMA-BUF */
    uint8_t is_mapped_memory;   /* NEW: Memory type indicator */
    ucs_list_link_t list;       /* List linkage for tracking */
} uct_gaudi_mem_t;
```

### Configuration Structure

```c
struct {
    int dmabuf_supported;           /* Legacy DMA-BUF support */
    int mapped_dmabuf_supported;    /* NEW: Enhanced DMA-BUF */
    int nic_ports_available;        /* NEW: Available NIC ports */
    ucs_ternary_auto_value_t enable_rcache;
    double max_reg_ratio;
    ucs_on_off_auto_value_t alloc_whole_reg;
    ucs_ternary_auto_value_t enable_hw_block_access;  /* NEW */
} config;
```

## Configuration Parameters

### New Configuration Options

| Parameter | Default | Description |
|-----------|---------|-------------|
| `UCT_GAUDI_COPY_ENABLE_MAPPED_DMABUF` | `try` | Enhanced DMA-BUF with offset (Gaudi2+) |
| `UCT_GAUDI_COPY_ENABLE_HW_BLOCK_ACCESS` | `try` | Direct hardware block access |
| `UCT_GAUDI_COPY_ENABLE_NIC_SCALE_OUT` | `try` | NIC-based scale-out communication |

### Enhanced Features from Previous Implementation

| Parameter | Default | Description |
|-----------|---------|-------------|
| `UCT_GAUDI_COPY_ENABLE_RCACHE` | `try` | Memory registration cache |
| `UCT_GAUDI_COPY_REG_COST` | `7000ns` | Registration cost estimation |
| `UCT_GAUDI_COPY_ENABLE_DMABUF` | `try` | Legacy DMA-BUF support |

## Usage Examples

### Enhanced DMA-BUF Usage

```bash
# Enable enhanced DMA-BUF with automatic fallback
export UCT_GAUDI_COPY_ENABLE_MAPPED_DMABUF=yes

# Combined with registration cache for optimal performance
export UCT_GAUDI_COPY_ENABLE_RCACHE=yes
export UCT_GAUDI_COPY_RCACHE_MAX_REGIONS=1000
```

### Scale-Out Configuration

```bash
# Enable NIC-based scale-out
export UCT_GAUDI_COPY_ENABLE_NIC_SCALE_OUT=yes

# Verify NIC port detection
ucx_info -d | grep "nic_ports"
```

### Hardware Block Access

```bash
# Enable advanced hardware access
export UCT_GAUDI_COPY_ENABLE_HW_BLOCK_ACCESS=yes

# Useful for performance tuning and debugging
export UCX_LOG_LEVEL=debug
```

## Compatibility and Device Support

### Device Support Matrix

| Feature | Gaudi | Gaudi2 | Gaudi3 | Notes |
|---------|-------|--------|--------|-------|
| Legacy DMA-BUF | ✓ | ✓ | ✓ | Always available |
| Enhanced DMA-BUF | ✗ | ✓ | ✓ | Offset support |
| HW Block Access | ✓ | ✓ | ✓ | Driver dependent |
| NIC Scale-Out | Limited | ✓ | ✓ | Based on NIC ports |
| Registration Cache | ✓ | ✓ | ✓ | Performance enhancement |

### Backward Compatibility

- All new features have automatic fallback mechanisms
- Existing applications continue to work without modification
- Configuration parameters use `try` defaults for graceful degradation
- Legacy API support maintained

## Performance Impact

### Expected Benefits

1. **Enhanced DMA-BUF**: 10-20% improvement in cross-subsystem data sharing
2. **Registration Cache**: 50-80% reduction in memory registration overhead
3. **NIC Scale-Out**: Enables multi-device communication patterns
4. **Hardware Block Access**: Foundation for advanced optimizations

### Benchmarking

```bash
# Performance comparison with new features
UCT_GAUDI_COPY_ENABLE_MAPPED_DMABUF=yes \
UCT_GAUDI_COPY_ENABLE_RCACHE=yes \
ucx_perftest -t gaudi_bw -s 1MB:1GB -n 1000

# Measure registration performance
UCT_GAUDI_COPY_ENABLE_RCACHE=yes \
./test_gaudi_memreg_performance
```

## Troubleshooting

### Common Issues

1. **Enhanced DMA-BUF Not Available**
   ```
   Warning: Enhanced DMA-BUF API not available, using legacy mode
   ```
   - **Cause**: Older driver or Gaudi1 device
   - **Solution**: Update driver or use legacy DMA-BUF

2. **NIC Ports Not Detected**
   ```
   Debug: Detected 0 NIC ports for scale-out
   ```
   - **Cause**: No external NIC ports or scale-out not supported
   - **Solution**: Check hardware configuration

3. **Registration Cache Issues**
   ```
   Warning: Failed to create registration cache
   ```
   - **Cause**: Insufficient memory or configuration issue
   - **Solution**: Adjust `UCT_GAUDI_COPY_RCACHE_*` parameters

### Debug Information

```bash
# Enable detailed logging
export UCX_LOG_LEVEL=debug
export UCX_LOG_FILE=gaudi_debug.log

# Check detected capabilities
ucx_info -d | grep -i gaudi
```

## Future Enhancements

Based on the hl-thunk-open analysis, future improvements could include:

1. **PDMA Channel Utilization**: Advanced DMA channel management
2. **ARC Integration**: Scheduler and engine ARC utilization
3. **Advanced NIC Features**: Collective operations and reduction support
4. **Multi-Die Support**: Gaudi3 multi-die architecture optimization

## References

- hl-thunk-open repository: Latest driver interface
- UCX Documentation: Core UCX concepts
- Habana Labs Documentation: Hardware specifications
- DMA-BUF Documentation: Linux kernel DMA-BUF subsystem

---

*This document reflects the integration of hl-thunk-open features into UCX Gaudi implementation as of 2025.*