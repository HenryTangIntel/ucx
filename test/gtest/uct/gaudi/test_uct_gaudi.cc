/*
 * Copyright (C) 2023, [Your Name/Organization Here]. ALL RIGHTS RESERVED.
 * See file LICENSE for terms.
 */

#include <common/test.h>
#include <uct/api/uct.h>
#include <ucs/sys/sys.h> // For ucs_global_opts

// Basic test fixture for Gaudi UCT tests
class test_uct_gaudi : public uct_test {
public:
    test_uct_gaudi() {
        // Potentially set Gaudi-specific environment variables here if needed for tests
        // For example, to select a specific device or mode.
    }

protected:
    // Helper to find a UCT MD component by name
    bool find_md_resource(const std::string& md_name_to_find, uct_md_resource_desc_t& found_resource) {
        uct_md_resource_desc_t *md_resources;
        unsigned num_md_resources;
        ucs_status_t status;

        status = uct_query_md_resources(&md_resources, &num_md_resources);
        if (status != UCS_OK) {
            return false;
        }

        bool found = false;
        for (unsigned i = 0; i < num_md_resources; ++i) {
            if (md_name_to_find == md_resources[i].md_name) {
                found_resource = md_resources[i];
                found = true;
                break;
            }
        }

        uct_release_md_resource_list(md_resources);
        return found;
    }
};

// Test if the "gaudi_md" component can be found
TEST_F(test_uct_gaudi, find_gaudi_md) {
#if HAVE_GAUDI
    uct_md_resource_desc_t gaudi_md_resource;
    bool found = find_md_resource("gaudi_md", gaudi_md_resource);
    if (!found) {
        // It's possible that the placeholder detection in configure.m4 passed,
        // but the actual Gaudi libraries (libhlthunk) are not present on the test system.
        // Or, the query_resources function in gaudi_md.c isn't working as expected.
        // We should not fail catastrophically if Gaudi is *meant* to be available but isn't at runtime.
        // However, if --with-gaudi was explicitly "yes", this might indicate a problem.
        UCS_TEST_MESSAGE << "Gaudi MD (gaudi_md) not found. This might be expected if Gaudi libraries are not installed, or if --with-gaudi=auto and no devices detected.";
        // Depending on strictness, could use GTEST_SKIP or just a warning.
        // For now, let's assume if HAVE_GAUDI is set, we expect it.
        // However, the dummy query_resources currently *always* returns a "gaudi" MD.
    }
    ASSERT_TRUE(found) << "Gaudi MD component (gaudi_md) was not found by uct_query_md_resources.";

    // Try to open the MD
    uct_md_config_t *md_config = NULL;
    status = uct_md_config_read(gaudi_md_resource.md_name, NULL, NULL, &md_config);
    ASSERT_UCS_OK(status) << "Failed to read Gaudi MD config";

    uct_md_h gaudi_md;
    status = uct_md_open(gaudi_md_resource.md_name, md_config, &gaudi_md);
    uct_config_release(md_config); // Release config regardless of md_open outcome

    ASSERT_UCS_OK(status) << "Failed to open Gaudi MD: " << gaudi_md_resource.md_name;
    ASSERT_NE(nullptr, gaudi_md) << "Gaudi MD handle is null after successful open.";

    uct_md_close(gaudi_md);
#else
    UCS_TEST_MESSAGE << "Gaudi support not compiled in, skipping Gaudi MD test.";
    GTEST_SKIP();
#endif
}

// TODO: Add more tests as functionality is implemented:
// - Test memory allocation/registration (once mkey_pack is more than a stub)
// - Test interface query and open
// - Test endpoint creation
// - Basic data transfer tests (put/get/am)

// Note: If ucs_global_opts.skip_tests_on_missing_transports is set,
// UCT tests might skip this automatically if the transport isn't found.
// The explicit HAVE_GAUDI check is for clarity and to ensure it's only run
// when compiled with Gaudi support.

// Register the test fixture with GTest
// This is usually done in a main test file or a common header for the test group.
// For now, this file is self-contained for simplicity of adding it.
// If there's a central uct_test_main.cc or similar, it might not be needed here.
// UCS_TEST_MAIN_FUNC_REQUIRED_DEF; // Might not be needed if part of a larger suite.
