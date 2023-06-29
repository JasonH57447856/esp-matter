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
#include <blemesh_bridge.h>
#include "esp_log.h"
#include "cJSON.h"
#include <app_uart.h>
#include "app_mqtt_util.h"
#include "app_uart_util.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "freertos/queue.h"
#include <platform/CHIPDeviceLayer.h>
#include "lockly_c_header.h"
#include "lockly_c_encode.h"
#include "uart_util.h"


#define APP_TASK_NAME "Matter_Bridge"

#define deviceUUID "4e0031003131470532353835"
#define MasterCode "18753912"
#define Password "12345678"



#define APP_EVENT_QUEUE_SIZE 20
#define APP_TASK_STACK_SIZE (3000)
#define APP_TASK_PRIORITY 3

static const char * const TAG = "App-Task";

namespace {
//TimerHandle_t sFunctionTimer; // FreeRTOS app sw timer.



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
 /*   sFunctionTimer = xTimerCreate("FnTmr",          // Just a text name, not used by the RTOS kernel
                                  1,                // == default timer period (mS)
                                  false,            // no timer reload (==one-shot)
                                  (void *) this,    // init timer id = app task obj context
                                  TimerEventHandler // timer callback handler
    );  */
	uint8_t mac_addr[6] = {11,22,33,44,55,66};
	blemesh_bridge_match_bridged_door_lock(mac_addr);
	app_uart_init();
	uart_send_task_init();
/*	if (ConnectivityMgr().IsWiFiStationProvisioned()){
		app_mqtt_init();
		app_sntp_init();
		}*/

	
    return err;
}

void AppTask::AppTaskMain(void * pvParameter)
{
    AppEvent event;
   // Clock::Timestamp lastChangeTime = Clock::kZero;

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

void AppTask::LockActionEventHandler(AppEvent * aEvent)
{
   unsigned int Length;
   unsigned char *buffer;
   uint8_t bda[6];
   ESP_LOGI(TAG, "LockActionEventHandler, lock endpoint: 0x%x, lock status: 0x%x", aEvent->LockEvent.EndpointID,aEvent->LockEvent.Action);

/*   updatePgAesKey((const unsigned char *)MasterCode,MASTER_CODE_LEN,(const unsigned char *)deviceUUID,(DEVICE_UUID_LEN * 2));
   buffer = encodeOpenCloseCmd(&Length,
									   2,
									   (unsigned char *)Password,
									   8,
									   1,
									   aEvent->LockEvent.Action,
									   PGCCmdSrc_alexa,
									   PGCEnType_alexa);
   uart_sent_ble_data((esp_bd_addr_t *)bda, buffer, Length, 100,200);
   free(buffer);*/

}

void AppTask::ServiceInitActionEventHandler(AppEvent * aEvent)
{
   ESP_LOGI(TAG, "ServiceInitActionEventHandler, ServiceType: 0x%x", aEvent->ServiceInitEvent.ServiceType);
   app_mqtt_init();
   app_sntp_init();

}

void AppTask::PostLockActionRequest(chip::EndpointId aEndpointID, int8_t aAction)
{
    AppEvent event;
    event.Type              = AppEvent::kEventType_Lock;
    event.LockEvent.EndpointID  = aEndpointID;
    event.LockEvent.Action = aAction;
    event.Handler           = LockActionEventHandler;
    PostEvent(&event);
}

esp_err_t AppTask::PostMqttActionRequest(uint32_t len, uint8_t* buf)
{
    AppEvent event;
    event.Type              = AppEvent::kEventType_Mqtt;
    event.MqttEvent.len  = len;
    event.MqttEvent.buf = buf;
    event.Handler           = MqttEventHandler;
    return PostEvent(&event);
}

esp_err_t AppTask::PostUartActionRequest(uint32_t len, uint8_t* buf)
{
    AppEvent event;
    event.Type              = AppEvent::kEventType_Uart;
    event.UartEvent.len  = len;
    event.UartEvent.buf = buf;
    event.Handler           =UartReceiveEventHandler;
    return PostEvent(&event);
}

esp_err_t AppTask::PostServiceInitActionRequest(uint8_t type)
{
    AppEvent event;
    event.Type              = AppEvent::kEventType_ServiceInit;
    event.ServiceInitEvent.ServiceType  = type;
    event.Handler           =ServiceInitActionEventHandler;
    return PostEvent(&event);
}

esp_err_t AppTask::PostEvent(const AppEvent * aEvent)
{
    if (sAppEventQueue != NULL)
    {
        if (!xQueueSend(sAppEventQueue, aEvent, (TickType_t)0))
        {
            ESP_LOGI(TAG, "Failed to post event to app task event queue");
			return ESP_FAIL;
        }
		else
			return ESP_OK;
    }
	return ESP_FAIL;
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


void app_uart_process(uint8_t *buf, uint32_t length)
{
    ESP_LOGI(TAG, "app_uart_process length= %" PRIX32 " ", length);
	uint8_t* buffer = (uint8_t*) pvPortMalloc(length);
	if(buffer){
		memcpy(buffer, (uint8_t*)buf, length);
		if(GetAppTask().PostUartActionRequest(length, buffer)==ESP_FAIL){
			 vPortFree(buffer);
		     buffer = NULL;
			}
	}else{
		ESP_LOGE(TAG, "app uart pvPortMalloc error");
	}
}

void app_mqtt_process(esp_mqtt_event_handle_t event_data)
{
	ESP_LOGI(TAG, "app_mqtt_process");
	ESP_LOGI(TAG, "TOPIC=%s", event_data->topic);
	
	uint8_t* buf = (uint8_t*) pvPortMalloc(event_data->data_len);
	if(buf){
		memcpy(buf, (uint8_t*)event_data->data, event_data->data_len);
		if(GetAppTask().PostMqttActionRequest(event_data->data_len, buf)==ESP_FAIL){
			 vPortFree(buf);
		     buf = NULL;
			}
	}else{
		ESP_LOGE(TAG, "app mqtt pvPortMalloc error");
	}
	
}


