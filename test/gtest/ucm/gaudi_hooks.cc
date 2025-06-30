/*
 * Copyright (C) 2025, [Your Name/Organization]. ALL RIGHTS RESERVED.
 * See file LICENSE for terms.
 */

#include <ucm/api/ucm.h>
#include <common/test.h>
#include <hlthunk.h> // Adjust include as needed for Gaudi thunk API

static ucm_event_t alloc_event, free_event;

static void gaudi_mem_alloc_callback(ucm_event_type_t event_type,
                                     ucm_event_t *event, void *arg)
{
    alloc_event.mem_type.address  = event->mem_type.address;
    alloc_event.mem_type.size     = event->mem_type.size;
    alloc_event.mem_type.mem_type = event->mem_type.mem_type;
}

static void gaudi_mem_free_callback(ucm_event_type_t event_type,
                                    ucm_event_t *event, void *arg)
{
    free_event.mem_type.address  = event->mem_type.address;
    free_event.mem_type.size     = event->mem_type.size;
    free_event.mem_type.mem_type = event->mem_type.mem_type;
}
enum hlthunk_device_name devices[] = {
        HLTHUNK_DEVICE_GAUDI3,
        HLTHUNK_DEVICE_GAUDI2,
        HLTHUNK_DEVICE_GAUDI,
        HLTHUNK_DEVICE_DONT_CARE
};
    

class gaudi_ucm_hooks : public ucs::test {
protected:
    int fd;
    uint64_t handle;

    virtual void init() {
        ucs_status_t result;
        uint64_t size = 4096;
        int ret;
        ucs::test::init();

        // Open Gaudi device (template, adjust as needed)
        fd = hlthunk_open(devices[1],NULL);
        ASSERT_GT(fd, 0);

        // Install memory hooks
        result = ucm_set_event_handler(UCM_EVENT_MEM_TYPE_ALLOC, 0,
                                       gaudi_mem_alloc_callback,
                                       reinterpret_cast<void*>(this));
        ASSERT_UCS_OK(result);

        result = ucm_set_event_handler(UCM_EVENT_MEM_TYPE_FREE, 0,
                                       gaudi_mem_free_callback,
                                       reinterpret_cast<void*>(this));
        ASSERT_UCS_OK(result);

        // Allocate Gaudi device memory (simulate/test)
        handle = hlthunk_device_memory_alloc(fd, size, 4096, true, true);
        ASSERT_EQ(0, ret);
        check_mem_alloc_events((void*)(uintptr_t)handle, size, UCS_MEMORY_TYPE_GAUDI);

        // Free Gaudi device memory
        ret = hlthunk_device_memory_free(fd, handle);
        ASSERT_EQ(0, ret);
        check_mem_free_events((void*)(uintptr_t)handle, 0, UCS_MEMORY_TYPE_GAUDI);
    }

    virtual void cleanup() {
        ucm_unset_event_handler(UCM_EVENT_MEM_TYPE_ALLOC,
                                gaudi_mem_alloc_callback,
                                reinterpret_cast<void*>(this));
        ucm_unset_event_handler(UCM_EVENT_MEM_TYPE_FREE,
                                gaudi_mem_free_callback,
                                reinterpret_cast<void*>(this));
        // Close Gaudi device (template, adjust as needed)
        if (fd > 0) {
            hlthunk_close(fd);
        }
        ucs::test::cleanup();
    }

    void check_mem_alloc_events(void *ptr, size_t size,
                                int expect_mem_type = UCS_MEMORY_TYPE_GAUDI)  {
        ASSERT_EQ(ptr, alloc_event.mem_type.address);
        ASSERT_EQ(size, alloc_event.mem_type.size);
        EXPECT_TRUE((alloc_event.mem_type.mem_type == expect_mem_type) ||
                    (alloc_event.mem_type.mem_type == UCS_MEMORY_TYPE_UNKNOWN));
    }

    void check_mem_free_events(void *ptr, size_t size,
                               int expect_mem_type = UCS_MEMORY_TYPE_GAUDI) {
        printf("[DEBUG] ptr: %p, free_event.mem_type.address: %p, size: %lu, free_event.size: %lu\n",
               ptr, free_event.mem_type.address, (unsigned long)size, (unsigned long)free_event.mem_type.size);
        ASSERT_EQ(ptr, free_event.mem_type.address);
        ASSERT_EQ(expect_mem_type, free_event.mem_type.mem_type);
    }
};

UCS_TEST_F(gaudi_ucm_hooks, memory_intercept) {
    // All logic is in init()
}
