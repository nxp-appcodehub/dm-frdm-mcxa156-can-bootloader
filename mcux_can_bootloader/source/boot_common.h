/*
 * Copyright 2024 NXP
 * All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef __BOOT_COMMON_H__
#define __BOOT_COMMON_H__

#include "board.h"
#include "bl_cfg.h"

void flash_init(void);
int memory_get_sector_size(void);
int memory_erase(uint32_t start_addr, uint32_t len);
int memory_write(uint32_t start_addr, uint8_t *buf, uint32_t len);
int memory_read(uint32_t addr, uint8_t *buf, uint32_t len);

void jump_to_app(void);

#endif
