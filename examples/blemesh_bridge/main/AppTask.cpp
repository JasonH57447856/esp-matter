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

#include "AppTask.h"
#include <app_blemesh.h>
#include "esp_log.h"

/*
#include <app-common/zap-generated/attributes/Accessors.h>
#include <app-common/zap-generated/ids/Clusters.h>
#include <app/server/OnboardingCodesUtil.h>
#include <app/server/Server.h>
#include <app/util/af-enums.h>
#include <app/util/attribute-storage.h>
#include <lib/support/CodeUtils.h>
#include <lock/AppConfig.h>
#include <lock/AppEvent.h>
#include <platform/CHIPDeviceLayer.h>
#include <platform/internal/CHIPDeviceLayerInternal.h>*/


#define APP_TASK_NAME "Matter_Bridge"


#define FACTORY_RESET_TRIGGER_TIMEOUT 3000
#define FACTORY_RESET_CANCEL_WINDOW_TIMEOUT 3000
#define APP_EVENT_QUEUE_SIZE 10
#define APP_TASK_STACK_SIZE (3000)
#define APP_TASK_PRIORITY 1
#define STATUS_LED_GPIO_NUM GPIO_NUM_2 // Use LED1 (blue LED) as status LED on DevKitC

static const char * const TAG = "App-Task";

namespace {
TimerHandle_t sFunctionTimer; // FreeRTOS app sw timer.


BaseType_t sAppTaskHandle;
QueueHandle_t sAppEventQueue;


StackType_t appStack[APP_TASK_STACK_SIZE / sizeof(StackType_t)];
} // namespace

using namespace ::chip::DeviceLayer;
using namespace ::chip::System;

AppTask AppTask::sAppTask;

esp_err_t AppTask::StartAppTask()
{
    sAppEventQueue = xQueueCreate(APP_EVENT_QUEUE_SIZE, sizeof(AppEvent));
    if (sAppEventQueue == NULL)
    {
        ESP_LOGE(TAG, "Failed to allocate app event queue");
        return ESP_FAIL;
    }

    // Start App task.
    sAppTaskHandle = xTaskCreate(AppTaskMain, APP_TASK_NAME, ArraySize(appStack), NULL, APP_TASK_PRIORITY, NULL);
    return sAppTaskHandle ? ESP_OK : ESP_FAIL;
}

esp_err_t AppTask::Init()
{
	esp_err_t err = ESP_OK;

    // Create FreeRTOS sw timer for Function Selection
    sFunctionTimer = xTimerCreate("FnTmr",          // Just a text name, not used by the RTOS kernel
                                  1,                // == default timer period (mS)
                                  false,            // no timer reload (==one-shot)
                                  (void *) this,    // init timer id = app task obj context
                                  TimerEventHandler // timer callback handler
    );  
	uint8_t mac_addr[6] = {11,22,33,44,55,66};
	blemesh_bridge_match_bridged_door_lock(mac_addr);
	//app_mqtt_init();
	app_uart_init();

	
    return err;
}

void AppTask::AppTaskMain(void * pvParameter)
{
    AppEvent event;
    Clock::Timestamp lastChangeTime = Clock::kZero;

    esp_err_t err = sAppTask.Init();
    if (err != ESP_OK)
    {
        ESP_LOGI(TAG, "AppTask.Init() failed");
        return;
    }

    ESP_LOGI(TAG, "App Task started");

    while (true)
    {
        BaseType_t eventReceived = xQueueReceive(sAppEventQueue, &event, portMAX_DELAY);
        while (eventReceived == pdTRUE)
        {
            sAppTask.DispatchEvent(&event);
            eventReceived = xQueueReceive(sAppEventQueue, &event, 0);
        }

    }
}

void AppTask::TimerEventHandler(TimerHandle_t xTimer)
{
    AppEvent event;
    event.Type                = AppEvent::kEventType_Timer;
    event.TimerEvent.Context = (void *) xTimer;
    event.Handler             = FunctionTimerEventHandler;
    sAppTask.PostEvent(&event);
}

void AppTask::FunctionTimerEventHandler(AppEvent * aEvent)
{
}

void AppTask::LockActionEventHandler(AppEvent * aEvent)
{

   ESP_LOGI(TAG, "LockActionEventHandler, lock status: 0x%x", aEvent->LockEvent.Action);

}

void AppTask::PostLockActionRequest(int8_t aActor, int8_t aAction)
{
    AppEvent event;
    event.Type              = AppEvent::kEventType_Lock;
    event.LockEvent.Actor  = aActor;
    event.LockEvent.Action = aAction;
    event.Handler           = LockActionEventHandler;
    PostEvent(&event);
}

void AppTask::PostEvent(const AppEvent * aEvent)
{
    if (sAppEventQueue != NULL)
    {
        if (!xQueueSend(sAppEventQueue, aEvent, 1))
        {
            ESP_LOGI(TAG, "Failed to post event to app task event queue");
        }
    }
}

void AppTask::DispatchEvent(AppEvent * aEvent)
{
    if (aEvent->Handler)
    {
        aEvent->Handler(aEvent);
    }
    else
    {
        ESP_LOGI(TAG, "Event received with no handler. Dropping event.");
    }
}


