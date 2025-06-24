/**
 * Copyright (c) NVIDIA CORPORATION & AFFILIATES, 2001-2021. ALL RIGHTS RESERVED.
 * Copyright (C) Habana Labs Ltd. 2024. ALL RIGHTS RESERVED.
 * See file LICENSE for terms.
 */
#include <ucm/api/ucm.h>
#include <ucs/debug/assert.h>
#include <ucs/sys/ptr_arith.h>
#include <common/test.h>
#include <hlthunk.h> // Assuming this provides Gaudi memory functions
#include "mock_hlthunk.h"

class gaudi_hooks : public ucs::test {
protected:
    virtual void init() {
        ucs_status_t result;
        ucs::test::init();

        m_alloc_events.reserve(1000);
        m_free_events.reserve(1000);

        result = ucm_set_event_handler(UCM_EVENT_MEM_TYPE_ALLOC, 0,
                                       gaudi_mem_alloc_callback, this);
        ASSERT_UCS_OK(result);

        result = ucm_set_event_handler(UCM_EVENT_MEM_TYPE_FREE, 0,
                                       gaudi_mem_free_callback, this);
        ASSERT_UCS_OK(result);
    }

    virtual void cleanup()
    {
        ucm_unset_event_handler(UCM_EVENT_MEM_TYPE_FREE, gaudi_mem_free_callback,
                                this);
        ucm_unset_event_handler(UCM_EVENT_MEM_TYPE_ALLOC,
                                gaudi_mem_alloc_callback, this);
        ucs::test::cleanup();
    }

    void check_mem_alloc_events(
            void *ptr, size_t size,
            ucs_memory_type_t expect_mem_type = UCS_MEMORY_TYPE_GAUDI_DEVICE) const
    {
        check_event_present(m_alloc_events, "alloc", ptr, size,
                            expect_mem_type);
    }

    void check_mem_free_events(void *ptr, size_t size) const
    {
        check_event_present(m_free_events, "free", ptr, size);
    }

private:
    struct mem_event {
        void              *address;
        size_t            size;
        ucs_memory_type_t mem_type;
    };

    using mem_event_vec_t = std::vector<mem_event>;

    void check_event_present(
            const mem_event_vec_t &events, const std::string &name, void *ptr,
            size_t size,
            ucs_memory_type_t mem_type = UCS_MEMORY_TYPE_UNKNOWN) const
    {
        for (const auto &e : events) {
            if (/* Start address match */
                (ptr >= e.address) &&
                /* End address match */
                (UCS_PTR_BYTE_OFFSET(ptr, size) <=
                 UCS_PTR_BYTE_OFFSET(e.address, e.size)) &&
                /* Memory type match */
                ((e.mem_type == mem_type) ||
                 (e.mem_type == UCS_MEMORY_TYPE_UNKNOWN))) {
                return;
            }
        }

        FAIL() << "Could not find memory " << name << " event for " << ptr
               << ".." << UCS_PTR_BYTE_OFFSET(ptr, size) << " type "
               << ucs_memory_type_names[mem_type];
    }

    static void push_event(mem_event_vec_t &events, const mem_event &e)
    {
        ucs_assertv(events.size() < events.capacity(), "size=%zu capacity=%zu",
                    events.size(), events.capacity());
        events.push_back(e);
    }

    void mem_alloc_event(void *address, size_t size, ucs_memory_type_t mem_type)
    {
        push_event(m_alloc_events, {address, size, mem_type});
    }

    void mem_free_event(void *address, size_t size)
    {
        push_event(m_free_events, {address, size, UCS_MEMORY_TYPE_UNKNOWN});
    }

    static void gaudi_mem_alloc_callback(ucm_event_type_t event_type,
                                         ucm_event_t *event, void *arg)
    {
        auto self = reinterpret_cast<gaudi_hooks*>(arg);
        self->mem_alloc_event(event->mem_type.address, event->mem_type.size,
                              event->mem_type.mem_type);
    }

    static void gaudi_mem_free_callback(ucm_event_type_t event_type,
                                        ucm_event_t *event, void *arg)
    {
        auto self = reinterpret_cast<gaudi_hooks*>(arg);
        self->mem_free_event(event->mem_type.address, event->mem_type.size);
    }

    mem_event_vec_t m_alloc_events;
    mem_event_vec_t m_free_events;
};

UCS_TEST_F(gaudi_hooks, test_hlthunk_allocate_device_memory_free) {
    void *dptr;
    size_t size = 64;
    int device_id = 0; // Assuming device 0 for testing

    // Allocate device memory
    int ret = hlthunk_allocate_device_memory(device_id, &dptr, size);
    ASSERT_EQ(ret, 0); // Assuming 0 for success
    check_mem_alloc_events(dptr, size, UCS_MEMORY_TYPE_GAUDI_DEVICE);

    // Free device memory
    ret = hlthunk_free_device_memory(device_id, dptr);
    ASSERT_EQ(ret, 0); // Assuming 0 for success
    check_mem_free_events(dptr, size);

    // Large allocation
    size = 256 * UCS_MBYTE;
    ret = hlthunk_allocate_device_memory(device_id, &dptr, size);
    ASSERT_EQ(ret, 0);
    check_mem_alloc_events(dptr, size, UCS_MEMORY_TYPE_GAUDI_DEVICE);

    ret = hlthunk_free_device_memory(device_id, dptr);
    ASSERT_EQ(ret, 0);
    check_mem_free_events(dptr, size);
}