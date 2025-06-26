# UCX Gaudi Module

This module provides UCX integration for Intel Habanalabs Gaudi AI accelerator hardware.

## Overview

The Gaudi module implements memory domain functionality within UCX to support direct access to Gaudi device memory. This enables high-performance communication for AI workloads leveraging Gaudi hardware.

## Components

The module consists of the following key components:

- **Memory Domain (MD)**: Implements `uct_md_ops_t` for memory registration, allocation, and other memory operations
- **Component**: Registers the Gaudi component with UCX using `UCT_COMPONENT_DEFINE`
- **Transport Layer**: Provides access to Gaudi device memory

## Hardware Requirements

- Intel Habanalabs Gaudi AI accelerator
- Habanalabs kernel driver (`habanalabs`)
- Habanalabs runtime library (`libhl-thunk.so`)

## Testing the Module

Several test utilities are provided to verify the module's functionality:

### Basic Module Check

To check if the module is properly registered with UCX:

```bash
./gaudi_test_check
```

### Comprehensive Module Test

Tests all functional aspects of the module:

```bash
./gaudi_test_final
```

### Hardware Operations Test

Tests actual memory registration and operations with Gaudi hardware:

```bash
./gaudi_hardware_test
```

### Hardware Status Check

Checks if Gaudi hardware and drivers are properly installed:

```bash
./check_gaudi_hardware.sh
```

### All-in-One Test Script

Run all tests with a single command:

```bash
./test_gaudi_module.sh
```

## Module Structure

The module implements these primary interfaces:

1. **UCT Component Interface**:
   - Component registration via `UCT_COMPONENT_DEFINE`
   - Memory domain resource discovery

2. **Memory Domain (MD) Interface**:
   - Memory registration functions
   - Memory allocation functions
   - Device handling

## Development Status

The module has been successfully integrated with UCX and includes:

- [x] Component registration
- [x] Memory domain implementation
- [x] Core API functions
- [ ] Advanced memory operations
- [ ] Optimized transfer paths

## Future Work

- Performance optimizations for data transfers
- Support for zero-copy operations
- Integration with collective operations

## License

This module is covered under the same license as the UCX project.

## Support

For issues or questions related to the Gaudi module, please contact the UCX development team.
