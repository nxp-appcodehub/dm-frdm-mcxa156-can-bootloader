/*
 * Copyright 2023 NXP
 * All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */
 
#include "app_can.h"


#define RX_MESSAGE_BUFFER_NUM           (0)
#define TX_MESSAGE_BUFFER_NUM           (1)
#define CANFD_SEND_BLOCKING_TIMEOUT     (20000)


#ifndef RT_ALIGN 
#define RT_ALIGN(size, align)           (((size) + (align) - 1) & ~((align) - 1))
#endif

static void (*rx_indicate)(uint32_t id, uint8_t *buf, uint8_t len) = NULL;

static flexcan_handle_t flexcanHandle;
static flexcan_mb_transfer_t txXfer, rxXfer;
static flexcan_fd_frame_t txFrame, rxFrame;
static uint8_t is_send_complete = 0;

#define BYTE_TO_DLC(byteLength) ((byteLength <= 8) ? (byteLength) : \
                                (byteLength <= 12) ? 9 : \
                                (byteLength <= 16) ? 10 : \
                                (byteLength <= 20) ? 11 : \
                                (byteLength <= 24) ? 12 : \
                                (byteLength <= 32) ? 13 : \
                                (byteLength <= 48) ? 14 : \
                                (byteLength <= 64) ? 15 : -1)


void app_can_dump_msg(uint32_t id, uint8_t *buf, uint8_t len)
{
    int i;

    PRINTF("ID:0x%X, len:%d, data:", id, len);
    for(i=0; i<len; i++)
    {
        PRINTF(" %X", buf[i]);
    }
   
    PRINTF("\r\n");
}

static uint32_t endianSwap4bytes(uint32_t var)
{

  uint8_t *bytePtr = (uint8_t*)&var;
  uint32_t swappedVar = (uint32_t)((bytePtr[0] << 24) | 
                                   (bytePtr[1] << 16) | 
                                   (bytePtr[2] << 8) | 
                                   (bytePtr[3]));
  return (swappedVar);
}


static void endianSwapBuffer(uint8_t * dBuff, uint8_t * sBuff, uint8_t length)
{
  uint8_t ctr = 0;
  uint8_t byteCtr = 0;
  length = length / 4;
  
  for(ctr = 0; ctr < length; ctr++)
  {
    dBuff[0 + byteCtr] = sBuff[3 + byteCtr];
    dBuff[1 + byteCtr] = sBuff[2 + byteCtr];
    dBuff[2 + byteCtr] = sBuff[1 + byteCtr];
    dBuff[3 + byteCtr] = sBuff[0 + byteCtr]; 
    byteCtr = byteCtr + 4;
  }
  
}


int app_can_send(uint32_t can_id, uint8_t* buf, uint8_t len)
{
    volatile int i, timeout, ret;

    txFrame.format = (uint8_t)kFLEXCAN_FrameFormatStandard;
    txFrame.type   = (uint8_t)kFLEXCAN_FrameTypeData;
    txFrame.id     = FLEXCAN_ID_STD(can_id);
    txFrame.length = BYTE_TO_DLC(len);
    txFrame.brs = 1U;
    txFrame.edl = 1U;

    for (i = 0; i < RT_ALIGN(len, 4)/4; i++)
    {
        txFrame.dataWord[i] = endianSwap4bytes((uint32_t)(*((uint32_t *)buf + i)));
    }

    is_send_complete = 0;
    timeout = 0;
    ret = FLEXCAN_TransferFDSendNonBlocking(CAN0, &flexcanHandle, &txXfer);
    
    while(is_send_complete == 0 && timeout < CANFD_SEND_BLOCKING_TIMEOUT)
    {
        timeout++;
    }
    
    return ret;
}



