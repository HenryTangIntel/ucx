# DMA-BUF Connection Between Gaudi and InfiniBand

## The Two DMA-BUF Connection Points

### 1. **Gaudi Side - DMA-BUF Export** 
**Location:** `/workspace/ucx/src/uct/gaudi/copy/gaudi_copy_md.c`

```c
// When Gaudi allocates memory, it can export it as DMA-BUF
static int uct_gaudi_export_dmabuf(uct_gaudi_md_t *gaudi_md, 
                                   uct_gaudi_mem_t *gaudi_memh)
{
    // Use Habana hlthunk API to export device memory as DMA-BUF
    dmabuf_fd = hlthunk_device_mapped_memory_export_dmabuf_fd(
        gaudi_md->hlthunk_fd,     // Gaudi device file descriptor
        gaudi_memh->dev_addr,     // Device memory address  
        gaudi_memh->size,         // Memory size
        0,                        // Offset
        (O_RDWR | O_CLOEXEC)     // Access flags
    );
    
    // Returns a DMA-BUF file descriptor that can be shared
    return dmabuf_fd;
}

// Called during memory allocation:
if (flags & UCT_MD_MEM_FLAG_FIXED) {
    int dmabuf_fd = uct_gaudi_export_dmabuf(gaudi_md, gaudi_memh);
    if (dmabuf_fd >= 0) {
        gaudi_memh->dmabuf_fd = dmabuf_fd;  // Store the DMA-BUF FD
        ucs_debug("Exported allocated memory as DMA-BUF fd %d", dmabuf_fd);
    }
}
```

### 2. **InfiniBand Side - DMA-BUF Import**
**Location:** `/workspace/ucx/src/uct/ib/base/ib_md.c`

```c
// When IB registers memory, it can use a DMA-BUF file descriptor
static ucs_status_t uct_ib_mem_reg_internal(uct_ib_md_t *md, void *address,
                                           size_t length, uint64_t access_flags,
                                           struct ibv_dm *dm,
                                           const uct_md_mem_reg_params_t *params,
                                           struct ibv_mr **mr_p)
{
    // Extract DMA-BUF parameters from registration request
    int dmabuf_fd = UCS_PARAM_VALUE(UCT_MD_MEM_REG_FIELD, params, dmabuf_fd,
                                    DMABUF_FD, UCT_DMABUF_FD_INVALID);
    size_t dmabuf_offset = UCS_PARAM_VALUE(UCT_MD_MEM_REG_FIELD, params, dmabuf_offset,
                                          DMABUF_OFFSET, 0);

    // If DMA-BUF FD is provided, use IB DMA-BUF registration
    if (dmabuf_fd != UCT_DMABUF_FD_INVALID) {
#if HAVE_DECL_IBV_REG_DMABUF_MR
        // Register using the DMA-BUF file descriptor from Gaudi
        mr = ibv_reg_dmabuf_mr(
            md->pd,           // IB protection domain
            dmabuf_offset,    // Offset within DMA-BUF
            length,           // Memory size
            (uintptr_t)address, // Virtual address hint
            dmabuf_fd,        // The DMA-BUF FD from Gaudi!
            access_flags      // RDMA access permissions
        );
#endif
    }
    
    return UCS_OK;
}
```

## The Complete Connection Flow

```
┌─────────────────┐    DMA-BUF FD    ┌─────────────────┐
│   GAUDI SIDE    │ ────────────────▶│     IB SIDE     │
└─────────────────┘                  └─────────────────┘

1. Gaudi Memory Allocation:
   ┌────────────────────────────────────────────────┐
   │ uct_gaudi_copy_md_mem_alloc()                  │
   │ ├─ hlthunk_device_memory_alloc()              │ 
   │ ├─ Maps to device virtual address             │
   │ └─ uct_gaudi_export_dmabuf()                  │
   │    └─ hlthunk_device_mapped_memory_export_    │
   │       dmabuf_fd() ──────────┐                 │
   └────────────────────────────│─────────────────┘
                                │
                                ▼
                        ┌──────────────┐
                        │  DMA-BUF FD  │ ◄─── Shared file descriptor
                        │   (fd=42)    │      representing Gaudi memory
                        └──────────────┘
                                │
                                ▼
   ┌────────────────────────────│─────────────────┐
   │ 2. IB Memory Registration: │                 │
   │ uct_ib_md_mem_reg()        │                 │
   │ ├─ Receives DMA-BUF FD ────┘                 │
   │ └─ ibv_reg_dmabuf_mr()                       │
   │    ├─ pd = IB protection domain              │
   │    ├─ dmabuf_fd = 42 (from Gaudi)           │
   │    ├─ address = Gaudi virtual address        │
   │    ├─ length = memory size                   │
   │    └─ access_flags = RDMA permissions        │
   │                                              │
   │ Result: IB can now do RDMA on Gaudi memory! │
   └──────────────────────────────────────────────┘
```

## Key Integration Points in Your Test

In your integration test (`test_gaudi_ib_integration.c`), the connection happens here:

```c
// 1. Gaudi allocates memory with DMA-BUF export capability
uct_mem_alloc_params_t alloc_params;
alloc_params.flags = UCT_MD_MEM_ACCESS_LOCAL_READ | UCT_MD_MEM_ACCESS_LOCAL_WRITE;
alloc_params.mds.mds = &ctx->gaudi_md;
alloc_params.mem_type = UCS_MEMORY_TYPE_GAUDI;

ucs_status_t status = uct_mem_alloc(4096, alloc_methods, 2, &alloc_params, &allocated_mem);
// ↑ This allocates Gaudi memory and potentially exports DMA-BUF

// 2. IB registers the Gaudi memory using cross-registration
if (ctx->ib_found && ctx->ib_supports_gaudi) {
    uct_mem_h ib_memh;
    status = uct_md_mem_reg(ctx->ib_md, allocated_mem.address, 4096, 
                           UCT_MD_MEM_ACCESS_ALL, &ib_memh);
    // ↑ This internally uses the DMA-BUF FD to register with IB
}
```

## The Magic: Zero-Copy RDMA

Once this connection is established:

1. **Gaudi** writes data to its device memory
2. **InfiniBand** can directly read that memory via RDMA (no CPU copy!)
3. **Remote nodes** can access Gaudi memory over the network
4. **GPUDirect RDMA** enables direct device-to-device transfers

## Hardware Requirements

- **Gaudi**: Must support DMA-BUF export (`hlthunk_device_mapped_memory_export_dmabuf_fd`)
- **InfiniBand**: Must support DMA-BUF import (`ibv_reg_dmabuf_mr`) 
- **Kernel**: DMA-BUF infrastructure enabled
- **Drivers**: GPUDirect RDMA support in both Habana and Mellanox drivers

The DMA-BUF file descriptor is the **bridge** that allows these two completely different memory domains to share memory without copying!
