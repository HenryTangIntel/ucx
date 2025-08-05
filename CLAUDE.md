# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

UCX (Unified Communication X) is an award-winning, optimized production-proven communication framework for modern high-bandwidth and low-latency networks. It provides abstractions for RDMA (InfiniBand and RoCE), TCP, GPUs, shared memory, and network atomic operations.

### Architecture

UCX consists of four main components:
- **UCP (Protocol)**: High-level abstractions (tag-matching, streams, connection negotiation, multi-rail)
- **UCT (Transport)**: Low-level communication primitives (active messages, RMA, atomics)
- **UCS (Services)**: Data structures, algorithms, and system utilities
- **UCM (Memory)**: Memory allocation/release event interception for registration cache

### Supported Transports
- InfiniBand, Omni-Path, RoCE, Cray Gemini/Aries
- CUDA, ROCm, Intel Level Zero (ZE)
- Shared memory (POSIX, SysV, CMA, KNEM, XPMEM)
- TCP/IP

**Note**: UCX >= 1.12.0 requires rdma-core >= 28.0 or MLNX_OFED >= 5.0

## Build System

UCX uses autotools (autoconf/automake). Always run `./autogen.sh` first when building from git (not needed for release tarballs).

### Development Build
```bash
./autogen.sh
./contrib/configure-devel --prefix=$PWD/install-debug
make -j8
```
**Note**: Development builds include significant performance penalties due to debugging code.

### Release Build
```bash  
./autogen.sh
./contrib/configure-release --prefix=/where/to/install
make -j8
make install
```

### Other Build Options
```bash
# Profile build
./contrib/configure-prof

# Optimized build
./contrib/configure-opt

# Multi-threaded release
./contrib/configure-release-mt

# Build documentation
make docs

# Build packages
contrib/buildrpm.sh -s -b        # RPM
dpkg-buildpackage -us -uc        # DEB
```

### Performance Testing
```bash
# Build perftest tool
make -C src/tools/perf

# Run server
./src/tools/perf/ucx_perftest -c 0

# Run client (different terminal/host)
./src/tools/perf/ucx_perftest <server-hostname> -t tag_lat -c 1
```

## Testing

### Unit Tests (GTest)
```bash
make -C test/gtest test
```

### MPI Tests
```bash
make -C test/mpi test
```

### Performance Tests
Performance tests are in `src/tools/perf/` and provide various benchmark types:
- Latency tests: `*_lat` (pingpong pattern)
- Bandwidth tests: `*_bw` (stream pattern) 
- Message rate tests: `*_mr`
- Available APIs: UCT (low-level) and UCP (high-level)

### Test Categories
- `am_*`: Active message tests
- `put_*`/`get_*`: RMA (Remote Memory Access) tests
- `tag_*`: Tagged message matching tests (UCP only)
- `add_*`/`fadd_*`/`swap_*`/`cswap_*`: Atomic operation tests

## Code Style Guidelines

From `docs/CodeStyle.md`:
- 4 spaces, no tabs
- Up to 80 columns
- Single space around operators
- Functions must begin with `ucp_/uct_/ucs_/ucm_`
- Macros must begin with `UCP_/UCT_/UCS_/UCM_`
- Output pointer arguments have `_p` suffix
- Value types have `_t` suffix
- API handles have `_h` suffix
- No leading underscores in function names
- Macro arguments begin with `_` (e.g., `_value`)

### Header File Suffixes
- `_fwd.h`: Forward declarations
- `_types.h`: Type declarations  
- `.inl`: Inline functions
- `_def.h`: Preprocessor macros

### Include Order
1. `config.h`
2. Specific internal header
3. UCX headers
4. System headers

### C++ Guidelines (Tests Only)
- Used only for unit testing with C++11 features
- Prefer references over pointers, `auto` for type deduction
- Use move semantics, `constexpr`, `using` instead of `typedef`

## Key Directories

- `src/ucp/`: UCP protocol layer implementation
- `src/uct/`: UCT transport layer with specific transport implementations
  - `src/uct/ib/`: InfiniBand/RoCE transports
  - `src/uct/tcp/`: TCP transport
  - `src/uct/cuda/`: CUDA GPU memory support
  - `src/uct/rocm/`: ROCm GPU memory support
- `src/ucs/`: Common services and utilities
- `src/ucm/`: Memory management hooks
- `src/tools/perf/`: Performance testing framework
- `test/gtest/`: C++ unit tests using Google Test
- `contrib/`: Build configuration scripts and utilities

## Performance Test Framework

The performance testing is organized around:
- `perftest.c`: Main test runner and test type definitions
- `lib/libperf.c`: Core performance testing library
- `perftest_params.c`: Parameter parsing and validation
- `perftest_run.c`: Test execution logic

Test types are defined in `tests[]` array with different APIs (UCT/UCP), commands (PUT/GET/TAG/etc.), and patterns (PINGPONG/STREAM_UNI).

## Common Tasks

### Adding New Transport
1. Create transport directory under `src/uct/`
2. Implement transport interface (`*_iface.c`, `*_ep.c`, `*_md.c`)
3. Add configuration in `configure.m4`
4. Update `Makefile.am` files

### Performance Test Development
- Tests are defined in `src/tools/perf/perftest.c` 
- Library functions in `src/tools/perf/lib/`
- Memory allocators for different memory types in subdirectories (`cuda/`, `rocm/`, `ze/`)

### Building Specific Components
```bash
# Build only UCP
make -C src/ucp

# Build only performance tools  
make -C src/tools/perf

# Build with specific configure options
./configure --enable-debug --enable-profiling --enable-stats
```

## Development Workflow

### Configure Options (available via `./configure --help`)
- `--enable-debug`: Enable debug build
- `--enable-profiling`: Enable profiling support
- `--enable-stats`: Enable statistics collection
- `--enable-gtest`: Enable Google Test framework
- `--with-valgrind`: Enable Valgrind support
- `--enable-mt`: Enable multi-threading support

### Running Single Tests
```bash
# Run specific GTest
cd test/gtest && make && ./gtest --gtest_filter="*test_name*"

# Run MPI tests
make -C test/mpi test

# Run specific performance test type
./src/tools/perf/ucx_perftest <server> -t tag_lat -c 1
```

### Known Issues
- UCX may hang with glibc 2.25-2.29 due to pthread_rwlock bugs
- DCv5 MLX5 requires UCX_DC_MLX5_RX_INLINE=0 with rdma-core v22 (fixed in v24)