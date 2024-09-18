/*
 * Copyright 2023 NXP
 * All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */
 
#ifndef __BL_CFG_H__
#define __BL_CFG_H__

#include "fsl_debug_console.h"

#define BL_DEBUG		1
#if ( BL_DEBUG == 1 )
#include <stdio.h>
#define BL_TRACE	PRINTF
#else
#define BL_TRACE(...)
#endif


#define CAN_BIT_RATE        (500*1000)
#define CAN_BIT_RATE_FD     (2*1000*1000)
#define CAN_TX_ID           (0x08)
#define CAN_RX_ID           (0x09)

/* Base address of user application */
#ifndef FLASH_BASE
#define FLASH_BASE                  (0x00000000)
#endif

#define BL_SIZE                     (32*1024)
#define DEFAULT_APPLICATION_START   (FLASH_BASE + BL_SIZE)
#define DEFAULT_BL_TIMEOUT          (1000)


#define TARGET_FLASH_SIZE           (1024*1024)
#define TARGET_RAM_BASE             (0x20000000)
#define TARGET_RAM_SIZE             (128*1024)


#endif
