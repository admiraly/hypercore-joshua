%ifndef JOSHUA_BOOT_INC
%define JOSHUA_BOOT_INC

%define ST_CON_IN             48
%define ST_CON_OUT            64
%define ST_RUNTIME           88
%define ST_BOOT              96
%define ST_TABLE_COUNT       104
%define ST_TABLES            112
%define BS_GET_MEMORY_MAP    56
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
%define BOOT_MAGIC 0x004F464E4948534A

%endif
