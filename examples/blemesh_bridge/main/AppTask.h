/*
 *
 *    Copyright (c) 2020 Project CHIP Authors
 *
 *    Licensed under the Apache License, Version 2.0 (the "License");
 *    you may not use this file except in compliance with the License.
 *    You may obtain a copy of the License at
 *
 *        http://www.apache.org/licenses/LICENSE-2.0
 *
 *    Unless required by applicable law or agreed to in writing, software
 *    distributed under the License is distributed on an "AS IS" BASIS,
 *    WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *    See the License for the specific language governing permissions and
 *    limitations under the License.
 */

#pragma once

#include <stdbool.h>
#include <stdint.h>

//#include <lock/AppEvent.h>
//#include <lock/BoltLockManager.h>

#include "freertos/FreeRTOS.h"
#include <ble/BLEEndPoint.h>
#include <lib/support/CodeUtils.h>
#include <platform/CHIPDeviceLayer.h>

#include "app_mqtt.h"
#include "app_uart.h"



struct AppEvent;
typedef void (*EventHandler)(AppEvent *);

struct AppEvent
{
    enum AppEventTypes
    {
        kEventType_Button = 0,
        kEventType_Timer,
        kEventType_Lock,
        kEventType_Install,
        kEventType_Mqtt,
        kEventType_Uart,
    };

    uint16_t Type;

    union
    {
        struct
        {
            uint8_t Action;
        } ButtonEvent;
        struct
        {
            void * Context;
        } TimerEvent;
        struct
        {
            uint8_t Action;
            int8_t Actor;
        } LockEvent;
		struct
        {
            uint8_t len;
            uint8_t* buf;
        } MqttEvent;
		struct
        {
            uint8_t len;
            uint8_t* buf;
        } UartEvent;		
    };

    EventHandler Handler;
};


class AppTask
{

public:
    esp_err_t StartAppTask();
    static void AppTaskMain(void * pvParameter);

    void PostLockActionRequest(int8_t aActor, int8_t aAction);
	void PostMqttActionRequest(uint8_t len, uint8_t* buf);
	void PostUartActionRequest(uint8_t len, uint8_t* buf);
    void PostEvent(const AppEvent * event);



private:
    friend AppTask & GetAppTask(void);

    esp_err_t Init();


    void DispatchEvent(AppEvent * event);

    static void FunctionTimerEventHandler(AppEvent * aEvent);
    static void FunctionHandler(AppEvent * aEvent);
    static void LockActionEventHandler(AppEvent * aEvent);
	static void MqttActionEventHandler(AppEvent * aEvent);
	static void UartActionEventHandler(AppEvent * aEvent);
    static void TimerEventHandler(TimerHandle_t xTimer);


    void StartTimer(uint32_t aTimeoutMs);

    static AppTask sAppTask;
};

inline AppTask & GetAppTask(void)
{
    return AppTask::sAppTask;
}



