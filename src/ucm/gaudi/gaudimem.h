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
#include <hlthunk.h>
#endif


#ifdef __cplusplus
extern "C" {
#endif

int ucm_hlthunk_allocate_device_memory(int device_id, void **dptr, size_t size);
int ucm_hlthunk_free_device_memory(int device_id, void *dptr);

#ifdef __cplusplus
}
#endif

#endif // UCM_GAUDI_MEM_H_
