/*
 * Copyright 2023 NXP
 * All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */
 
#include "mq.h"

#define C_MAX_SIZE          40

#ifndef ABS
#define ABS(a)         (((a) < 0) ? (-(a)) : (a))
#endif

typedef struct
{
    msg_t m_Msg[C_MAX_SIZE];
    uint16_t front;
    uint16_t rear;
}SqQueue_t;

//
static SqQueue_t msgQueue;


void mq_init(void)
{
    msgQueue.front = msgQueue.rear = 0;
}

/*
if(mq_exist())
{
    pMsg = mq_pop();
}
*/
uint8_t mq_exist(void)
{
    return msgQueue.front != msgQueue.rear;
}

msg_t *mq_pop(void)
{
    msg_t *pMsg = (void*)0;
    if(msgQueue.front != msgQueue.rear)
    {
        pMsg = &msgQueue.m_Msg[msgQueue.front];
        msgQueue.front = (msgQueue.front + 1) % C_MAX_SIZE;
    }
    return pMsg;
}

/* 0 fail  1 succ */
uint8_t mq_push(msg_t pMsg)
{ 
    if(msgQueue.front == (msgQueue.rear+1) % C_MAX_SIZE)
    {
        return 0;
    }
    else
    {
        msgQueue.m_Msg[msgQueue.rear] = pMsg;
        msgQueue.rear = (msgQueue.rear + 1) % C_MAX_SIZE;
        return 1;
    }
}

/* 0:full, 1:not full */
uint8_t mq_get_empty(void)
{
    
    if(msgQueue.front > msgQueue.rear)
    {
        return 0;
    }
    else
    {
        return C_MAX_SIZE - (msgQueue.rear - msgQueue.front);
    }
}
