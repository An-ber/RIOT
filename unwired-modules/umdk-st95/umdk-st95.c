/*
 * Copyright (C) 2016-2018 Unwired Devices LLC <info@unwds.com>

 * Permission is hereby granted, free of charge, to any person obtaining
 * a copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the Software
 * is furnished to do so, subject to the following conditions:

 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.

 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR
 * PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE
 * FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR
 * OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 */

/**
 * @defgroup
 * @ingroup
 * @brief
 * @{
 * @file        umdk-st95.c
 * @brief       umdk-st95 module implementation
 * @author      Mikhail Perkov

 */

#ifdef __cplusplus
extern "C" {
#endif

/* define is autogenerated, do not change */
#undef _UMDK_MID_
#define _UMDK_MID_ UNWDS_ST95_MODULE_ID

/* define is autogenerated, do not change */
#undef _UMDK_NAME_
#define _UMDK_NAME_ "st95"

#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <limits.h>

#include "board.h"

#include "st95.h"

#include "umdk-ids.h"
#include "unwds-common.h"
#include "include/umdk-st95.h"

#include "thread.h"
#include "rtctimers-millis.h"

#define ENABLE_DEBUG (1)
#include "debug.h"

static msg_t msg_wu = { .type = UMDK_ST95_MSG_WAKE_UP, };
static msg_t msg_rx = { .type = UMDK_ST95_MSG_UID, };

static kernel_pid_t radio_pid;
static uwnds_cb_t *callback;

static st95_t dev;

static st95_params_t st95_params = { .spi = UMDK_ST95_SPI_DEV, .cs_spi = UMDK_ST95_SPI_CS, 
                                .irq_in = UMDK_ST95_IRQ_IN, .irq_out = UMDK_ST95_IRQ_OUT, 
                                .ssi_0 = UMDK_ST95_SSI_0, .ssi_1 = UMDK_ST95_SSI_1,
                                .vcc = UMDK_ST95_VCC_ENABLE };

static uint8_t length_uid = 0;
static uint8_t uid_full[255];
static uint8_t sak = 0;

static void umdk_st95_get_uid(void);

#if ENABLE_DEBUG
    #define PRINTBUFF _printbuff
    static void _printbuff(uint8_t *buff, unsigned len)
    {
        while (len) {
            len--;
            printf("%02X ", *buff++);
        }
        printf("\n");
    }
#else
    #define PRINTBUFF(...)
#endif


static void *radio_send(void *arg)
{
    (void) arg;
    msg_t msg;
    msg_t msg_queue[16];
    msg_init_queue(msg_queue, 16);
      
    while (1) {
        msg_receive(&msg);
        
        module_data_t data;
        data.as_ack = true;
        data.data[0] = _UMDK_MID_;
        data.length = 1;

        switch(msg.type) {
            case UMDK_ST95_MSG_WAKE_UP: {
                if(st95_is_wake_up(&dev) == ST95_WAKE_UP) {
                    umdk_st95_get_uid(); 
                }
                             
                break;
            }
            case UMDK_ST95_MSG_UID: {
                if(msg.content.value == UMDK_ST95_UID_OK) {
                    DEBUG("Sak: %02X -> UID[%d]: ", sak, length_uid);
                    _printbuff(uid_full, length_uid);
                   
                    memcpy(data.data + 1, uid_full, length_uid);
                    data.length += length_uid;
                }
                else {
                    DEBUG("[ERROR]: Invalid UID\n");
                    _printbuff(uid_full, length_uid);
                    
                    data.data[1] = 0;
                    data.length = 2;
                }
                
                DEBUG("RADIO: ");
                _printbuff(data.data, data.length);
                
                callback(&data);
                rtctimers_millis_sleep(UMDK_ST95_DELAY_DETECT_MS);                
                st95_sleep(&dev);      
            }
            default: 
            break;            
        }
    }
    return NULL;
}

static void umdk_st95_get_uid(void)
{
    length_uid = 0;
    sak = 0;
    memset(uid_full, 0x00, sizeof(uid_full));
    
    if(st95_get_uid(&dev, &length_uid, uid_full, &sak) == ST95_OK) {
        msg_rx.content.value = UMDK_ST95_UID_OK;        
    }
    else {
        msg_rx.content.value = UMDK_ST95_UID_ERROR;
    }
    
    msg_try_send(&msg_rx, radio_pid);
}

static void wake_up_cb(void * arg)
{
    (void) arg;
    msg_try_send(&msg_wu, radio_pid);
}

void umdk_st95_init(uwnds_cb_t *event_callback)
{
    (void)event_callback;
    callback = event_callback;
   
    dev.cb = wake_up_cb;
                                                                   
     /* Create handler thread */
    char *stack = (char *) allocate_stack(UMDK_ST95_STACK_SIZE);
    if (!stack) {
        return;
    }

    radio_pid = thread_create(stack, UMDK_ST95_STACK_SIZE, THREAD_PRIORITY_MAIN - 1, THREAD_CREATE_STACKTEST, radio_send, NULL, "st95 thread");
    
    if(st95_init(&dev, &st95_params) != ST95_OK){
        puts("[umdk-" _UMDK_NAME_ "] st95 driver initialization error");
    }
    else {   
        puts("[umdk-" _UMDK_NAME_ "] st95 driver initialization success");
        st95_sleep(&dev);
    }
}

bool umdk_st95_cmd(module_data_t *cmd, module_data_t *reply)
{      
    return false;

    reply->as_ack = true;
    reply->length = 1;
    reply->data[0] = _UMDK_MID_;
    reply->data[0] = cmd->data[0];
    return true; /* Allow reply */
}


#ifdef __cplusplus
}
#endif