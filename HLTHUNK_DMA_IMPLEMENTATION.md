# Proposed Asynchronous DMA API for the `hl-thunk` Library

This document provides a concrete, plausible implementation for adding asynchronous DMA (Direct Memory Access) capabilities to the `hl-thunk` library. The primary goal is to provide a non-blocking API that allows a user-space application like UCX to initiate a data transfer and then poll for its completion, enabling the crucial overlap of communication and computation.

The implementation is based on a standard model where the user-space library communicates with the kernel driver via `ioctl` calls.

---

## 1. Proposed Additions to `hlthunk.h`

To expose the new functionality, the following declarations should be added to the public header file, `include/uapi/hlthunk.h`.

```c
// Add these declarations inside hlthunk.h

/**
 * @brief Opaque handle representing an in-progress asynchronous DMA operation.
 *
 * This structure is intentionally left undefined in the public header to
 * hide implementation details from the consumer.
 */
typedef struct hlthunk_dma_request hlthunk_dma_request_t;

/**
 * @brief Initiates an asynchronous DMA transfer from the device to the host.
 *
 * This function submits a DMA command to the hardware and returns immediately.
 * The completion of the operation must be tracked by polling the returned
 * request handle.
 *
 * @param fd File descriptor of the Gaudi device.
 * @param host_addr Destination address in host memory.
 * @param device_addr Source address on the Gaudi device.
 * @param size Number of bytes to copy.
 * @param request_p Pointer to a location to store the newly created request handle.
 * @return 0 on success, negative error code on failure.
 */
hlthunk_public int
hlthunk_dma_device_to_host_async(int fd, void *host_addr,
                                 uint64_t device_addr, uint64_t size,
                                 hlthunk_dma_request_t **request_p);

/**
 * @brief Polls the status of an asynchronous DMA operation.
 *
 * This function checks if the hardware has completed the DMA transfer
 * associated with the request handle. This is a non-blocking call.
 *
 * @param fd File descriptor of the Gaudi device.
 * @param request The DMA request handle to poll.
 * @return 0 if the operation is complete.
 *         -EAGAIN or -EINPROGRESS if the operation is still in progress.
 *         Other negative error codes on failure.
 */
hlthunk_public int hlthunk_dma_poll(int fd, hlthunk_dma_request_t *request);

/**
 * @brief Frees the resources associated with a DMA request handle.
 *
 * This should be called after hlthunk_dma_poll() has returned success (0).
 *
 * @param request The DMA request handle to free.
 */
hlthunk_public void hlthunk_dma_request_free(hlthunk_dma_request_t *request);

/*
 * NOTE: Similar prototypes would be added for:
 * - hlthunk_dma_host_to_device_async()
 * - hlthunk_dma_device_to_device_async()
 */
```

---

## 2. Sample Implementation (`src/dma.c`)

This is the core logic that would reside in a new source file within the `hl-thunk` library. It demonstrates how the user-space functions wrap `ioctl` calls to the kernel driver.