static FLEXCAN_CALLBACK(flexcan_callback)
{
    switch (status)
    {
        /* Process FlexCAN Rx event. */
        case kStatus_FLEXCAN_RxIdle:
            if (RX_MESSAGE_BUFFER_NUM == result)
            {
                static uint8_t buf[64], len;
                static uint32_t id;
                
                len = DLC_LENGTH_DECODE(rxFrame.length);
                
                endianSwapBuffer((uint8_t *)buf, (uint8_t *)&(rxFrame.dataWord[0]), DLC_LENGTH_DECODE(rxFrame.length));
    
                id = rxFrame.id >> CAN_ID_STD_SHIFT;
                
                if(rx_indicate) rx_indicate(id, buf, len);
                
                FLEXCAN_TransferFDReceiveNonBlocking(CAN0, &flexcanHandle, &rxXfer);
            }
            break;

        /* Process FlexCAN Tx event. */
        case kStatus_FLEXCAN_TxIdle:
            if (TX_MESSAGE_BUFFER_NUM == result)
            {
                is_send_complete = 1;
            }
            break;

        default:
            break;
    }
}

void app_can_init(uint32_t bitrate, uint32_t bitrateFD, uint8_t rx_id)
{
    flexcan_config_t flexcanConfig;
    flexcan_rx_mb_config_t mbConfig;
    
    /* Init FlexCAN module. */
    /*
     * flexcanConfig.clkSrc                 = kFLEXCAN_ClkSrc0;
     * flexcanConfig.bitRate                = 1000000U;
     * flexcanConfig.bitRateFD              = 2000000U;
     * flexcanConfig.maxMbNum               = 16;
     * flexcanConfig.enableSelfWakeup       = false;
     * flexcanConfig.enableIndividMask      = false;
     * flexcanConfig.disableSelfReception   = false;
     * flexcanConfig.enableListenOnlyMode   = false;
     * flexcanConfig.enableDoze             = false;
     */
    FLEXCAN_GetDefaultConfig(&flexcanConfig);
    flexcanConfig.bitRate = bitrate;
    flexcanConfig.bitRateFD = bitrateFD;
    flexcanConfig.disableSelfReception   = true;

    flexcan_timing_config_t timing_config;
    memset(&timing_config, 0, sizeof(flexcan_timing_config_t));

    if (FLEXCAN_FDCalculateImprovedTimingValues(CAN0, flexcanConfig.bitRate, flexcanConfig.baudRateFD, CLOCK_GetFlexcanClkFreq(), &timing_config))
    {
        /* Update the improved timing configuration*/
        memcpy(&(flexcanConfig.timingConfig), &timing_config, sizeof(flexcan_timing_config_t));
    }
    else
    {
        PRINTF("No found Improved Timing Configuration. Just used default configuration\r\n\r\n");
    }
    
    FLEXCAN_FDInit(CAN0, &flexcanConfig, CLOCK_GetFlexcanClkFreq(), kFLEXCAN_64BperMB, true);
    
    /* Setup Rx Message Buffer. */
    mbConfig.format = kFLEXCAN_FrameFormatStandard;
    mbConfig.type   = kFLEXCAN_FrameTypeData;
    mbConfig.id     = FLEXCAN_ID_STD(rx_id);
    
    FLEXCAN_SetFDRxMbConfig(CAN0, RX_MESSAGE_BUFFER_NUM, &mbConfig, true);
    FLEXCAN_SetRxMbGlobalMask(CAN0, FLEXCAN_ID_STD(0x7F));
    
    /* Setup Tx Message Buffer. */
    FLEXCAN_SetFDTxMbConfig(CAN0, TX_MESSAGE_BUFFER_NUM, true);

    FLEXCAN_TransferCreateHandle(CAN0, &flexcanHandle, flexcan_callback, NULL);
    
    txXfer.mbIdx = (uint8_t)TX_MESSAGE_BUFFER_NUM;
    txXfer.framefd = &txFrame;
    
    rxXfer.mbIdx = (uint8_t)RX_MESSAGE_BUFFER_NUM;
    rxXfer.framefd = &rxFrame;
    FLEXCAN_TransferFDReceiveNonBlocking(CAN0, &flexcanHandle, &rxXfer);
}



void app_can_set_rx_indicate(void (*indicate)(uint32_t id, uint8_t *buf, uint8_t len))
{
    rx_indicate = indicate;
}

