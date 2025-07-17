# UCX New Provider Implementation Guide

This guide provides a step-by-step walkthrough for creating a new UCT (Unified Communication Transport) provider in UCX. We will use the placeholder name `new_provider` for the new transport.

## 1. Create the Directory Structure

First, create a new directory for your provider under `src/uct/`. The typical structure includes subdirectories for the base implementation, and any specific APIs if needed.

```bash
mkdir -p src/uct/new_provider/base
```

Your initial file structure will look like this:

```
src/uct/new_provider/
├── Makefile.am
├── configure.m4
└── base/
    ├── new_provider_iface.c
    ├── new_provider_iface.h
    ├── new_provider_ep.c
    ├── new_provider_ep.h
    ├── new_provider_md.c
    └── new_provider_md.h
```

## 2. Integrate with the Build System

To make UCX aware of your new provider, you need to integrate it with the Autotools build system.

### `configure.m4`

Create `src/uct/new_provider/configure.m4`. This file contains the logic to detect the necessary libraries and hardware for your provider.

```m4
# src/uct/new_provider/configure.m4
AC_DEFUN([UCX_UCT_NEW_PROVIDER_CHECK], [
    AC_ARG_WITH([new_provider],
                [AS_HELP_STRING([--with-new_provider],
                                [build new_provider transport])],
                [], [with_new_provider=yes])

    AS_IF([test "$with_new_provider" = "yes"], [
        # Add checks for required libraries, headers, or devices here.
        # For example:
        # AC_CHECK_HEADER([new_provider.h], [],
        #                 [AC_MSG_ERROR([new_provider.h not found])])

        # Define a symbol if the provider is enabled
        AC_DEFINE([HAVE_UCT_NEW_PROVIDER], [1], [Define if new_provider is enabled])
        ucx_uct_components_all="$ucx_uct_components_all new_provider"
    ])
])
```

### `Makefile.am`

Create `src/uct/new_provider/Makefile.am` to define how your provider's source files are compiled.

```makefile
# src/uct/new_provider/Makefile.am
if HAVE_UCT_NEW_PROVIDER
noinst_LTLIBRARIES += src/uct/new_provider/libuct_new_provider.la
endif

src_uct_new_provider_libuct_new_provider_la_SOURCES = \
    base/new_provider_md.c \
    base/new_provider_iface.c \
    base/new_provider_ep.c

# Add any provider-specific CFLAGS or LDFLAGS here
# src_uct_new_provider_libuct_new_provider_la_CPPFLAGS = $(AM_CPPFLAGS) -I...
# src_uct_new_provider_libuct_new_provider_la_LDFLAGS = $(AM_LDFLAGS) -L...
# src_uct_new_provider_libuct_new_provider_la_LIBADD = ...
```

### Update `configure.ac`

Add your new `configure.m4` file to the main `configure.ac` in the root directory.

```m4
# In configure.ac, add this line in the UCT section
UCX_UCT_NEW_PROVIDER_CHECK
```

### Update `src/uct/Makefile.am`

Add your provider's subdirectory to `src/uct/Makefile.am`.

```makefile
# In src/uct/Makefile.am
SUBDIRS += new_provider
```

## 3. Implement the Memory Domain (MD)

The Memory Domain (`md`) is responsible for managing local hardware resources and memory registration.

## 4. Architectural Note on Memory Domains

A critical design decision is how to model your memory domains, which depends on the transport's purpose. There are two primary models in UCX for accelerator transports.

### Model 1: Multi-MD for `copy` Transports

*   **Purpose**: For transports that manage a specific device's memory and perform local operations (e.g., Host-to-Device, Device-to-Host). Examples include `cuda_copy`, `rocm_copy`, and `ze_copy`.
*   **Implementation**:
    *   The `query_resources` function should discover and report **one resource for each physical device**.
    *   The `md_open` function should allocate a **new, unique MD structure** for each resource it's asked to open (e.g., by using `ucs_malloc`).
    *   This results in **one MD instance per physical device**.

### Model 2: Singleton MD for `ipc` Transports

*   **Purpose**: For transports that manage the connections *between* multiple devices on the same node (Inter-Process Communication). Examples include `cuda_ipc` and `rocm_ipc`.
*   **Implementation**:
    *   The `query_resources` function should still report one resource for each physical device that can be an IPC endpoint.
    *   However, the `md_open` function should be implemented as a **singleton**. It should return a pointer to the **same, shared MD instance** every time it is called.
    *   This is typically achieved by defining a `static` MD variable inside `md_open` and using a `pthread_once` to initialize it the first time it's called.
    *   This single MD instance is responsible for discovering and managing all devices on the node to facilitate the connections between them.

