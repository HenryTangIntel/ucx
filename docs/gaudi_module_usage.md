# Using the UCX Gaudi Module

This document provides instructions for using the UCX Gaudi module for high-performance communication with Intel Habanalabs Gaudi AI accelerators.

## Prerequisites

Before using the UCX Gaudi module, ensure you have the following:

1. **Hardware**: Intel Habanalabs Gaudi AI accelerator card(s)
2. **Driver**: Habanalabs kernel driver (`habanalabs`)
3. **Runtime Library**: Habana Labs runtime library (`libhl-thunk.so`)
4. **UCX**: Built with Gaudi support

To verify your setup, run the hardware check script:

```bash
./check_gaudi_hardware.sh
```

## Building UCX with Gaudi Support

To build UCX with Gaudi support:

```bash
# Configure with Gaudi support
./configure --prefix=/path/to/install --enable-debug --enable-params-check

# Build and install
make -j$(nproc)
make install
```

## Programming with the Gaudi Module

### 1. Initializing UCT

First, initialize the UCT framework:

```c
#include <uct/api/uct.h>

ucs_status_t status;
status = uct_init();
if (status != UCS_OK) {
    // Handle error
}
```

### 2. Finding the Gaudi Component

Query the available UCT components and find the Gaudi component:

```c
uct_component_h *components;
unsigned num_components;
uct_component_h gaudi_component = NULL;

status = uct_query_components(&components, &num_components);
if (status != UCS_OK) {
    // Handle error
}

for (unsigned i = 0; i < num_components; i++) {
    if (strcmp(components[i]->name, "gaudi") == 0) {
        gaudi_component = components[i];
        break;
    }
}

if (gaudi_component == NULL) {
    // Gaudi component not available
}
```

### 3. Opening a Gaudi Memory Domain

Open a memory domain for the Gaudi component:

```c
uct_md_config_t *md_config;
uct_md_h md;

status = uct_md_config_read(gaudi_component, NULL, NULL, &md_config);
if (status != UCS_OK) {
    // Handle error
}

status = uct_md_open(gaudi_component, "gaudi", md_config, &md);
uct_config_release(md_config);
if (status != UCS_OK) {
    // Handle error
}
```

### 4. Memory Operations

#### Registering Memory

To register memory for use with Gaudi:

```c
void *buffer = malloc(size);
uct_mem_h memh;

status = uct_md_mem_reg(md, buffer, size, UCT_MD_MEM_ACCESS_ALL, &memh);
if (status != UCS_OK) {
    // Handle error
}

// Use the memory handle (memh) with communication operations

// When done, deregister the memory
status = uct_md_mem_dereg(md, memh);
if (status != UCS_OK) {
    // Handle error
}
```

#### Allocating Memory

To allocate memory through the Gaudi memory domain:

```c
uct_allocated_memory_t mem;

status = uct_md_mem_alloc(md, size, &mem, UCT_MD_MEM_ACCESS_ALL, "allocation_name");
if (status != UCS_OK) {
    // Handle error
}

// Use the allocated memory at mem.address

// When done, free the memory
status = uct_md_mem_free(&mem);
if (status != UCS_OK) {
    // Handle error
}
```

### 5. Transport Layer Resources

Query and use transport layer resources:

```c
uct_tl_resource_desc_t *resources;
unsigned num_resources;

status = uct_md_query_tl_resources(md, &resources, &num_resources);
if (status != UCS_OK) {
    // Handle error
}

// Use the transport layer resources

uct_release_tl_resource_list(resources);
```

### 6. Cleanup

Clean up all resources when done:

```c
uct_md_close(md);
uct_release_component_list(components);
uct_cleanup();
```

## Example Program

Here's a complete example that demonstrates basic usage of the Gaudi module:

