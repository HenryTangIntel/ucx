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
#include <uapi/hlthunk.h>
#endif

ucs_status_t ucm_gaudi_mem_init(void);

void ucm_gaudi_mem_cleanup(void);

// Potentially add functions here to be called by UCM events if Gaudi
// has specific memory event notifications or registration mechanisms.
// For example:
// void ucm_gaudi_mmap_hook(void *addr, size_t length, int prot, int flags, int fd, off_t offset);
// void ucm_gaudi_munmap_hook(void *addr, size_t length);

#endif // UCM_GAUDI_MEM_H_