Choosing the correct model is essential for aligning with the UCX architecture and ensuring correctness and performance.

### `base/new_provider_md.h`

```c
#ifndef UCT_NEW_PROVIDER_MD_H
#define UCT_NEW_PROVIDER_MD_H

#include <uct/base/uct_md.h>

// Define your MD configuration structure
typedef struct uct_new_provider_md_config {
    uct_md_config_t super;
    // Add provider-specific config fields here
} uct_new_provider_md_config_t;

// Define your MD structure
typedef struct uct_new_provider_md {
    uct_md_t super;
    // Add provider-specific resources here
} uct_new_provider_md_t;

extern uct_component_t uct_new_provider_component;

#endif
```

### `base/new_provider_md.c`

This file implements the core logic for your MD, including resource allocation and memory registration/deregistration.

```c
#include "new_provider_md.h"
#include <ucs/debug/log.h>

static ucs_status_t uct_new_provider_md_query(uct_md_h md, uct_md_attr_t *md_attr) {
    // Populate md_attr with your provider's capabilities
    md_attr->cap.flags         = UCT_MD_FLAG_REG;
    md_attr->cap.reg_mem_types = UCS_BIT(UCS_MEMORY_TYPE_HOST);
    md_attr->cap.mem_type      = UCS_MEMORY_TYPE_HOST;
    md_attr->cap.max_reg       = ULONG_MAX;
    md_attr->rkey_packed_size  = 0; /* No rkey needed for this example */
    memset(&md_attr->local_cpus, 0xff, sizeof(md_attr->local_cpus));
    return UCS_OK;
}

static ucs_status_t uct_new_provider_mem_reg(uct_md_h md, void *address, size_t length,
                                           unsigned flags, uct_mem_h *memh_p) {
    // Your memory registration logic here
    // For simple providers, this might be a no-op
    *memh_p = (void*)0xdeadbeef; // Placeholder
    return UCS_OK;
}

static ucs_status_t uct_new_provider_mem_dereg(uct_md_h md, uct_mem_h memh) {
    // Your memory deregistration logic here
    return UCS_OK;
}

static void uct_new_provider_md_close(uct_md_h md) {
    // Cleanup MD resources
    ucs_free(md);
}

static uct_md_ops_t uct_new_provider_md_ops = {
    .close        = uct_new_provider_md_close,
    .query        = uct_new_provider_md_query,
    .mem_reg      = uct_new_provider_mem_reg,
    .mem_dereg    = uct_new_provider_mem_dereg,
    .mkey_pack    = ucs_empty_function_return_unsupported,
};

static ucs_status_t uct_new_provider_md_open(uct_component_t *component, const char *md_name,
                                           const uct_md_config_t *config, uct_md_h *md_p) {
    uct_new_provider_md_t *md = ucs_malloc(sizeof(*md), "new_provider_md");
    if (md == NULL) {
        return UCS_ERR_NO_MEMORY;
    }

    md->super.ops       = &uct_new_provider_md_ops;
    md->super.component = &uct_new_provider_component;

    *md_p = &md->super;
    return UCS_OK;
}
```

## 4. Implement the Interface (iface)

The Interface (`iface`) represents a communication context and manages transport-level resources.

### `base/new_provider_iface.h`

```c
#ifndef UCT_NEW_PROVIDER_IFACE_H
#define UCT_NEW_PROVIDER_IFACE_H

#include <uct/base/uct_iface.h>

// Define your iface structure
typedef struct uct_new_provider_iface {
    uct_base_iface_t super;
    // Add iface-specific resources, e.g., sockets, queues
} uct_new_provider_iface_t;

#endif
```

### `base/new_provider_iface.c`

This file implements the core transport logic, such as sending and receiving data.

```c
#include "new_provider_iface.h"
#include "new_provider_ep.h"

// Implement your transport's core functions here, e.g.:
// - uct_new_provider_iface_query
// - uct_new_provider_iface_flush
// - uct_new_provider_ep_put_short
// - uct_new_provider_ep_am_short

static uct_iface_ops_t uct_new_provider_iface_ops = {
    .ep_put_short      = ucs_empty_function_return_unsupported,
    .ep_get_short      = ucs_empty_function_return_unsupported,
    .ep_am_short       = ucs_empty_function_return_unsupported,
    .ep_flush          = uct_base_ep_flush,
    .iface_flush       = uct_base_iface_flush,
    .ep_destroy        = ucs_empty_function,
    .iface_close       = ucs_empty_function,
};
```

