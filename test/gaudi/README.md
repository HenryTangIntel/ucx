# Gaudi-InfiniBand DMA-BUF Integration Guide

This directory contains test programs and documentation for integrating Habana Gaudi accelerators with InfiniBand using DMA-BUF for zero-copy data transfers.

## Overview

The Gaudi-InfiniBand integration enables:
- **Zero-copy transfers**: Direct RDMA access to Gaudi memory via DMA-BUF
- **High bandwidth**: Eliminate CPU copies between Gaudi and network
- **Low latency**: Direct memory sharing between accelerator and network adapter
- **Scalability**: Support for multi-node Gaudi clusters with IB interconnect

## Architecture

```
┌─────────────┐    DMA-BUF    ┌─────────────┐    RDMA     ┌─────────────┐
│   Gaudi     │◄─────────────►│ InfiniBand  │◄───────────►│ Remote Node │
│  Accelerator│               │   Adapter   │             │             │
└─────────────┘               └─────────────┘             └─────────────┘
       │                              │
       │         UCX Transport        │
       └──────────────────────────────┘
```

## Components

### 1. UCX Gaudi Transport (`gaudi_copy`)
- **Location**: `src/uct/gaudi/`
- **Purpose**: Provides UCX memory domain for Gaudi devices
- **Features**:
  - Device memory allocation/deallocation
  - Host memory registration with Gaudi
  - DMA-BUF export for IB sharing
  - Memory type detection and management

### 2. DMA-BUF Integration
- **Export**: Gaudi memory exported as DMA-BUF file descriptors
- **Import**: InfiniBand adapters can import and access DMA-BUF memory
- **Zero-copy**: Direct RDMA operations on Gaudi memory

### 3. UCX Memory Key Packing
- **Remote keys**: Include DMA-BUF fd for IB access
- **Metadata**: Memory address, size, and sharing information
- **Transport**: Enables remote nodes to access Gaudi memory

## Test Programs

### Core Functionality Tests
- `gaudi_dmabuf_test.c` - Tests DMA-BUF export functionality
- `gaudi_ib_integration_test.c` - Tests Gaudi and InfiniBand integration
- `ucx_gaudi_info.c` - Displays Gaudi device information
- `basic_gaudi_test.c` - Basic Gaudi transport functionality
- `simple_gaudi_test.c` - Simple Gaudi memory allocation test

### Advanced Integration Tests
- `gaudi_memory_type_test.c` - UCS memory type support for Gaudi
- `gaudi_ucm_test.c` - UCM memory hooks for Gaudi device memory

### Test Automation Scripts
- `run_all_gaudi_tests.sh` - **Comprehensive test suite** - Runs all Gaudi tests systematically with detailed logging
- `gaudi_stress_tests.sh` - **Stress testing** - Memory allocation, DMA-BUF, and UCX integration stress tests
- `quick_verify.sh` - **Quick verification** - Fast check of Gaudi integration status
- `demo.sh` - Interactive demonstration script

### Usage Examples

#### Quick Status Check
```bash
./quick_verify.sh
```

#### Full Test Suite
```bash
./run_all_gaudi_tests.sh
```

#### Stress Testing
```bash
./gaudi_stress_tests.sh
```

#### Interactive Demo
```bash
./demo.sh
```

### Individual Test Programs

### `gaudi_dmabuf_test`
Tests basic DMA-BUF functionality:
```bash
./gaudi_dmabuf_test -s 1048576 -v
```
- Allocates Gaudi device memory
- Exports as DMA-BUF
- Tests basic DMA-BUF operations
- Validates UCX integration

### `gaudi_ib_integration_test`
Tests Gaudi-IB integration:
```bash
./gaudi_ib_integration_test -s 65536 -d mlx5 -v
```
- Allocates Gaudi memory with DMA-BUF export
- Opens InfiniBand memory domain
- Tests memory key packing/unpacking
- Validates zero-copy path

### `ucx_gaudi_info`
Displays Gaudi transport information:
```bash
./ucx_gaudi_info -a -v
```
- Shows available Gaudi components
- Lists transport capabilities
- Displays memory domain features
- Shows configuration options

## Building

Use the provided Makefile:
```bash
cd test/gaudi
make all
```

