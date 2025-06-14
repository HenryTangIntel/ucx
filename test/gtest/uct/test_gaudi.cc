// Copyright (c) 2025. All rights reserved.
// Gaudi UCX transport unit test example

// Copyright (c) 2025. All rights reserved.
// Gaudi UCX transport unit test

#include "test_md.h"
extern "C" {
#include <uct/gaudi/gaudi_md.h>
}

class test_gaudi : public test_md {
    // Optionally override protected methods if needed
};


UCS_TEST_P(test_gaudi, basic_md_query) {
    uct_md_attr_t md_attr;
    ucs_status_t status = uct_md_query(md(), &md_attr);
    EXPECT_UCS_OK(status);
    EXPECT_TRUE(md_attr.cap.flags & UCT_MD_FLAG_REG);
    // Add more Gaudi-specific checks here
}

// Instantiate the test for the Gaudi MD
_UCT_MD_INSTANTIATE_TEST_CASE(test_gaudi, gaudi)
