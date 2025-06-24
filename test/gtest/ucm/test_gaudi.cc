#include <gtest/gtest.h>
#include <ucm/api/ucm.h>

// If you have a specific Gaudi header, include it here
// #include <ucm/gaudi/gaudimem.h>

TEST(ucm_gaudi, basic_init)
{
    // If you have a Gaudi-specific init function, call it here.
    // For now, just check that UCM is initialized.
    EXPECT_EQ(ucm_init(), UCS_OK);
    // Add more Gaudi-specific checks as needed.
}