```c
#include <sys/ioctl.h>
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>

#include "uapi/hlthunk.h"

// --- Hypothetical Kernel Driver Interface ---
// These definitions illustrate the contract between hl-thunk and the
// habanalabs.ko kernel driver. They would be defined in a shared header.

// The ioctl command numbers used to invoke kernel functions.
#define HLSYS_IOCTL_DMA_D2H   _IOWR('H', 0x10, struct hlsys_dma_args)
#define HLSYS_IOCTL_WAIT_DMA  _IOWR('H', 0x11, struct hlsys_wait_args)

// Structure for submitting a D2H DMA command to the kernel.
struct hlsys_dma_args {
    uint64_t host_addr;     // IN: Destination host address
    uint64_t device_addr;   // IN: Source device address
    uint64_t size;          // IN: Size of transfer
    uint64_t seq;           // OUT: Kernel returns a unique sequence number for this job
};

// Structure for waiting/polling for a DMA command's completion.
struct hlsys_wait_args {
    uint64_t seq;           // IN: The sequence number to check
    uint64_t timeout_us;    // IN: Timeout (0 for a non-blocking poll)
    int32_t  status;        // OUT: Kernel returns the completion status
};

// --- End of Hypothetical Kernel Interface ---


/**
 * @brief Internal definition for the opaque DMA request handle.
 * This is the "concrete" type for hlthunk_dma_request_t.
 */
struct hlthunk_dma_request {
    uint64_t seq; // The sequence number returned by the kernel
};


/**
 * @brief Implementation of the asynchronous D2H DMA function.
 */
int hlthunk_dma_device_to_host_async(int fd, void *host_addr,
                                     uint64_t device_addr, uint64_t size,
                                     hlthunk_dma_request_t **request_p)
{
    if (!host_addr || !request_p || size == 0) {
        return -EINVAL;
    }

    // 1. Allocate a request handle to track this specific operation.
    hlthunk_dma_request_t *req = malloc(sizeof(*req));
    if (!req) {
        return -ENOMEM;
    }

    // 2. Prepare the arguments for the ioctl call to the kernel.
    struct hlsys_dma_args dma_args = {
        .host_addr   = (uint64_t)host_addr,
        .device_addr = device_addr,
        .size        = size
    };

    // 3. Call the kernel driver to submit the DMA command. This ioctl is
    //    designed to be non-blocking. It queues the command with the hardware
    //    and returns immediately.
    if (ioctl(fd, HLSYS_IOCTL_DMA_D2H, &dma_args) < 0) {
        perror("ioctl(HLSYS_IOCTL_DMA_D2H)");
        free(req);
        return -errno;
    }

    // 4. The kernel has accepted the command and returned a unique sequence
    //    number. Store it in our request handle.
    req->seq = dma_args.seq;

    // 5. Return the opaque handle to the caller (e.g., UCX).
    *request_p = req;

    return 0; // Success
}

/**
 * @brief Implementation of the non-blocking poll function.
 */
int hlthunk_dma_poll(int fd, hlthunk_dma_request_t *request)
{
    if (!request) {
        return -EINVAL;
    }

    // 1. Prepare arguments for the wait/poll ioctl.
    struct hlsys_wait_args wait_args = {
        .seq        = request->seq,
        .timeout_us = 0 // A timeout of 0 makes this a non-blocking poll.
    };

    // 2. Call the kernel to check the status of our sequence number.
    if (ioctl(fd, HLSYS_IOCTL_WAIT_DMA, &wait_args) < 0) {
        perror("ioctl(HLSYS_IOCTL_WAIT_DMA)");
        return -errno;
    }

    // 3. Interpret the status returned by the kernel.
    if (wait_args.status == 0) {
        // 0 means the operation is complete.
        return 0;
    } else {
        // A non-zero status like EAGAIN means it's still in progress.
        return -EAGAIN;
    }
}

/**
 * @brief Implementation of the request free function.
 */
void hlthunk_dma_request_free(hlthunk_dma_request_t *request)
{
    if (request) {
        free(request);
    }
}
```

---

## 3. How UCX Would Use This New API

This implementation provides the exact primitives that the UCX `gaudi_copy` transport needs. The lifecycle of a transfer would be:

1.  **Submission:** When `uct_gaudi_copy_ep_get_zcopy` is called for a D2H transfer, it will call `hlthunk_dma_device_to_host_async()`.

2.  **Tracking:** It receives the `hlthunk_dma_request_t*` handle. It stores this handle in its own internal UCX request object (`ucs_request_t`) and returns `UCS_INPROGRESS` to the higher layers.

3.  **Polling:** During calls to `ucp_worker_progress()`, the `gaudi_copy` transport's progress function (`uct_gaudi_copy_iface_progress()`) is called. It iterates through its list of pending requests. For each one, it gets the stored `hlthunk_dma_request_t*` and calls `hlthunk_dma_poll()`.

4.  **Completion:**
    - If `hlthunk_dma_poll()` returns `-EAGAIN`, the operation is still in progress. UCX does nothing and continues its progress loop.
    - If `hlthunk_dma_poll()` returns `0`, the operation is complete. UCX will then:
        1.  Call `hlthunk_dma_request_free()` to release the `hl-thunk` handle.
        2.  Trigger the `uct_completion_t` callback to notify the application that the data is now available in the host buffer.
        3.  Free its own internal `ucs_request_t`.

This design correctly separates concerns: `hl-thunk` provides the minimal, low-level primitives to command the hardware asynchronously, and UCX builds the higher-level progress, scheduling, and completion logic on top of it.
