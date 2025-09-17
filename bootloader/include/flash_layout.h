#pragma once

#include "hardware/flash.h"


#define BOOTLOADER_SIZE             (32 * 1024) /* 32kB seems fine */

#define _SECTOR_TO_ADDR(_s)         (XIP_BASE + _s)
#define _ADDR_TO_SECTOR(_a)         (_a - XIP_BASE)
#define _SECTOR_TO_PAGE(_s)         (_s / FLASH_PAGE_SIZE)

#define PERSIST_CONFIG_SECTOR       BOOTLOADER_SIZE
#define PERSIST_CONFIG_SECTOR_ADDR  _SECTOR_TO_ADDR(PERSIST_CONFIG_SECTOR)
#define PERSIST_CONFIG_SIZE         (FLASH_PAGE_SIZE * 8) /* = FLASH_SECTOR_SIZE / 2 */
#define PERSIST_RAW_DATA            ((uint8_t*)PERSIST_CONFIG_SECTOR_ADDR)

#define FW_MAX_SIZE                 (800 * 1024)
#define FW_SECTOR                   (PERSIST_CONFIG_SECTOR + 2 * FLASH_SECTOR_SIZE)
#define FW_ADDR                     _SECTOR_TO_ADDR(FW_SECTOR)

_Static_assert(FW_ADDR + FW_MAX_SIZE < XIP_BASE + PICO_FLASH_SIZE_BYTES, "Firmware address overrun.");
