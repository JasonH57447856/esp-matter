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


//#include "freertos/FreeRTOS.h"
//#include <ble/BLEEndPoint.h>
//#include <lib/support/CodeUtils.h>
//#include <platform/CHIPDeviceLayer.h>

#include "app_mqtt.h"
#include "app_uart.h"
#include "app_sntp.h"
#include "LEDWidget.h"

#include <esp_matter.h>
#include <esp_matter_core.h>

using namespace chip;
using namespace chip::app::Clusters;


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
        kEventType_ServiceInit,
    };

    uint16_t Type;

    union
    {
        struct
        {
            uint8_t mPinNo;
            uint8_t mAction;
        } ButtonEvent;		
        struct
        {
            void * Context;
        } TimerEvent;
        struct
        {
            chip::EndpointId EndpointID;
            bool Action;
        } LockEvent;
        struct
        {
			uint8_t ServiceType;
        } ServiceInitEvent;		
		struct
        {
            uint32_t len;
            uint8_t* buf;
        } MqttEvent;
		struct
        {
            uint32_t len;
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

    void PostLockActionRequest(chip::EndpointId aEndpointID,  int8_t aAction);
	esp_err_t PostServiceInitActionRequest(uint8_t type);
	esp_err_t PostMqttActionRequest(uint32_t len, uint8_t* buf);
	esp_err_t PostUartActionRequest(uint32_t len, uint8_t* buf);
    esp_err_t PostEvent(const AppEvent * event);
	void ButtonEventHandler(uint8_t btnIdx, uint8_t btnAction);



private:
    friend AppTask & GetAppTask(void);

    esp_err_t Init();


    void DispatchEvent(AppEvent * event);
    void StartTimer(uint32_t aTimeoutMs);
    void CancelTimer(void);

    static void FunctionTimerEventHandler(AppEvent * aEvent);
    static void FunctionHandler(AppEvent * aEvent);	
    static void TimerEventHandler(TimerHandle_t xTimer);
    static void LockActionEventHandler(AppEvent * aEvent);
	static void ServiceInitActionEventHandler(AppEvent * aEvent);

    enum Function_t
    {
        kFunction_NoneSelected   = 0,
        kFunction_SoftwareUpdate = 0,
        kFunction_StartBleAdv    = 1,
        kFunction_FactoryReset   = 2,

        kFunction_Invalid
    } Function;

    Function_t mFunction;
    bool mFunctionTimerActive;


    static AppTask sAppTask;
};

inline AppTask & GetAppTask(void)
{
    return AppTask::sAppTask;
}


