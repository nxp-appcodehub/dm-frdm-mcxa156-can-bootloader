/*
 * Copyright 2023 NXP
 * All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <stdio.h>
#include <stdlib.h>

#include "fsl_device_registers.h"
#include "fsl_debug_console.h"
#include "pin_mux.h"
#include "clock_config.h"
#include "board.h"

#include "usb_device_config.h"
#include "usb.h"
#include "usb_device.h"

#include "usb_device_class.h"
#include "usb_device_cdc_acm.h"
#include "usb_device_ch9.h"

#include "usb_device_descriptor.h"
#include "virtual_com.h"
#include "mq.h"
#include "app_can.h"


/* CAN define */
#define CAN_BIT_RATE                    (500*1000)
#define CAN_BIT_RATE_FD                 (2*1000*1000)
#define CAN_TX_ID                       (0x09)
#define CAN_RX_ID                       (0x08)
#define CANFD_FRAME_PAYLOAD_MAX         (64-8)
#define CANFD_SEND_BLOCKING_TIMEOUT     (100000)

/* message type define */
#define MSG_CAN_RX          (1)
#define MSG_CAN_TX          (2)
#define MSG_USB_CDC_OUT     (3)
#define MSG_USB_CDC_IN      (4)


void can_rx_indicate(uint32_t id, uint8_t *buf, uint8_t len);
void CDC_APPInit(void);
extern volatile  uint8_t is_cdc_in_complete;
extern usb_cdc_vcom_struct_t s_cdcVcom;

void main(void)
{
    uint8_t tx_can_buf[64];
    
    BOARD_InitBootPins();
    BOARD_InitBootClocks();
    BOARD_InitDebugConsole();

    CDC_APPInit();
    
    PRINTF("USB CDC to CAN bridge demo\r\n");
    PRINTF("CAN BAUD,FD:%d,%d\r\n", CAN_BIT_RATE, CAN_BIT_RATE_FD);
    PRINTF("CAN TX_ID:%d\r\n", CAN_TX_ID);
    PRINTF("CAN RX_ID:%d\r\n", CAN_RX_ID);


    msg_t *rx_msg;
    
    app_can_init(CAN_BIT_RATE, CAN_BIT_RATE_FD, CAN_RX_ID);
    app_can_set_rx_indicate(can_rx_indicate);
    
    int i;
    
    while (1)
    {
        if(mq_exist())
        {
            rx_msg = mq_pop();
            if(rx_msg->cmd == MSG_USB_CDC_OUT)
            {
                uint8_t ofs = 0;
                uint8_t payload_len = 0;
                uint8_t total_rx_size = rx_msg->len;

                /* translate payload to CAN frame */
                while(total_rx_size)
                {
                    memset(tx_can_buf, 0, sizeof(tx_can_buf));

                    (total_rx_size < CANFD_FRAME_PAYLOAD_MAX)?(payload_len = total_rx_size):(payload_len = CANFD_FRAME_PAYLOAD_MAX);
                    tx_can_buf[7] = payload_len;

                    memcpy(&tx_can_buf[8], rx_msg->buf+ofs, payload_len);

                    ofs += payload_len;

                    app_can_send(CAN_TX_ID, tx_can_buf, 64);
                    total_rx_size -= payload_len;
                }

                //PRINTF("cdc out msg->len:%d, total:%d\r\n", rx_msg->len, cdc_out_count);
            }

            if(rx_msg->cmd == MSG_CAN_RX)
            {
                //PRINTF("can_rx:id:0x%X, size:%d\r\n", rx_msg->can_id, rx_msg->buf[7]);
                
                is_cdc_in_complete = 0;
                volatile uint32_t timeout = 0;
                
                USB_DeviceCdcAcmSend(s_cdcVcom.cdcAcmHandle, USB_CDC_VCOM_BULK_IN_ENDPOINT, &rx_msg->buf[8], rx_msg->buf[7]);
                
                while(is_cdc_in_complete == 0 && timeout < CANFD_SEND_BLOCKING_TIMEOUT)
                {
                    timeout++;
                }
            }
        }
        
    }

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
    PRINTF("%s\r\n", __FUNCTION__);
    while(1)
    {
         NVIC_SystemReset();
    }
}