```c
#include <ucs/type/status.h>
#include <uct/api/uct.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main() {
    ucs_status_t status;
    uct_component_h *components;
    unsigned num_components;
    uct_component_h gaudi_component = NULL;
    uct_md_config_t *md_config;
    uct_md_h md;
    void *buffer;
    uct_mem_h memh;
    size_t buffer_size = 1024;
    
    // Initialize UCT
    status = uct_init();
    if (status != UCS_OK) {
        fprintf(stderr, "Failed to initialize UCT: %s\n", ucs_status_string(status));
        return -1;
    }
    
    // Find Gaudi component
    status = uct_query_components(&components, &num_components);
    if (status != UCS_OK) {
        fprintf(stderr, "Failed to query components: %s\n", ucs_status_string(status));
        uct_cleanup();
        return -1;
    }
    
    for (unsigned i = 0; i < num_components; i++) {
        if (strcmp(components[i]->name, "gaudi") == 0) {
            gaudi_component = components[i];
            break;
        }
    }
    
    if (gaudi_component == NULL) {
        fprintf(stderr, "Gaudi component not found\n");
        uct_release_component_list(components);
        uct_cleanup();
        return -1;
    }
    
    // Open Gaudi memory domain
    status = uct_md_config_read(gaudi_component, NULL, NULL, &md_config);
    if (status != UCS_OK) {
        fprintf(stderr, "Failed to read MD config: %s\n", ucs_status_string(status));
        uct_release_component_list(components);
        uct_cleanup();
        return -1;
    }
    
    status = uct_md_open(gaudi_component, "gaudi", md_config, &md);
    uct_config_release(md_config);
    if (status != UCS_OK) {
        fprintf(stderr, "Failed to open MD: %s\n", ucs_status_string(status));
        uct_release_component_list(components);
        uct_cleanup();
        return -1;
    }
    
    // Allocate and register memory
    buffer = aligned_alloc(4096, buffer_size);
    if (buffer == NULL) {
        fprintf(stderr, "Failed to allocate memory\n");
        uct_md_close(md);
        uct_release_component_list(components);
        uct_cleanup();
        return -1;
    }
    
    memset(buffer, 0, buffer_size);
    
    status = uct_md_mem_reg(md, buffer, buffer_size, UCT_MD_MEM_ACCESS_ALL, &memh);
    if (status != UCS_OK) {
        fprintf(stderr, "Failed to register memory: %s\n", ucs_status_string(status));
        free(buffer);
        uct_md_close(md);
        uct_release_component_list(components);
        uct_cleanup();
        return -1;
    }
    
    printf("Memory successfully registered with Gaudi\n");
    
    // Deregister memory
    status = uct_md_mem_dereg(md, memh);
    if (status != UCS_OK) {
        fprintf(stderr, "Failed to deregister memory: %s\n", ucs_status_string(status));
    }
    
    // Cleanup
    free(buffer);
    uct_md_close(md);
    uct_release_component_list(components);
    uct_cleanup();
    
    printf("Successful test with Gaudi module\n");
    return 0;
}
```

## Troubleshooting

### Common Issues

1. **Gaudi Component Not Found**
   - Ensure UCX was built with Gaudi support
   - Check that the module is registered properly

2. **Memory Registration Failures**
   - Verify that the Habanalabs driver is loaded (`lsmod | grep habanalabs`)
   - Ensure the libhl-thunk.so library is in the library path

3. **Communication Errors**
   - Ensure Gaudi hardware is properly configured 
   - Check driver status with `dmesg | grep habanalabs`

For more detailed diagnostics, use the hardware check script:

```bash
./check_gaudi_hardware.sh
```

## Performance Tips

1. Use aligned memory for better performance (aligned to 4KB or more)
2. Prefer memory registration over allocation for externally managed buffers
3. Reuse memory handles when possible to avoid registration overhead
4. Use batch operations for multiple small transfers

## Further Reading

- UCT API Documentation
- Habana Labs Developer Documentation
- Intel Gaudi Documentation