## 5. Implement the Endpoint (ep)

The Endpoint (`ep`) represents a connection to a remote peer.

### `base/new_provider_ep.h`

```c
#ifndef UCT_NEW_PROVIDER_EP_H
#define UCT_NEW_PROVIDER_EP_H

#include <uct/base/uct_ep.h>

// Define your endpoint structure
typedef struct uct_new_provider_ep {
    uct_base_ep_t super;
    // Add connection-specific data here
} uct_new_provider_ep_t;

#endif
```

### `base/new_provider_ep.c`

This file contains the implementation of the data transfer operations for an endpoint.

```c
#include "new_provider_ep.h"

// Example of a put_short implementation
ucs_status_t uct_new_provider_ep_put_short(uct_ep_h tl_ep, const void *buffer,
                                           unsigned length, uint64_t remote_addr,
                                           uct_rkey_t rkey) {
    // Your data transfer logic here
    return UCS_ERR_UNSUPPORTED;
}
```

## 6. Resource Discovery and Management

The entry point to your component is the `query_resources` function. Its job is to find all physical hardware resources your transport can use, allocate a list (as a dynamic array) to describe them, and return it to the UCX framework.

### The `query_resources` Implementation

Here is a more detailed example of how to implement this function, which is typically placed at the end of `new_provider_md.c`.

```c
static ucs_status_t uct_new_provider_query_resources(uct_component_t *component,
                                                   uct_resource_desc_t **resources_p,
                                                   unsigned *num_resources_p) {
    // Step 1: Discover the hardware
    int num_devices = 0;
    // Call your driver's API to get the number of devices.
    // For this example, we'll pretend it returns 2 devices.
    num_devices = 2;

    // For an IPC transport, you would add a check here:
    // if (num_devices < 2) {
    //     *num_resources_p = 0;
    //     *resources_p = NULL;
    //     return UCS_OK;
    // }

    if (num_devices == 0) {
        *num_resources_p = 0;
        *resources_p = NULL;
        return UCS_OK;
    }

    // Step 2: Allocate an array for the resource descriptions
    uct_resource_desc_t *resources = ucs_calloc(num_devices,
                                                sizeof(uct_resource_desc_t),
                                                "new_provider_resources");
    if (resources == NULL) {
        return UCS_ERR_NO_MEMORY;
    }

    // Step 3: Populate the array for each discovered device
    for (int i = 0; i < num_devices; i++) {
        // Set the name of the MD that can open this resource
        ucs_snprintf_safe(resources[i].md_name, sizeof(resources[i].md_name),
                          "%s", component->name);

        // Set a unique name for this specific device
        ucs_snprintf_safe(resources[i].dev_name, sizeof(resources[i].dev_name),
                          "provider%d", i);

        // Set the transport type (SELF for copy, IPC for ipc)
        resources[i].dev_type = UCT_DEVICE_TYPE_SELF;
    }

    // Step 4: Return the array and its size to the UCX framework
    *resources_p     = resources;
    *num_resources_p = num_devices;
    return UCS_OK;
}
```

## 7. Define the Component

Finally, you define the component structure itself, which points to your `query_resources` and `md_open` functions.

```c
// Typically at the end of new_provider_md.c

UCT_COMPONENT_DEFINE(uct_new_provider_component, "new_provider",
                     uct_new_provider_query_resources, // Your discovery function
                     uct_new_provider_md_open,         // Your MD open function
                     NULL,
                     UCS_CONFIG_EMPTY_GLOBAL_LIST,
                     uct_new_provider_md_config_table,
                     sizeof(uct_new_provider_md_config_t),
                     NULL, /* iface open */
                     0,    /* iface config table */
                     0,    /* iface config size */
                     NULL, /* iface attr */
                     0)
```

## 8. Add Testing

Create a new test file under `test/gtest/uct/` to validate your provider's functionality. You can use existing tests (e.g., `test_uct_self.cc`) as a starting point.

### `test/gtest/uct/test_new_provider.cc`

```cpp
#include <uct/test_uct_common.h>

class test_new_provider : public uct_test {
public:
    test_new_provider() {
        // Test setup
    }
};

UCS_TEST_P(test_new_provider, put_short) {
    // Your test logic here
}

UCT_INSTANTIATE_TEST_CASE(test_new_provider)
```

This guide provides a high-level skeleton. For a complete implementation, you will need to fill in the details specific to your hardware and transport protocol. Referencing existing providers like `tcp`, `sm`, or `gaudi` is highly recommended.

