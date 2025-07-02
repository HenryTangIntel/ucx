/*
 * Copyright (C) 2023, [Your Name/Organization Here]. ALL RIGHTS RESERVED.
 * See file LICENSE for terms.
 */

#ifndef UCM_GAUDI_MEM_H_
#define UCM_GAUDI_MEM_H_

#include <ucm/api/ucm.h>

#if HAVE_CONFIG_H
#  include "config.h"
#endif

// Include the Gaudi thunk header if Gaudi support is compiled in UCT
#if HAVE_GAUDI
#include <habanalabs/hlthunk.h>
#endif



#ifdef __cplusplus
extern "C" {
#endif

uint64_t ucm_hlthunk_device_memory_alloc(int fd, uint64_t size, uint64_t page_size, bool contiguous, bool shared);
int ucm_hlthunk_device_memory_free(int fd, uint64_t handle);
uint64_t ucm_hlthunk_device_memory_map(int fd, uint64_t handle, uint64_t hint_addr);
int ucm_hlthunk_device_memory_unmap(int fd, uint64_t addr);

#ifdef __cplusplus
}
#endif

#endif // UCM_GAUDI_MEM_H_
