
#ifdef HAVE_GAUDI
UCS_TEST_F(test_ucm_gaudi, hlthunk_device_memory_alloc_and_free) {
    int fd = 0; // Use fd 0 for testing (replace with a valid fd if needed)
    uint64_t size = 4096;
    uint64_t page_size = 4096;
    bool contiguous = true;
    bool shared = false;
    uint64_t handle = 0;

    int ret = ucm_hlthunk_device_memory_alloc(fd, size, page_size, contiguous, shared, &handle);
    EXPECT_EQ(ret, 0);
    EXPECT_NE(handle, 0u);

    ret = ucm_hlthunk_device_memory_free(fd, handle);
    EXPECT_EQ(ret, 0);
}
#endif
