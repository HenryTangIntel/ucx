#ifndef UCT_GAUDI_DEVICE_MANAGER_H
#define UCT_GAUDI_DEVICE_MANAGER_H

#include <ucs/type/status.h>

/**
 * @brief Acquires a handle to a Gaudi device.
 *
 * This function provides a thread-safe, reference-counted mechanism to get a
 * Gaudi device handle (file descriptor).
 *
 * It handles two scenarios:
 * 1. Application-Owned Handle: If the environment variable
 *    "UCX_GAUDI_DEVICE_FD_<device_index>" is set, this function will dup()
 *    the provided file descriptor.
 * 2. UCX-Owned Handle: If the environment variable is not set, this function
 *    will call hlthunk_open() itself.
 *
 * For internal UCX use, it ensures hlthunk_open() is only called once per
 * device, with subsequent requests returning a cached handle.
 *
 * @param [in]  device_index  The index of the Gaudi device.
 * @param [out] fd_p          Pointer to be filled with the file descriptor.
 *
 * @return UCS_OK on success, or an error code on failure.
 */
ucs_status_t uct_gaudi_device_get_handle(int device_index, int *fd_p);


/**
 * @brief Releases a handle to a Gaudi device.
 *
 * This function decrements the reference count for the device handle. When the
 * reference count drops to zero, it will call hlthunk_close() on the file
 * descriptor that was acquired by uct_gaudi_device_get_handle().
 *
 * @param [in] device_index  The index of the Gaudi device to release.
 *
 * @return UCS_OK on success, or an error code if the device was not open.
 */
ucs_status_t uct_gaudi_device_put_handle(int device_index);

#endif // UCT_GAUDI_DEVICE_MANAGER_H
