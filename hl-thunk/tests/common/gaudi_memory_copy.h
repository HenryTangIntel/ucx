/* SPDX-License-Identifier: MIT
 *
 * Copyright 2025 HabanaLabs, Ltd.
 *
 */

#ifndef GAUDI_MEMORY_COPY_H
#define GAUDI_MEMORY_COPY_H

#include <stddef.h>
#include "hlthunk.h"

/**
 * @brief An enum to specify the location of a memory buffer.
 */
typedef enum {
	MEMORY_LOCATION_HOST,
	MEMORY_LOCATION_DEVICE
} memory_location_t;

/**
 * @brief Performs a memory copy between host and/or device.
 *
 * This function is a wrapper that handles memory copies in all four directions:
 * - Host to Device (H2D)
 * - Device to Host (D2H)
 * - Device to Device (D2D)
 * - Host to Host (H2H)
 *
 * The function determines the appropriate copy mechanism based on the source
 * and destination location enums. For any copy involving the device, it uses
 * a DMA transfer. For a host-only copy, it uses a standard memcpy.
 *
 * @param fd The file descriptor for the device.
 * @param dst Pointer to the destination memory buffer.
 * @param dst_location The location of the destination buffer (HOST or DEVICE).
 * @param src Pointer to the source memory buffer.
 * @param src_location The location of the source buffer (HOST or DEVICE).
 * @param size The number of bytes to copy.
 * @return 0 on success, or a negative value on failure.
 */
int gaudi_memory_copy(int fd, void *dst, memory_location_t dst_location,
			const void *src, memory_location_t src_location,
			size_t size);

int gaudi_memory_copy_h2d(int fd, void *dst, const void *src, size_t size);

int gaudi_memory_copy_d2h(int fd, void *dst, const void *src, size_t size);

#endif /* GAUDI_MEMORY_COPY_H */
