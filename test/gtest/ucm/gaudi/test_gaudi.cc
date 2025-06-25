#ifdef HAVE_GAUDI
UCS_TEST_F(test_ucm_gaudi, hlthunk_allocate_and_free_device_memory) {
    int device_id = 0; // Use device 0 for testing
    void *dptr = NULL;
    size_t size = 4096;

    int ret = ucm_hlthunk_allocate_device_memory(device_id, &dptr, size);
    EXPECT_EQ(ret, 0);
    EXPECT_NE(dptr, nullptr);

    ret = ucm_hlthunk_free_device_memory(device_id, dptr);
    EXPECT_EQ(ret, 0);
}
#endif
