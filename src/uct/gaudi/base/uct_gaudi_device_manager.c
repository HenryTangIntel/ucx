#include "uct_gaudi_device_manager.h"

#include <habanalabs/hlthunk.h>
#include <ucs/debug/log.h>
#include <pthread.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>

#define UCT_GAUDI_MAX_DEVICES 16

/* Holds the state for a single device */
typedef struct uct_gaudi_device_state {
    int fd;
    int ref_count;
} uct_gaudi_device_state_t;

/* Global state for the device manager */
static uct_gaudi_device_state_t G_gaudi_devices[UCT_GAUDI_MAX_DEVICES];
static pthread_mutex_t G_manager_mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_once_t G_init_once = PTHREAD_ONCE_INIT;

/**
 * One-time initialization for the manager.
 */
static void uct_gaudi_manager_init()
{
    int i;
    for (i = 0; i < UCT_GAUDI_MAX_DEVICES; ++i) {
        G_gaudi_devices[i].fd = -1;
        G_gaudi_devices[i].ref_count = 0;
    }
}

ucs_status_t uct_gaudi_device_get_handle(int device_index, int *fd_p)
{
    char env_name[64];
    const char *env_val;
    int new_fd = -1;
    ucs_status_t status = UCS_OK;

    if ((device_index < 0) || (device_index >= UCT_GAUDI_MAX_DEVICES)) {
        ucs_error("Gaudi device index %d is out of range", device_index);
        return UCS_ERR_INVALID_PARAM;
    }

    /* Ensure the manager is initialized exactly once */
    pthread_once(&G_init_once, uct_gaudi_manager_init);

    pthread_mutex_lock(&G_manager_mutex);

    if (G_gaudi_devices[device_index].ref_count > 0) {
        /* Fast path: Device is already open, just increment ref count */
        G_gaudi_devices[device_index].ref_count++;
        *fd_p = G_gaudi_devices[device_index].fd;
        goto out_unlock;
    }

    /* First time opening this device: check env var or call hlthunk_open */
    snprintf(env_name, sizeof(env_name), "UCX_GAUDI_DEVICE_FD_%d", device_index);
    env_val = getenv(env_name);

    if (env_val != NULL) {
        /* Application provided the handle. Duplicate it for safety. */
        int app_fd = atoi(env_val);
        ucs_debug("Found %s=%d, duplicating handle", env_name, app_fd);
        new_fd = dup(app_fd);
        if (new_fd < 0) {
            ucs_error("dup() failed for fd %d (from %s): %m", app_fd, env_name);
            status = UCS_ERR_IO_ERROR;
            goto out_unlock;
        }
    } else {
        /* UCX is responsible for opening the device */
        ucs_debug("Calling hlthunk_open for device %d", device_index);
        new_fd = hlthunk_open(device_index, NULL);
        if (new_fd < 0) {
            ucs_error("hlthunk_open(device=%d) failed: %m", device_index);
            status = UCS_ERR_NO_DEVICE;
            goto out_unlock;
        }
    }

    /* Success: store the new handle and set ref count to 1 */
    G_gaudi_devices[device_index].fd        = new_fd;
    G_gaudi_devices[device_index].ref_count = 1;
    *fd_p                                   = new_fd;

out_unlock:
    pthread_mutex_unlock(&G_manager_mutex);
    return status;
}

ucs_status_t uct_gaudi_device_put_handle(int device_index)
{
    ucs_status_t status = UCS_OK;

    if ((device_index < 0) || (device_index >= UCT_GAUDI_MAX_DEVICES)) {
        ucs_error("Gaudi device index %d is out of range", device_index);
        return UCS_ERR_INVALID_PARAM;
    }

    pthread_mutex_lock(&G_manager_mutex);

    if (G_gaudi_devices[device_index].ref_count == 0) {
        ucs_warn("put_handle called on device %d which is not open", device_index);
        status = UCS_ERR_INVALID_PARAM;
        goto out_unlock;
    }

    G_gaudi_devices[device_index].ref_count--;

    if (G_gaudi_devices[device_index].ref_count == 0) {
        /* Last user released the handle, so close it */
        ucs_debug("Closing handle for device %d", device_index);
        hlthunk_close(G_gaudi_devices[device_index].fd);
        G_gaudi_devices[device_index].fd = -1;
    }

out_unlock:
    pthread_mutex_unlock(&G_manager_mutex);
    return status;
}
