/*
 * Copyright 2023 NXP
 * All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */
 
#ifndef __APP_CAN_H__
#define __APP_CAN_H__

#include "fsl_debug_console.h"
#include "fsl_flexcan.h"


void app_can_init(uint32_t bitrate, uint32_t bitrateFD, uint8_t rx_id);
int app_can_send(uint32_t can_id, uint8_t* buf, uint8_t len);
void app_can_dump_msg(uint32_t id, uint8_t *buf, uint8_t len);
void app_can_set_rx_indicate(void (*indicate)(uint32_t id, uint8_t *buf, uint8_t len));

#endif
