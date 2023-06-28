/*
   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/

#pragma once

#include "AppTask.h"

#ifdef __cplusplus
extern "C" {
#endif

struct UartSendEvent
{
    enum UartSendEventTypes
    {
        kEventType_Data = 0,
        kEventType_Ack,
    };
    uint16_t Type;
    uint32_t len;
    uint8_t* buf;
};


void UartReceiveEventHandler(AppEvent * aEvent);
void app_uart_send(const void *src, size_t size);
esp_err_t uart_send_task_init(void);




#ifdef __cplusplus
}
#endif
