%ifndef JOSHUA_BOOT_INC
%define JOSHUA_BOOT_INC

%define ST_CON_IN             48
%define ST_CON_OUT            64
%define ST_RUNTIME           88
%define ST_BOOT              96
%define ST_TABLE_COUNT       104
%define ST_TABLES            112
%define BS_GET_MEMORY_MAP    56
%define BS_HANDLE_PROTOCOL   152
%define BS_EXIT_BOOT         232
%define BS_STALL             248
%define BS_SET_WATCHDOG      256
%define BS_LOCATE_PROTOCOL   320
%define RT_RESET_SYSTEM      104
%define CON_READ_KEY         8
%define CON_OUTPUT           8
%define CON_SET_ATTRIBUTE    40
%define CON_CLEAR            48
%define GOP_MODE             24
%define MODE_INFO            8
%define MODE_FB_BASE         24
%define MODE_FB_SIZE         32
%define INFO_WIDTH           4
%define INFO_HEIGHT          8
%define INFO_FORMAT          12
%define INFO_STRIDE          32
%define EFI_INVALID_PARAMETER 0x8000000000000002
%define EFI_NOT_READY        0x8000000000000006
%define MAP_CAPACITY         0x20000

struc BOOT_INFO
 .magic:             resq 1
 .version:           resd 1
 .size:              resd 1
 .framebuffer:       resq 1
 .framebuffer_size:  resq 1
 .width:             resd 1
 .height:            resd 1
 .stride:            resd 1
 .pixel_format:      resd 1
 .memory_map:        resq 1
 .memory_map_size:   resq 1
 .descriptor_size:   resq 1
 .descriptor_version: resd 1
 .reserved:          resd 1
 .acpi_rsdp:         resq 1
endstruc

%if BOOT_INFO_size != 88
 %error Invalid boot-info size
%endif
; PE layout constants are shared by the handwritten headers and image checks.
%define PE_TEXT_SIZE 0x4000
%define PE_DATA_RVA 0x5000
%define PE_BSS_RVA 0x6000
%define PE_BSS_SIZE 0xba000
%define PE_RELOC_RVA (PE_BSS_RVA + PE_BSS_SIZE)
%define PE_IMAGE_SIZE (PE_RELOC_RVA + 0x1000)
%define CANDIDATE_CAPACITY 8192
%define PAGING_TABLE_CAPACITY 16384
%define PAGING_FLAG_NXE 1
%define PAGING_FLAG_1G 2

struc PAGING_INVENTORY
 .entries: resq 1
 .count: resq 1
 .capacity: resq 1
 .memory_map: resq 1
 .map_size: resq 1
 .descriptor_size: resq 1
 .physical_bits: resq 1
 .flags: resq 1
 .root: resq 1
endstruc

; v0 remains frozen at 88 bytes. v1 appends loaded-image bounds.
struc BOOT_INFO_V1
 .v0: resb BOOT_INFO_size
 .image_base: resq 1
 .image_size: resq 1
endstruc
%if BOOT_INFO_V1_size != 104
 %error Invalid boot-info v1 size
%endif

struc RANGE_SET
 .entries: resq 1
 .count: resq 1
 .capacity: resq 1
endstruc

%define FRAME_ALLOCATION_CAPACITY 4096
%define FRAME_ALLOCATOR_MAGIC 0x314d41524653484a
struc FRAME_ALLOCATOR
 .ready: resq 1
 .ranges: resq 1
 .records: resq 1
 .count: resq 1
 .capacity: resq 1
 .total_pages: resq 1
 .free_pages: resq 1
endstruc

%define FRAME_OWNER_PAGING 0x50414745
%define PTE_NX 0x8000000000000000
struc OWNED_PAGING
 .allocator: resq 1
 .root: resq 1
 .table_count: resq 1
 .failed: resq 1
 .physical_limit: resq 1
 .pool_next: resq 1
 .pool_end: resq 1
 .old_root: resq 1
 .image_base: resq 1
 .image_end: resq 1
 .guard: resq 1
 .framebuffer_start: resq 1
 .framebuffer_end: resq 1
 .pat: resq 1
 .ready: resq 1
endstruc

%define BOOT_MAGIC 0x004F464E4948534A

%endif
