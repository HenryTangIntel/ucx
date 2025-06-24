#include <gtest/gtest.h>
#include <ucs/type/status.h>
#include <ucm/api/ucm.h>

// Declare the functions we're testing from the real library
extern "C" {
ucs_status_t ucm_gaudi_mem_init(void);
void ucm_gaudi_mem_cleanup(void);
}

class test_gaudi : public ::testing::Test {
protected:
    void SetUp() override {
        // No init needed as we do it in the test
    }

    void TearDown() override {
        // No cleanup needed as we do it in the test
    }
};

TEST_F(test_gaudi, init_cleanup) {
    ucs_status_t status;

    status = ucm_gaudi_mem_init();
    ASSERT_EQ(UCS_OK, status);

    // The test is just verifying we can call these functions
    ucm_gaudi_mem_cleanup();
}

// Test double initialization
TEST_F(test_gaudi, double_init) {
    ucs_status_t status;
    
    status = ucm_gaudi_mem_init();
    ASSERT_EQ(UCS_OK, status);
    
    // Second init should still return OK (idempotent initialization)
    status = ucm_gaudi_mem_init();
    ASSERT_EQ(UCS_OK, status);
    
    // Cleanup only once needed since our implementation is a no-op
    ucm_gaudi_mem_cleanup();
}

// Test init-cleanup-init cycle
TEST_F(test_gaudi, init_cleanup_cycle) {
    ucs_status_t status;
    
    // First cycle
    status = ucm_gaudi_mem_init();
    ASSERT_EQ(UCS_OK, status);
    ucm_gaudi_mem_cleanup();
    
    // Second cycle should work too
    status = ucm_gaudi_mem_init();
    ASSERT_EQ(UCS_OK, status);
    ucm_gaudi_mem_cleanup();
}

#if !HAVE_GAUDI
TEST_F(test_gaudi, unsupported) {
    // This test only makes sense when Gaudi support is NOT compiled in
    ucs_status_t status = ucm_gaudi_mem_init();
    ASSERT_EQ(UCS_ERR_UNSUPPORTED, status);
}
#endif

