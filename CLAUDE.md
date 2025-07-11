# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

UCX (Unified Communication X) is a high-performance communication framework for modern networks, supporting RDMA, TCP, GPUs, shared memory, and atomic operations. This is a C/C++ codebase using GNU autotools for building.

## Build System and Common Commands

### Development Build
```bash
./autogen.sh
./contrib/configure-devel --prefix=$PWD/install-debug
make -j8
make install
```

### Release Build  
```bash
./autogen.sh
./contrib/configure-release --prefix=/where/to/install
make -j8
make install
```

### Testing
```bash
# Run unit tests
make -C test/gtest test

# Performance tests
./src/tools/perf/ucx_perftest -c 0  # server
./src/tools/perf/ucx_perftest <hostname> -t tag_lat -c 1  # client
```

### Documentation
```bash
make docs  # Build Doxygen documentation
```

### Other Utilities
```bash
./src/tools/info/ucx_info -d     # Display device information
./src/tools/info/ucx_info -c     # Display configuration
```

## Code Architecture

UCX is organized as a layered architecture with four main components:

### Core Components

1. **UCP (Protocol Layer)** - `/src/ucp/`
   - High-level communication abstractions (tag-matching, streams, connections)
   - Key subdirs: `api/`, `core/`, `proto/`, `tag/`, `rma/`, `wireup/`

2. **UCT (Transport Layer)** - `/src/uct/`
   - Low-level transport primitives (active messages, RMA, atomics)
   - Transport implementations: `ib/`, `tcp/`, `sm/`, `cuda/`, `rocm/`, `gaudi/`

3. **UCS (Services Layer)** - `/src/ucs/`
   - Common utilities: data structures, memory management, debugging, configuration
   - Key subdirs: `datastruct/`, `memory/`, `debug/`, `async/`, `sys/`, `arch/`

4. **UCM (Memory Layer)** - `/src/ucm/`
   - Memory allocation/release event interception
   - Hardware-specific memory handlers for CUDA, ROCm, Gaudi

### Key Design Patterns

- **Plugin Architecture**: Transports implemented as separate components with runtime selection
- **Object-Oriented C**: Function pointers for polymorphism, struct embedding for inheritance
- **Performance-First**: Zero-copy operations, inline functions (`.inl` files), lock-free data structures
- **Memory Pool Management**: Custom allocators and registration caching

### Important Files for Development

#### API Headers
- `/src/ucp/api/ucp.h` - Main UCP API
- `/src/uct/api/uct.h` - Main UCT API
- `/src/ucm/api/ucm.h` - Memory management API

#### Core Implementation
- `/src/ucp/core/ucp_context.h` - UCP context management
- `/src/uct/base/uct_component.h` - Transport plugin interface
- `/src/ucs/datastruct/` - Core data structures and algorithms

#### Examples and Tools
- `/examples/` - Sample UCP/UCT applications
- `/src/tools/info/` - Diagnostic utilities
- `/src/tools/perf/` - Performance testing framework

## Development Guidelines

### Build Configuration
- Use `./contrib/configure-devel` for development builds (includes debugging, profiling, tests)
- Use `./contrib/configure-release` for production builds (optimized, minimal debugging)
- Many features conditionally compiled based on available hardware/libraries

### Component Structure
- Each transport in `/src/uct/` has its own Makefile.am and configure.m4
- Memory handlers in `/src/ucm/` follow similar plugin pattern
- Architecture-specific code in `/src/ucs/arch/` for x86_64, ARM, PowerPC, RISC-V

### Testing Strategy
- Unit tests in `/test/gtest/` organized by component (ucp, uct, ucs, ucm)
- Integration tests in `/test/gaudi/` for hardware-specific features
- Performance tests via `/src/tools/perf/ucx_perftest`

### Memory Management
- Custom memory pools used throughout codebase
- Memory registration caching for performance
- UCM provides hooks for tracking memory allocation events
- Be aware of memory type detection (host, CUDA, ROCm, etc.)

## Hardware Support

UCX supports multiple transports and accelerators:
- **Networking**: InfiniBand, RoCE, TCP, Omni-Path, Cray Gemini/Aries  
- **Shared Memory**: POSIX, SysV, CMA, KNEM, XPMEM
- **Accelerators**: NVIDIA CUDA, AMD ROCm, Intel Gaudi, Intel Level Zero
- **CPU Architectures**: x86_64, ARM v8, PowerPC 8/9, RISC-V

Transport selection happens at runtime based on available hardware and configuration.