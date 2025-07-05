# Copyright (c) 2025, Habana Labs Ltd. an Intel Company. ALL RIGHTS RESERVED.
# See file LICENSE for terms.

if HAVE_GAUDI

# Gaudi-specific UCT tests
gaudi_test_sources = \
    test/gtest/uct/gaudi/test_gaudi_md.cc \
    test/gtest/uct/gaudi/test_gaudi_dma.cc \
    test/gtest/uct/gaudi/test_gaudi_transport.cc \
    test/gtest/uct/gaudi/test_gaudi_performance.cc \
    test/gtest/uct/gaudi/test_gaudi_advanced.cc \
    test/gtest/uct/gaudi/test_gaudi_error_handling.cc \
    test/gtest/uct/gaudi/test_gaudi_integration.cc \
    test/gtest/uct/gaudi/test_gaudi_dma_comprehensive.cc \
    test/gtest/uct/gaudi/test_gaudi_stress.cc

# Add Gaudi tests to the main UCT test suite
test_uct_SOURCES += $(gaudi_test_sources)

# Include directories for Gaudi tests
test_uct_CPPFLAGS += -I$(abs_top_srcdir)/src/uct/gaudi

# Libraries required for Gaudi tests
test_uct_LDADD += $(GAUDI_LIBS) -lhlthunk

# Test data files (if any)
gaudi_test_data = 

EXTRA_DIST += $(gaudi_test_data)

endif # HAVE_GAUDI

# Test configurations for Gaudi
gaudi_test_configs = \
    test/gtest/uct/gaudi/gaudi_copy.conf \
    test/gtest/uct/gaudi/gaudi_ipc.conf \
    test/gtest/uct/gaudi/gaudi_advanced.conf

EXTRA_DIST += $(gaudi_test_configs)

# Test scripts for Gaudi integration
gaudi_test_scripts = \
    test/gtest/uct/gaudi/run_gaudi_tests.sh \
    test/gtest/uct/gaudi/run_gaudi_tests_enhanced.sh

EXTRA_DIST += $(gaudi_test_scripts)

# Install test headers for external testing
gaudi_test_headers = \
    test/gtest/uct/gaudi/test_gaudi_common.h

noinst_HEADERS += $(gaudi_test_headers)