The Makefile automatically picks up the UCX build environment and links against:
- UCX libraries (libuct, libucs, libucp)
- Habana Labs hlthunk library
- System libraries

## Usage Example

### Step 1: Allocate Gaudi Memory with DMA-BUF
```c
uct_md_mem_alloc(gaudi_md, &size, &address, UCS_MEMORY_TYPE_GAUDI, 
                 UCS_SYS_DEVICE_ID_UNKNOWN, UCT_MD_MEM_FLAG_FIXED, 
                 "my_buffer", &memh);
```

### Step 2: Pack Memory Key for IB Sharing
```c
uct_md_mkey_pack(gaudi_md, memh, address, size, &params, rkey_buffer);
// rkey_buffer now contains DMA-BUF fd and metadata
```

### Step 3: Share with Remote Node
```c
// Send rkey_buffer to remote node via control channel
send_to_remote(rkey_buffer, rkey_size);
```

### Step 4: Remote Node Accesses Gaudi Memory
```c
// Remote node receives rkey and attaches to Gaudi memory
uct_md_mem_attach(ib_md, rkey_buffer, &params, &remote_memh);
// Now can perform RDMA directly to Gaudi memory
uct_ep_put_short(ib_ep, data, size, remote_address, remote_memh);
```

## Configuration

### Environment Variables
```bash
# Enable Gaudi DMA-BUF support
export UCT_GAUDI_COPY_DMABUF=yes

# Select specific Gaudi device
export UCT_GAUDI_COPY_DEVICE=0

# IB device selection
export UCT_IB_DEVICE=mlx5_0
```

### UCX Configuration
```bash
# List available transports
ucx_info -d

# Check Gaudi transport
ucx_info -d | grep gaudi

# Test memory bandwidth
ucx_perftest -t gaudi_copy -m host
```

## Requirements

### Hardware
- Habana Gaudi/Gaudi2 accelerator cards
- InfiniBand HCA (Mellanox ConnectX-5/6/7 recommended)
- System with PCIe slots for both devices

### Software
- UCX with Gaudi transport (this implementation)
- Habana Labs drivers and hlthunk library
- InfiniBand drivers (MLNX_OFED or inbox)
- Linux kernel with DMA-BUF support

### System Configuration
- IOMMU enabled for device memory sharing
- Proper permissions for device access
- Network configuration for IB fabric

## Troubleshooting

### Common Issues

1. **DMA-BUF export fails**
   - Check Gaudi driver version
   - Verify hlthunk library installation
   - Ensure proper device permissions

2. **IB component not found**
   - Install InfiniBand drivers
   - Check UCX IB transport build
   - Verify HCA detection

3. **Memory sharing fails**
   - Check IOMMU configuration
   - Verify kernel DMA-BUF support
   - Test with smaller buffer sizes

### Debugging
```bash
# Enable UCX debug logging
export UCS_LOG_LEVEL=debug
export UCT_LOG_LEVEL=debug

# Run test with verbose output
./gaudi_ib_integration_test -v

# Check system logs
dmesg | grep -i "gaudi\|ib\|dma"
```

## Performance Considerations

### Optimization Tips
1. **Large transfers**: Use large buffer sizes (>1MB) for best bandwidth
2. **Memory alignment**: Align buffers to page boundaries
3. **Pinned memory**: Keep frequently used buffers registered
4. **CPU affinity**: Bind processes to NUMA nodes near devices

### Expected Performance
- **Bandwidth**: Up to 200 GB/s (HDR InfiniBand)
- **Latency**: Sub-microsecond for small messages
- **CPU overhead**: Minimal for large transfers

## Future Enhancements

1. **Multi-device support**: Handle multiple Gaudi devices
2. **Memory pools**: Pre-allocated DMA-BUF memory pools
3. **GPU Direct**: Integration with GPU Direct RDMA
4. **Compression**: Hardware compression for network transfers
5. **Monitoring**: Performance monitoring and telemetry

## References

- [UCX Documentation](https://openucx.readthedocs.io/)
- [Habana Labs Developer Guide](https://docs.habana.ai/)
- [InfiniBand Architecture](https://www.infinibandta.org/)
- [DMA-BUF Framework](https://www.kernel.org/doc/html/latest/driver-api/dma-buf.html)
