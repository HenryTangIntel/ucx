/* SPDX-License-Identifier: MIT
 *
 * Copyright 2025 HabanaLabs, Ltd.
 *
 */

#include <string.h>
#include "gaudi_memory_copy.h"
#include "uapi/hlthunk.h"

int gaudi_memory_copy_h2d(int fd, void *dst, const void *src, size_t size)
{
	struct hlthunk_memory_copy_args args = {
		.dst_handle = (uint64_t)dst,
		.src_handle = (uint64_t)src,
		.size = size,
		.dma_dir = HLTHUNK_DMA_DIR_HOST_TO_DEVICE,
	};

	return hlthunk_memory_copy(fd, &args);
}

int gaudi_memory_copy_d2h(int fd, void *dst, const void *src, size_t size)
{
	struct hlthunk_memory_copy_args args = {
		.dst_handle = (uint64_t)dst,
		.src_handle = (uint64_t)src,
		.size = size,
		.dma_dir = HLTHUNK_DMA_DIR_DEVICE_TO_HOST,
	};

	return hlthunk_memory_copy(fd, &args);
}

int gaudi_memory_copy(int fd, void *dst, memory_location_t dst_location,
			const void *src, memory_location_t src_location,
			size_t size)
{
	if (src_location == MEMORY_LOCATION_HOST && dst_location == MEMORY_LOCATION_DEVICE) {
		return gaudi_memory_copy_h2d(fd, dst, src, size);
	} else if (src_location == MEMORY_LOCATION_DEVICE && dst_location == MEMORY_LOCATION_HOST) {
		return gaudi_memory_copy_d2h(fd, dst, src, size);
	} else if (src_location == MEMORY_LOCATION_DEVICE && dst_location == MEMORY_LOCATION_DEVICE) {
		struct hlthunk_memory_copy_args args = {
			.dst_handle = (uint64_t)dst,
			.src_handle = (uint64_t)src,
			.size = size,
			.dma_dir = HLTHUNK_DMA_DIR_DEVICE_TO_DEVICE,
		};
		return hlthunk_memory_copy(fd, &args);
	} else if (src_location == MEMORY_LOCATION_HOST && dst_location == MEMORY_LOCATION_HOST) {
		memcpy(dst, src, size);
		return 0;
	}

	return -1;
}
