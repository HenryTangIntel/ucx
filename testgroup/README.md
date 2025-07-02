# UCX Gaudi Test Suite

## Successfully Built Test Programs

This directory contains the compiled UCX Gaudi integration tests:

### 🔧 Core UCX Gaudi Transport Tests
- **`gaudi_uct_direct_test`** - Direct UCX Gaudi transport layer integration test
- **`gaudi_dmabuf_mlx_test`** - Gaudi device memory over MLX/IB transport simulation
- **`gaudi_query_md_resources_test`** - Memory domain resource queries
- **`gaudi_async_test`** - Async/event functionality validation
- **`gaudi_comm_test`** - Communication layer tests

### 🔍 DMA-buf Integration Tests  
- **`test_dmabuf_comprehensive`** - Comprehensive DMA-buf functionality tests
- **`test_dmabuf_simple`** - Basic DMA-buf operations
- **`test_gaudi_dmabuf`** - Gaudi DMA-buf support validation

### ✅ Verification Tests
- **`gaudi_test_final`** - Final verification test for Gaudi component
- **`simple_test`** - Basic compilation test

## Build Summary

- ✅ **9 working test programs** successfully compiled
- 🎯 Tests exercise the real UCX Gaudi transport layer in `/workspace/ucx/src/uct/gaudi/copy/`
- 🔧 Uses actual UCT API calls (`uct_md_open`, `uct_md_mem_reg`, etc.)
- ⚠️ Some tests have API compatibility issues and were excluded from build

## Key Insight

These tests validate the **real UCX Gaudi integration**, not hl-thunk APIs in isolation. The UCX layer provides proper abstraction for integrating Gaudi device memory with network transports like MLX/IB.

## Usage

Run individual tests:
```bash
export LD_LIBRARY_PATH="../install/lib:$LD_LIBRARY_PATH"
./gaudi_uct_direct_test
./gaudi_dmabuf_mlx_test
```

Or use the Makefile for organized building:
```bash
make all          # Build all working tests
make gaudi        # Build Gaudi-specific tests  
make dmabuf       # Build DMA-buf tests
```
