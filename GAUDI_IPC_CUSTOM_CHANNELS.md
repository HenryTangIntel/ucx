# Gaudi IPC Custom Channel Implementation

## Overview

This document describes the custom channel-based IPC (Inter-Process Communication) implementation for Gaudi accelerators within UCX. Unlike traditional IPC models that rely on memory handle sharing, this implementation uses dedicated communication channels for high-performance, node-local device-to-device transfers.

## Architecture

### Core Components

1. **Memory Domain (gaudi_ipc_md.c)**
   - Manages node-local device detection
   - Creates and maintains custom channels between device pairs
   - Handles memory registration with channel association

2. **Cache System (gaudi_ipc_cache.c)**
   - Caches channel mappings for efficient reuse
   - Supports both traditional and channel-based memory regions
   - Provides fallback mechanisms for compatibility

3. **Interface Layer (gaudi_ipc_iface.c)**
   - Standard UCX transport interface
   - Configurable bandwidth and latency parameters
   - Device reachability detection

4. **Endpoint Operations (gaudi_ipc_ep.c)**
   - Zero-copy PUT/GET operations
   - Automatic channel selection for optimal performance
   - Fallback to DMA when channels are unavailable

### Custom Channel Model

#### Channel Creation
```c
ucs_status_t uct_gaudi_ipc_channel_create(uct_gaudi_ipc_md_t *md, 
                                          uint32_t src_device, uint32_t dst_device,
                                          uint32_t *channel_id);
```

- Establishes bidirectional communication channel between two Gaudi devices
- Returns unique channel ID for subsequent operations
- Channels are cached and reused across multiple transfers

#### Channel-Based Copy
```c
ucs_status_t uct_gaudi_ipc_channel_copy(uct_gaudi_ipc_md_t *md,
                                        uint32_t channel_id,
                                        void *dst, void *src, size_t length);
```

- Direct device-to-device memory transfer using custom channel
- Bypasses host memory staging for maximum performance
- Leverages hardware-specific optimizations

### Key Data Structures

#### Enhanced Remote Key
```c
typedef struct {
    uct_gaudi_ipc_md_handle_t  ph;              /* Traditional IPC handle */
    pid_t                      pid;             /* Process ID */
    void*                      d_bptr;          /* Device base pointer */
    size_t                     b_len;           /* Buffer length */
    uint32_t                   src_device_id;   /* Source device ID */
    uint32_t                   dst_device_id;   /* Destination device ID */
    uint32_t                   channel_id;      /* Custom channel ID */
} uct_gaudi_ipc_rkey_t;
```

#### Memory Domain with Channel Infrastructure
```c
typedef struct uct_gaudi_ipc_md {
    uct_md_t                 super;
    int                      device_count;      /* Number of Gaudi devices */
    int                     *device_fds;        /* Device file descriptors */
    uint64_t                *channel_map;       /* Channel mapping matrix */
    pthread_mutex_t          channel_lock;      /* Channel operations lock */
} uct_gaudi_ipc_md_t;
```

## Required hl-thunk Library Functions

The implementation depends on these custom channel functions that need to be implemented in the hl-thunk library:

### Channel Management
```c
/* Create custom communication channel between devices */
int hlthunk_ipc_channel_create(int src_fd, int dst_fd, uint32_t *channel_id);

/* Destroy communication channel */
int hlthunk_ipc_channel_destroy(uint32_t channel_id);

/* Execute copy operation through custom channel */
int hlthunk_ipc_channel_copy(uint32_t channel_id, int src_fd, int dst_fd,
                            void *dst, void *src, size_t length);
```

### Kernel Driver Support

The hl-thunk functions will require corresponding kernel IOCTL operations:

```c
#define DRM_IOCTL_HL_IPC_CHANNEL_CREATE  /* Create inter-device channel */
#define DRM_IOCTL_HL_IPC_CHANNEL_DESTROY /* Destroy channel */
#define DRM_IOCTL_HL_IPC_CHANNEL_COPY    /* Channel-based copy */
```

## Usage Flow

### 1. Initialization
- UCX detects all Gaudi devices in the node
- Opens file descriptors for each device
- Initializes channel mapping infrastructure

### 2. Memory Registration
- Applications register device memory regions
- Memory is associated with source device information
- Channel IDs are assigned for optimal routing

### 3. Communication Setup
- Remote keys are packed with channel information
- Destination processes unpack keys with device details
- Channels are created on-demand between device pairs

### 4. Data Transfer
- PUT/GET operations use channel-based mapping when possible
- Direct device-to-device transfers through custom channels
- Automatic fallback to traditional DMA when needed

## Performance Benefits

### Zero-Copy Transfers
- Direct device-to-device communication eliminates host staging
- Custom channels optimized for Gaudi architecture
- Reduced latency and increased bandwidth

### Efficient Caching
- Channel mappings cached for reuse
- Minimal overhead for repeated transfers
- Smart cache invalidation and cleanup

### Hardware Optimization
- Leverages Gaudi-specific interconnect capabilities
- Optimized for node-local device topology
- Maximum utilization of available bandwidth

## Fallback Mechanisms

### Traditional IPC Support
- Maintains compatibility with standard IPC handles
- Seamless fallback when custom channels unavailable
- Support for cross-node communication via existing mechanisms

### DMA Fallback
- Uses standard DMA engines when channels fail
- Maintains functional correctness in all scenarios
- Gradual degradation of performance rather than failure

## Configuration

### Build-Time Detection
```autoconf
AC_CHECK_DECLS([hlthunk_ipc_channel_create,
                hlthunk_ipc_channel_copy,
                hlthunk_ipc_channel_destroy], [], [], [#include <hlthunk.h>])
```

### Runtime Parameters
- `GAUDI_IPC_BW`: Effective bandwidth configuration
- `GAUDI_IPC_LAT`: Latency parameters
- `GAUDI_IPC_OVERHEAD`: CPU overhead estimation

## Integration Checklist

### hl-thunk Library
- [ ] Implement `hlthunk_ipc_channel_create()`
- [ ] Implement `hlthunk_ipc_channel_copy()`
- [ ] Implement `hlthunk_ipc_channel_destroy()`
- [ ] Add channel management data structures

### Kernel Driver
- [ ] Add channel creation IOCTL
- [ ] Add channel copy IOCTL
- [ ] Add channel destruction IOCTL
- [ ] Implement channel resource management

### UCX Integration
- [ ] Update configure scripts for detection
- [ ] Remove stub implementations
- [ ] Add comprehensive testing
- [ ] Performance validation

## Testing Strategy

### Unit Tests
- Channel creation and destruction
- Memory registration with channels
- Cache management operations

### Integration Tests
- End-to-end PUT/GET operations
- Multi-process communication
- Fallback mechanism validation

### Performance Tests
- Bandwidth measurement
- Latency profiling
- Scalability testing with multiple devices

## Future Enhancements

### Multi-Node Support
- Extend channel model for cluster communication
- Hybrid local/remote channel management
- Network-aware channel selection

### Advanced Optimizations
- Async channel operations
- Batch transfer optimization
- Dynamic channel load balancing

This custom channel approach provides a foundation for high-performance, scalable IPC communication specifically optimized for Gaudi accelerator architectures within HPC and AI workloads.
