# Gaudi Integration Unit Tests for UCX

This directory contains comprehensive unit tests for the Gaudi (Habana) accelerator integration in UCX.

## Test Files Overview

### Core Test Files

1. **test_gaudi_md.cc** - Memory Domain tests
   - MD initialization and cleanup
   - Memory allocation/deallocation  
   - Memory registration/deregistration
   - Memory handle tracking
   - Key packing/unpacking

2. **test_gaudi_dma.cc** - Basic DMA functionality tests
   - DMA operation validation
   - Data integrity verification
   - Performance measurements
   - Command buffer management

3. **test_gaudi_transport.cc** - Transport layer tests
   - Interface and endpoint lifecycle
   - Data transfer operations
   - Transport capabilities
   - Error handling

4. **test_gaudi_performance.cc** - Performance benchmarking
   - Bandwidth measurements
   - Latency analysis
   - Throughput testing
   - Performance comparisons

### Advanced Test Files

5. **test_gaudi_advanced.cc** - Advanced functionality tests
   - Edge case handling
   - Multiple simultaneous allocations
   - Memory registration edge cases
   - System device detection
   - Memory handle stress testing
   - Mixed memory types
   - Concurrent operations

6. **test_gaudi_error_handling.cc** - Error handling and robustness
   - Invalid parameter handling
   - Memory allocation failures
   - Command buffer errors
   - Hardware info edge cases
   - System device detection errors
   - Memory mapping errors
   - Concurrent error scenarios
   - Resource cleanup after failures
   - Timeout detection
   - MD error handling

7. **test_gaudi_integration.cc** - End-to-end integration tests
   - Transport capabilities verification
   - Small, medium, and large data transfers
   - Multiple concurrent transfers
   - Memory allocation and registration integration
   - Interface and endpoint lifecycle
   - Different transfer sizes and patterns
   - Multi-device detection
   - Performance characteristics

8. **test_gaudi_dma_comprehensive.cc** - Comprehensive DMA testing
   - Size variations (4B to 256KB+)
   - Data pattern testing (zeros, ones, alternating, sequential, random)
   - Alignment requirement testing
   - Performance analysis across different sizes
   - Concurrent DMA operations
   - Command buffer reuse
   - Device memory DMA (host-to-device, device-to-host)
   - Auto-detection logic
   - Error recovery

9. **test_gaudi_stress.cc** - Stress and load testing
   - Heavy memory allocation/deallocation (1000+ iterations)
   - Concurrent DMA stress (8 threads × 100 ops)
   - Memory handle tracking under pressure
   - Rapid interface creation/destruction
   - System resource exhaustion recovery

### Supporting Files

10. **test_gaudi_common.h** - Common utilities and helper functions
    - Device availability checking
    - Memory allocation helpers
    - Buffer filling and verification
    - Pattern generation
    - Timing utilities

## Configuration Files

- **gaudi_copy.conf** - Basic copy transport configuration
- **gaudi_ipc.conf** - IPC transport configuration  
- **gaudi_advanced.conf** - Advanced test parameters and settings

## Test Scripts

- **run_gaudi_tests.sh** - Basic test runner
- **run_gaudi_tests_enhanced.sh** - Enhanced test runner with:
  - Parallel execution support
  - Comprehensive logging
  - Test filtering and selection
  - Timeout management
  - Performance reporting
  - Error handling and recovery

## Test Coverage

### Functional Areas Tested

1. **Memory Management**
   - Allocation/deallocation of device memory
   - Host memory registration/deregistration
   - Memory handle tracking and lifecycle
   - Mixed memory type handling
   - Edge cases (zero size, very large allocations)

2. **DMA Operations**
   - Host-to-host DMA transfers
   - Host-to-device DMA transfers
   - Device-to-host DMA transfers
   - Various data sizes (4B to 16MB+)
   - Different data patterns and alignments
   - Concurrent DMA operations
   - Command buffer management and reuse

3. **Transport Layer**
   - Transport discovery and initialization
   - Interface and endpoint lifecycle
   - Data transfer operations
   - Key packing/unpacking
   - Remote memory operations

4. **System Integration**
   - Device detection and enumeration
   - PCI bus ID mapping
   - System device association
   - Multi-device environments
   - UCX framework integration

5. **Error Handling**
   - Invalid parameter handling
   - Resource exhaustion scenarios
   - Hardware failure simulation
   - Timeout handling
   - Recovery mechanisms

6. **Performance**
   - Bandwidth measurements
   - Latency analysis
   - Throughput testing
   - Scalability assessment
   - Concurrent operation performance

7. **Stress Testing**
   - High-frequency allocations
   - Long-running operations
   - Resource exhaustion and recovery
   - Concurrent multi-threaded access
   - Interface lifecycle stress

### Test Methodologies

1. **Unit Testing** - Individual component functionality
2. **Integration Testing** - Cross-component interactions
3. **Performance Testing** - Benchmark measurements
4. **Stress Testing** - High-load scenarios
5. **Error Testing** - Failure condition handling
6. **Regression Testing** - Compatibility verification

## Running the Tests

### Prerequisites
- UCX built with Gaudi support (`--with-gaudi`)
- Habana Labs hlthunk library
- Gaudi hardware available (optional for compilation tests)

### Basic Usage
```bash
# Run all tests
./run_gaudi_tests_enhanced.sh

# Run specific test categories
./run_gaudi_tests_enhanced.sh --advanced-only
./run_gaudi_tests_enhanced.sh --performance-only
./run_gaudi_tests_enhanced.sh --error-only

# Run with parallel execution
./run_gaudi_tests_enhanced.sh -j 4

# Run with verbose output
./run_gaudi_tests_enhanced.sh -v
```

### Manual Test Execution
```bash
# Run individual test suites
./test_uct --gtest_filter="test_gaudi_md.*"
./test_uct --gtest_filter="test_gaudi_dma.*"
./test_uct --gtest_filter="test_gaudi_advanced.*"
./test_uct --gtest_filter="test_gaudi_stress.*"
```

## Test Statistics

- **Total test files**: 9 main test files + 1 common header
- **Total test cases**: 100+ individual test cases
- **Test categories**: 7 major functional areas
- **Code coverage**: Comprehensive coverage of all Gaudi integration components
- **Performance tests**: Bandwidth, latency, and throughput measurements
- **Stress tests**: Multi-threaded, high-load scenarios
- **Error tests**: Comprehensive error and edge case handling

## Integration with UCX Build System

All tests are integrated into the UCX autotools build system:
- Conditional compilation based on Gaudi availability
- Proper linking with required libraries
- Integration with main UCX test suite
- Distribution of test configurations and scripts

## Continuous Integration

The tests are designed to be suitable for:
- Automated CI/CD pipelines
- Nightly regression testing
- Performance monitoring
- Quality assurance workflows

## Expected Outcomes

When all tests pass, you can be confident that:
1. Gaudi integration is functional and stable
2. Memory management is correct and leak-free
3. DMA operations work reliably with data integrity
4. Transport layer integrates properly with UCX
5. Error handling is robust and recovers gracefully
6. Performance meets expectations
7. Multi-threading and concurrent access is safe
8. System integration works across different configurations
