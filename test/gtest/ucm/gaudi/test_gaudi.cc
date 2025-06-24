/**
 * Copyright (C) 2023, Habana Labs. ALL RIGHTS RESERVED.
 * See file LICENSE for terms.
 */

#include <common/test.h>
#include <ucm/api/ucm.h>
#include <ucm/gaudi/gaudimem.h>

class test_ucm_gaudi : public ucs::test {
protected:
    virtual void init() {
        ucs::test::init();
    }

    virtual void cleanup() {
        ucs::test::cleanup();
    }
};

UCS_TEST_F(test_ucm_gaudi, init_cleanup) {
    ucs_status_t status;

    /* Test initialization */
    status = ucm_gaudi_mem_init();
#ifdef HAVE_GAUDI
    EXPECT_EQ(UCS_OK, status);
#else
    EXPECT_EQ(UCS_ERR_UNSUPPORTED, status);
#endif

    /* Test cleanup - should not crash or cause errors */
    ucm_gaudi_mem_cleanup();
}

UCS_TEST_F(test_ucm_gaudi, double_init) {
    ucs_status_t status;
    
    /* First init */
    status = ucm_gaudi_mem_init();
#ifdef HAVE_GAUDI
    EXPECT_EQ(UCS_OK, status);
#else
    EXPECT_EQ(UCS_ERR_UNSUPPORTED, status);
#endif

    /* Second init - should behave the same */
    status = ucm_gaudi_mem_init();
#ifdef HAVE_GAUDI
    EXPECT_EQ(UCS_OK, status);
#else
    EXPECT_EQ(UCS_ERR_UNSUPPORTED, status);
#endif

    /* Cleanup */
    ucm_gaudi_mem_cleanup();
}

UCS_TEST_F(test_ucm_gaudi, init_cleanup_cycle) {
    ucs_status_t status;
    
    for (int i = 0; i < 5; ++i) {
        status = ucm_gaudi_mem_init();
#ifdef HAVE_GAUDI
        EXPECT_EQ(UCS_OK, status);
#else
        EXPECT_EQ(UCS_ERR_UNSUPPORTED, status);
#endif

        ucm_gaudi_mem_cleanup();
    }
}

UCS_TEST_F(test_ucm_gaudi, unsupported_on_non_gaudi) {
#ifndef HAVE_GAUDI
    ucs_status_t status;
    
    status = ucm_gaudi_mem_init();
    EXPECT_EQ(UCS_ERR_UNSUPPORTED, status);
    
    ucm_gaudi_mem_cleanup(); /* Should be safe to call even if init failed */
#endif
}
