/*
 * Copyright (C) 2007 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#pragma once

#include <stdint.h>

#define BOOT_MAGIC "ANDROID!"
#define BOOT_MAGIC_SIZE 8
#define BOOT_NAME_SIZE 16
#define BOOT_ARGS_SIZE 512
#define BOOT_EXTRA_ARGS_SIZE 1024

#define BOOT_HEADER_VERSION_ZERO 0

// The bootloader expects the structure of boot_img_hdr with header
// version 0 to be as follows:
struct boot_img_hdr_v0 {
    // Must be BOOT_MAGIC.
    uint8_t magic[BOOT_MAGIC_SIZE];

    uint32_t kernel_size; /* size in bytes */
    uint32_t kernel_addr; /* physical load addr */

    uint32_t ramdisk_size; /* size in bytes */
    uint32_t ramdisk_addr; /* physical load addr */

    uint32_t second_size; /* size in bytes */
    uint32_t second_addr; /* physical load addr */

    uint32_t tags_addr; /* physical addr for kernel tags */
    uint32_t page_size; /* flash page size we assume */

    // Version of the boot image header.
    // Alternately this is used as dt_size on some hardware.
    union {
        uint32_t header_version;
        uint32_t dt_size; /* device tree in bytes */
    };

    // Operating system version and security patch level.
    // For version "A.B.C" and patch level "Y-M-D":
    //   (7 bits for each of A, B, C; 7 bits for (Y-2000), 4 bits for M)
    //   os_version = A[31:25] B[24:18] C[17:11] (Y-2000)[10:4] M[3:0]
    uint32_t os_version;

    uint8_t name[BOOT_NAME_SIZE]; /* asciiz product name */

    uint8_t cmdline[BOOT_ARGS_SIZE];

    uint32_t id[8]; /* timestamp / checksum / sha1 / etc */

    // Supplemental command line data; kept here to maintain
    // binary compatibility with older versions of mkbootimg.
    uint8_t extra_cmdline[BOOT_EXTRA_ARGS_SIZE];
} __attribute__((packed));

/*
 * It is expected that callers would explicitly specify which version of the
 * boot image header they need to use.
 */
typedef struct boot_img_hdr_v0 boot_img_hdr;
typedef struct boot_img_hdr_v0 boot_img_hdr_v0;
typedef struct boot_img_hdr_v1 boot_img_hdr_v1;
typedef struct boot_img_hdr_v2 boot_img_hdr_v2;
typedef struct boot_img_hdr_v3 boot_img_hdr_v3;

/* When a boot header is of version 0, the structure of boot image is as
 * follows:
 *
 * +-----------------+
 * | boot header     | 1 page
 * +-----------------+
 * | kernel          | n pages
 * +-----------------+
 * | ramdisk         | m pages
 * +-----------------+
 * | second stage    | o pages
 * +-----------------+
 *
 * and on some hardware:
 * +-----------------+
 * | device tree     | p pages
 * +-----------------+
 *
 * n = (kernel_size + page_size - 1) / page_size
 * m = (ramdisk_size + page_size - 1) / page_size
 * o = (second_size + page_size - 1) / page_size
 *
 * and on some hardware:
 * p = (dt_size + page_size -1) / page_size
 *
 * 0. all entities are page_size aligned in flash
 * 1. kernel and ramdisk are required (size != 0)
 * 2. second is optional (second_size == 0 -> no second)
 * 3. load each element (kernel, ramdisk, second) at
 *    the specified physical address (kernel_addr, etc)
 * 4. prepare tags at tag_addr.  kernel_args[] is
 *    appended to the kernel commandline in the tags.
 * 5. r0 = 0, r1 = MACHINE_TYPE, r2 = tags_addr
 * 6. if second_size != 0: jump to second_addr
 *    else: jump to kernel_addr
 */

#define BOOT_HEADER_VERSION_ONE 1

struct boot_img_hdr_v1 {
    // Must be BOOT_MAGIC.
    uint8_t magic[BOOT_MAGIC_SIZE];

    uint32_t kernel_size; /* size in bytes */
    uint32_t kernel_addr; /* physical load addr */

    uint32_t ramdisk_size; /* size in bytes */
    uint32_t ramdisk_addr; /* physical load addr */

    uint32_t second_size; /* size in bytes */
    uint32_t second_addr; /* physical load addr */

    uint32_t tags_addr; /* physical addr for kernel tags */
    uint32_t page_size; /* flash page size we assume */

    // Version of the boot image header.
    // Alternately this is used as dt_size on some hardware.
    union {
        uint32_t header_version;
        uint32_t dt_size; /* device tree in bytes */
    };

    // Operating system version and security patch level.
    // For version "A.B.C" and patch level "Y-M-D":
    //   (7 bits for each of A, B, C; 7 bits for (Y-2000), 4 bits for M)
    //   os_version = A[31:25] B[24:18] C[17:11] (Y-2000)[10:4] M[3:0]
    uint32_t os_version;

    uint8_t name[BOOT_NAME_SIZE]; /* asciiz product name */

    uint8_t cmdline[BOOT_ARGS_SIZE];

    uint32_t id[8]; /* timestamp / checksum / sha1 / etc */

    // Supplemental command line data; kept here to maintain
    // binary compatibility with older versions of mkbootimg.
    uint8_t extra_cmdline[BOOT_EXTRA_ARGS_SIZE];

    uint32_t recovery_dtbo_size;   /* size in bytes for recovery DTBO/ACPIO image */
    uint64_t recovery_dtbo_offset; /* offset to recovery dtbo/acpio in boot image */
    uint32_t header_size;
} __attribute__((packed));

