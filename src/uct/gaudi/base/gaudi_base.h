/*
 * Copyright (c) Habana Labs, Ltd. 2024. All rights reserved.
 *
 * See file LICENSE for terms.
 */

#ifndef UCT_GAUDI_BASE_H_
#define UCT_GAUDI_BASE_H_

#include <ucs/status.h>

ucs_status_t uct_gaudi_base_init(void);
void uct_gaudi_base_cleanup(void);

#endif