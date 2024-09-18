/*
 * Copyright 2023 NXP
 * All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "fsl_debug_console.h"
#include "fsl_lpuart.h"
#include "boot_common.h"
#include "app_can.h"
#include "mcuboot.h"
#include "mq.h"


#define MSG_CAN_RX              (1)
#define MSG_CAN_TX              (2)
#define MSG_USB_CDC_OUT         (3)
#define MSG_USB_CDC_IN          (4)
#define CANFD_MAX_FRAME_SIZE    (64)

typedef struct
{
    uint8_t         en_jump;                    /* timeout jump flag */
    mcuboot_t       mcuboot;
}blsvr_t;

static blsvr_t blsvr = {0};

void BOARD_InitHardware(void);
void can_rx_indicate(uint32_t id, uint8_t *buf, uint8_t len);

static void mcuboot_complete(void) {}

static void mcuboot_reset(void)
{
    SDK_DelayAtLeastUs(100*1000, CLOCK_GetCoreSysClkFreq());
    NVIC_SystemReset();
}

int uart_send(uint8_t *buf, uint32_t len)
{
    /* send using serial */
    LPUART_WriteBlocking(LPUART0, buf, len);

    /* send using CAN */
    uint8_t tx_buf[CANFD_MAX_FRAME_SIZE];
    tx_buf[7] = len;
    memcpy(&tx_buf[8], buf, len);

    app_can_send(CAN_TX_ID, tx_buf, CANFD_MAX_FRAME_SIZE);
    
    return 0;
}

int main(void)
{
    int i;
    uint8_t c;
    
    BOARD_InitHardware();

    gpio_pin_config_t sw_config = {  kGPIO_DigitalInput, 0,  };
    GPIO_PinInit(GPIO1, 7, &sw_config);

    BL_TRACE("MCXA CAN bootloader\r\n");
    BL_TRACE("APP ADDR:0x%X\r\n", FLASH_BASE + BL_SIZE);
//    BL_TRACE("CoreSysClk:%dHz\r\n", CLOCK_GetCoreSysClkFreq());
//    BL_TRACE("FlexcanClk:%dHz\r\n", CLOCK_GetFlexcanClkFreq());

    if(GPIO_PinRead(GPIO1, 7) == 0)
    {
        BL_TRACE("SW Pressed, Enter bootloader mode\r\n");
        BL_TRACE("CAN BAUD,FD:%d,%d\r\n", CAN_BIT_RATE, CAN_BIT_RATE_FD);
        BL_TRACE("CAN TX_ID:%d\r\n", CAN_TX_ID);
        BL_TRACE("CAN RX_ID:%d\r\n", CAN_RX_ID);
        BL_TRACE("Waitting for connection...\r\n");
    }
    else
    {
        /* enable timeout timer */
        SysTick_Config((CLOCK_GetCoreSysClkFreq() / 1000U)*10);
    }

    mq_init();
    flash_init();
    app_can_init(CAN_BIT_RATE, CAN_BIT_RATE_FD, CAN_RX_ID);
    app_can_set_rx_indicate(can_rx_indicate);

    blsvr.mcuboot.op_send = uart_send;
    blsvr.mcuboot.op_reset = mcuboot_reset;
    blsvr.mcuboot.op_jump = jump_to_app;
    blsvr.mcuboot.op_complete = mcuboot_complete;
    blsvr.mcuboot.op_mem_erase = memory_erase;
    blsvr.mcuboot.op_mem_write = memory_write;
    blsvr.mcuboot.op_mem_read = memory_read;
    blsvr.mcuboot.cfg_flash_start = DEFAULT_APPLICATION_START;
    blsvr.mcuboot.cfg_flash_size = TARGET_FLASH_SIZE;
    blsvr.mcuboot.cfg_flash_sector_size = memory_get_sector_size();
    blsvr.mcuboot.cfg_ram_start = TARGET_RAM_BASE;
    blsvr.mcuboot.cfg_ram_size = TARGET_RAM_SIZE;

    blsvr.mcuboot.cfg_device_id = 0x12345678;
    
    mcuboot_init(&blsvr.mcuboot);

    msg_t *rx_msg;
    
    while (1)
    {
        if(LPUART_GetStatusFlags(LPUART0) & kLPUART_RxDataRegFullFlag)
        {
            c = LPUART_ReadByte(LPUART0);
            mcuboot_recv(&blsvr.mcuboot, &c, 1);
        }

        if(mq_exist())
        {
            rx_msg = mq_pop();
            if(rx_msg->cmd == MSG_CAN_RX)
            {
                static uint32_t rx_can_count = 0;
                
                uint8_t len;
                len = rx_msg->buf[7];
                rx_can_count += len;
                
                //PRINTF("can_rx:id:0x%X, size:%d, total:%d\r\n", rx_msg->can_id, len, rx_can_count);
                
                mcuboot_recv(&blsvr.mcuboot, &rx_msg->buf[8], len);
            }
        }
        
        if(blsvr.en_jump == 1)
        {
            BL_TRACE("TIMEOUT!, JUMP!\r\n");
            jump_to_app();
            blsvr.en_jump = 0;
        }

        mcuboot_proc(&blsvr.mcuboot);
    }
}


void SysTick_Handler(void)
{
    static int timeout;
    if(timeout > DEFAULT_BL_TIMEOUT / 10)
    {
        if(!blsvr.mcuboot.actived)
        {
            blsvr.en_jump = 1;
        }
        SysTick->CTRL &= ~SysTick_CTRL_TICKINT_Msk;
    }
    timeout++;
}


void can_rx_indicate(uint32_t id, uint8_t *buf, uint8_t len)
{
    msg_t msg;

    msg.cmd = MSG_CAN_RX;
    msg.can_id = id;
    msg.len = len;
    memcpy(msg.buf, buf, len);
    
    mq_push(msg);
}


void HardFault_Handler(void)
{
    BL_TRACE("%s\r\n", __FUNCTION__);
    while(1)
    {
         NVIC_SystemReset();
    }
}