/* When the boot image header has a version of 1, the structure of the boot
 * image is as follows:
 *
 * +---------------------+
 * | boot header         | 1 page
 * +---------------------+
 * | kernel              | n pages
 * +---------------------+
 * | ramdisk             | m pages
 * +---------------------+
 * | second stage        | o pages
 * +---------------------+
 * | recovery dtbo/acpio | p pages
 * +---------------------+

 * n = (kernel_size + page_size - 1) / page_size
 * m = (ramdisk_size + page_size - 1) / page_size
 * o = (second_size + page_size - 1) / page_size
 * p = (recovery_dtbo_size + page_size - 1) / page_size
 *
 * 0. all entities are page_size aligned in flash
 * 1. kernel and ramdisk are required (size != 0)
 * 2. recovery_dtbo/recovery_acpio is required for recovery.img in non-A/B
 *    devices(recovery_dtbo_size != 0)
 * 3. second is optional (second_size == 0 -> no second)
 * 4. load each element (kernel, ramdisk, second) at
 *    the specified physical address (kernel_addr, etc)
 * 5. If booting to recovery mode in a non-A/B device, extract recovery
 *    dtbo/acpio and apply the correct set of overlays on the base device tree
 *    depending on the hardware/product revision.
 * 6. prepare tags at tag_addr.  kernel_args[] is
 *    appended to the kernel commandline in the tags.
 * 7. r0 = 0, r1 = MACHINE_TYPE, r2 = tags_addr
 * 8. if second_size != 0: jump to second_addr
 *    else: jump to kernel_addr
 */

#define BOOT_HEADER_VERSION_TWO 2

struct boot_img_hdr_v2 {
    // Must be BOOT_MAGIC.
    uint8_t magic[BOOT_MAGIC_SIZE];

    uint32_t kernel_size; /* size in bytes */
    uint32_t kernel_addr; /* physical load addr */

    uint32_t ramdisk_size; /* size in bytes */
    uint32_t ramdisk_addr; /* physical load addr */

    uint32_t second_size; /* size in bytes */
    uint32_t second_addr; /* physical load addr */

    uint32_t tags_addr; /* physical addr for kernel tags */
    uint32_t page_size; /* flash page size we assume */

    // Version of the boot image header.
    // Alternately this is used as dt_size on some hardware.
    union {
        uint32_t header_version;
        uint32_t dt_size; /* device tree in bytes */
    };

    // Operating system version and security patch level.
    // For version "A.B.C" and patch level "Y-M-D":
    //   (7 bits for each of A, B, C; 7 bits for (Y-2000), 4 bits for M)
    //   os_version = A[31:25] B[24:18] C[17:11] (Y-2000)[10:4] M[3:0]
    uint32_t os_version;

    uint8_t name[BOOT_NAME_SIZE]; /* asciiz product name */

    uint8_t cmdline[BOOT_ARGS_SIZE];

    uint32_t id[8]; /* timestamp / checksum / sha1 / etc */

    // Supplemental command line data; kept here to maintain
    // binary compatibility with older versions of mkbootimg.
    uint8_t extra_cmdline[BOOT_EXTRA_ARGS_SIZE];

    uint32_t recovery_dtbo_size;   /* size in bytes for recovery DTBO/ACPIO image */
    uint64_t recovery_dtbo_offset; /* offset to recovery dtbo/acpio in boot image */
    uint32_t header_size;

    uint32_t dtb_size; /* size in bytes for DTB image */
    uint64_t dtb_addr; /* physical load address for DTB image */
} __attribute__((packed));

/* When the boot image header has a version of 2, the structure of the boot
 * image is as follows:
 *
 * +---------------------+
 * | boot header         | 1 page
 * +---------------------+
 * | kernel              | n pages
 * +---------------------+
 * | ramdisk             | m pages
 * +---------------------+
 * | second stage        | o pages
 * +---------------------+
 * | recovery dtbo/acpio | p pages
 * +---------------------+
 * | dtb                 | q pages
 * +---------------------+

 * n = (kernel_size + page_size - 1) / page_size
 * m = (ramdisk_size + page_size - 1) / page_size
 * o = (second_size + page_size - 1) / page_size
 * p = (recovery_dtbo_size + page_size - 1) / page_size
 * q = (dtb_size + page_size - 1) / page_size
 *
 * 0. all entities are page_size aligned in flash
 * 1. kernel, ramdisk and DTB are required (size != 0)
 * 2. recovery_dtbo/recovery_acpio is required for recovery.img in non-A/B
 *    devices(recovery_dtbo_size != 0)
 * 3. second is optional (second_size == 0 -> no second)
 * 4. load each element (kernel, ramdisk, second, dtb) at
 *    the specified physical address (kernel_addr, etc)
 * 5. If booting to recovery mode in a non-A/B device, extract recovery
 *    dtbo/acpio and apply the correct set of overlays on the base device tree
 *    depending on the hardware/product revision.
 * 6. prepare tags at tag_addr.  kernel_args[] is
 *    appended to the kernel commandline in the tags.
 * 7. r0 = 0, r1 = MACHINE_TYPE, r2 = tags_addr
 * 8. if second_size != 0: jump to second_addr
 *    else: jump to kernel_addr
 */

struct boot_img_hdr_v3
{
#define BOOT_MAGIC_SIZE 8
    uint8_t magic[BOOT_MAGIC_SIZE];

    uint32_t kernel_size;    /* size in bytes */
    uint32_t ramdisk_size;   /* size in bytes */

    uint32_t os_version;

    uint32_t header_size;    /* size of boot image header in bytes */
    uint32_t reserved[4];
    uint32_t header_version; /* offset remains constant for version check */

#define BOOT_ARGS_SIZE 512
#define BOOT_EXTRA_ARGS_SIZE 1024
    uint8_t cmdline[BOOT_ARGS_SIZE + BOOT_EXTRA_ARGS_SIZE];
};
