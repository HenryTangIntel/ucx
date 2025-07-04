/* SPDX-License-Identifier: MIT
 *
 * Copyright 2025 HabanaLabs, Ltd.
 *
 */

#include <string.h>
#include "gaudi_memory_copy.h"
#include "hlthunk_tests.h"

static int gaudi_memory_copy_common(int fd, void *dst, const void *src, size_t size,
					enum hltests_dma_direction dma_dir)
{
	struct hltests_cs_chunk execute_arr;
	struct hltests_pkt_info pkt_info;
	uint32_t cb_size = 0;
	void *cb;
	uint64_t seq;
	int rc;

	cb = hltests_create_cb(fd, 0x1000, EXTERNAL, 0);
	if (!cb)
		return -1;

	memset(&pkt_info, 0, sizeof(pkt_info));
	pkt_info.eb = EB_TRUE;
	pkt_info.mb = MB_TRUE;
	pkt_info.dma.src_addr = (uint64_t)src;
	pkt_info.dma.dst_addr = (uint64_t)dst;
	pkt_info.dma.size = size;
	pkt_info.dma.dma_dir = dma_dir;

	if (dma_dir == DMA_DIR_HOST_TO_DRAM || dma_dir == DMA_DIR_HOST_TO_SRAM)
		pkt_info.qid = hltests_get_dma_down_qid(fd, STREAM0);
	else
		pkt_info.qid = hltests_get_dma_up_qid(fd, STREAM0);

	cb_size = hltests_add_dma_pkt(fd, cb, cb_size, &pkt_info);

	execute_arr.cb_ptr = cb;
	execute_arr.cb_size = cb_size;
	execute_arr.queue_index = pkt_info.qid;

	rc = hltests_submit_cs_timeout(fd, NULL, 0, &execute_arr, 1, 0, 30, &seq);
	if (rc) {
		hltests_destroy_cb(fd, cb);
		return rc;
	}

	rc = hltests_wait_for_cs_until_not_busy(fd, seq);
	hltests_destroy_cb(fd, cb);
	return rc;
}

int gaudi_memory_copy_h2d(int fd, void *dst, const void *src, size_t size)
{
	return gaudi_memory_copy_common(fd, dst, src, size, DMA_DIR_HOST_TO_DRAM);
}

int gaudi_memory_copy_d2h(int fd, void *dst, const void *src, size_t size)
{
	return gaudi_memory_copy_common(fd, dst, src, size, DMA_DIR_DRAM_TO_HOST);
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
		return gaudi_memory_copy_common(fd, dst, src, size, DMA_DIR_DRAM_TO_DRAM);
	} else if (src_location == MEMORY_LOCATION_HOST && dst_location == MEMORY_LOCATION_HOST) {
		memcpy(dst, src, size);
		return 0;
	}

	return -1;
}