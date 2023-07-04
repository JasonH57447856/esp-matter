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
#include "Button.h"
#include "LEDWidget.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "freertos/queue.h"
#include <platform/CHIPDeviceLayer.h>
#include <app/server/Server.h>

#include "lockly_c_header.h"
#include "lockly_c_encode.h"
#include "uart_util.h"
#include "driver/gpio.h"


#define APP_TASK_NAME "Matter_Bridge"

#define deviceUUID "250045003437470734383732"
#define MasterCode "92210994"
#define Password "111555"


#define SYSTEM_STATE_LED GPIO_NUM_18
#define SYSTEM_STATE_LED_GREEN GPIO_NUM_19

#define APP_FUNCTION_BUTTON GPIO_NUM_26
#define APP_BUTTON_DEBOUNCE_PERIOD_MS 50
#define APP_BUTTON_PRESSED 0
#define APP_BUTTON_RELEASED 1
#define FACTORY_RESET_TRIGGER_TIMEOUT 3000
#define FACTORY_RESET_CANCEL_WINDOW_TIMEOUT 3000


#define APP_EVENT_QUEUE_SIZE 20
#define APP_TASK_STACK_SIZE (3000)
#define APP_TASK_PRIORITY 3

static const char * const TAG = "App-Task";




namespace {
TimerHandle_t sFunctionTimer; // FreeRTOS app sw timer.

LEDWidget sRedLED;
LEDWidget sGreenLED;
Button resetButton;


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

    sRedLED.Init(SYSTEM_STATE_LED);
	sGreenLED.Init(SYSTEM_STATE_LED_GREEN);
    resetButton.Init(APP_FUNCTION_BUTTON, APP_BUTTON_DEBOUNCE_PERIOD_MS);
    sRedLED.Set(true);
    sGreenLED.Set(true);	
	uint8_t mac_addr[6] = {11,22,33,44,55,66};
	blemesh_bridge_match_bridged_door_lock(mac_addr);
	app_uart_init();
	uart_send_task_init();
	if (ConnectivityMgr().IsWiFiStationProvisioned()){
        sRedLED.Set(true);
        sGreenLED.Set(false);
		}

	
    return err;
}

void AppTask::AppTaskMain(void * pvParameter)
{
    AppEvent event;
	bool sHaveBLEConnections;
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
        BaseType_t eventReceived = xQueueReceive(sAppEventQueue, &event, pdMS_TO_TICKS(10));
        while (eventReceived == pdTRUE)
        {
            sAppTask.DispatchEvent(&event);
            eventReceived = xQueueReceive(sAppEventQueue, &event, 0);
        }
	if (PlatformMgr().TryLockChipStack())
	{
		sHaveBLEConnections = (ConnectivityMgr().NumBLEConnections() != 0);
		PlatformMgr().UnlockChipStack();
	}
	
	// Update the status LED if factory reset has not been initiated.
	//
	// If system has "full connectivity", keep the LED On constantly.
	//
	// If no connectivity to the service OR subscriptions are not fully
	// established THEN blink the LED Off for a short period of time.
	//
	// If the system has ble connection(s) uptill the stage above, THEN blink
	// the LEDs at an even rate of 100ms.
	//
	// Otherwise, blink the LED ON for a very short time.
	
	if ((sAppTask.mFunction != kFunction_FactoryReset)&&(!ConnectivityMgr().IsWiFiStationProvisioned()))
	{
		if (sHaveBLEConnections)
		{
			sRedLED.Blink(100, 100);
		}
		else
		{
			sRedLED.Blink(50, 950);
		}
	}else if(sAppTask.mFunction == kFunction_FactoryReset)
		sRedLED.Blink(500, 500);
	//sRedLED.Animate();
	
	Clock::Timestamp now			= SystemClock().GetMonotonicTimestamp();
	Clock::Timestamp nextChangeTime = lastChangeTime + Clock::Seconds16(5);
	
	if (nextChangeTime < now)
	{
		lastChangeTime = now;	
    	ESP_LOGI(TAG, "[APP] Free memory: %"PRIX32" bytes", esp_get_free_heap_size());
	}
	if (resetButton.Poll())
	{
		ESP_LOGI(TAG, "resetButton.Poll");
		if (resetButton.IsPressed())
		{
			ESP_LOGI(TAG, "resetButton.IsPressed");
			GetAppTask().ButtonEventHandler(APP_FUNCTION_BUTTON, APP_BUTTON_PRESSED);
		}
		else
		{
			GetAppTask().ButtonEventHandler(APP_FUNCTION_BUTTON, APP_BUTTON_RELEASED);
			ESP_LOGI(TAG, "resetButton.IsReleased");
		}
	}

    }
}


void AppTask::ButtonEventHandler(uint8_t btnIdx, uint8_t btnAction)
{
    if (btnIdx != APP_FUNCTION_BUTTON)
    {
        return;
    }

    AppEvent button_event             = {};
    button_event.Type                = AppEvent::kEventType_Button;
    button_event.ButtonEvent.mPinNo  = btnIdx;
    button_event.ButtonEvent.mAction = btnAction;
    button_event.Handler = FunctionHandler;
    sAppTask.PostEvent(&button_event);

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
    if (aEvent->Type != AppEvent::kEventType_Timer)
    {
        return;
    }

    // If we reached here, the button was held past FACTORY_RESET_TRIGGER_TIMEOUT,
    // initiate factory reset
    if (sAppTask.mFunctionTimerActive && sAppTask.mFunction == kFunction_StartBleAdv)
    {
        ESP_LOGI(TAG, "Factory Reset Triggered. Release button within %ums to cancel.", FACTORY_RESET_CANCEL_WINDOW_TIMEOUT);

        // Start timer for FACTORY_RESET_CANCEL_WINDOW_TIMEOUT to allow user to
        // cancel, if required.
        sAppTask.StartTimer(FACTORY_RESET_CANCEL_WINDOW_TIMEOUT);

        sAppTask.mFunction = kFunction_FactoryReset;

        // Turn off all LEDs before starting blink to make sure blink is
        // co-ordinated.
        //sRedLED.Set(false);
        //sRedLED.Blink(500,500);
    }
    else if (sAppTask.mFunctionTimerActive && sAppTask.mFunction == kFunction_FactoryReset)
    {
        // Actually trigger Factory Reset
        sAppTask.mFunction = kFunction_NoneSelected;
        chip::Server::GetInstance().ScheduleFactoryReset();
    }
}

void AppTask::FunctionHandler(AppEvent * aEvent)
{
    if (aEvent->ButtonEvent.mPinNo != APP_FUNCTION_BUTTON)
    {
        return;
    }

    // To trigger software update: press the APP_FUNCTION_BUTTON button briefly (<
    // FACTORY_RESET_TRIGGER_TIMEOUT) To initiate factory reset: press the
    // APP_FUNCTION_BUTTON for FACTORY_RESET_TRIGGER_TIMEOUT +
    // FACTORY_RESET_CANCEL_WINDOW_TIMEOUT All LEDs start blinking after
    // FACTORY_RESET_TRIGGER_TIMEOUT to signal factory reset has been initiated.
    // To cancel factory reset: release the APP_FUNCTION_BUTTON once all LEDs
    // start blinking within the FACTORY_RESET_CANCEL_WINDOW_TIMEOUT
    if (aEvent->ButtonEvent.mAction == APP_BUTTON_PRESSED)
    {
        if (!sAppTask.mFunctionTimerActive && sAppTask.mFunction == kFunction_NoneSelected)
        {
            sAppTask.StartTimer(FACTORY_RESET_TRIGGER_TIMEOUT);
            sAppTask.mFunction = kFunction_StartBleAdv;
        }
    }
    else
    {
        // If the button was released before factory reset got initiated, start BLE advertissement in fast mode
        if (sAppTask.mFunctionTimerActive && sAppTask.mFunction == kFunction_StartBleAdv)
        {
            sAppTask.CancelTimer();
            sAppTask.mFunction = kFunction_NoneSelected;

            if (!ConnectivityMgr().IsWiFiStationProvisioned())
            {
            	chip::DeviceLayer::PlatformMgr().LockChipStack();
                // Enable BLE advertisements
                ConnectivityMgr().SetBLEAdvertisingEnabled(true);
                ConnectivityMgr().SetBLEAdvertisingMode(ConnectivityMgr().kFastAdvertising);
				chip::DeviceLayer::PlatformMgr().UnlockChipStack();
            }
            else
            {
                ESP_LOGI(TAG, "Network is already provisioned, Ble advertissement not enabled");
            }
        }
        else if (sAppTask.mFunctionTimerActive && sAppTask.mFunction == kFunction_FactoryReset)
        {
            sAppTask.CancelTimer();

            // Change the function to none selected since factory reset has been
            // canceled.
            sAppTask.mFunction = kFunction_NoneSelected;			
			sRedLED.Set(true);
            ESP_LOGI(TAG, "Factory Reset has been Canceled");
        }
    }
}

void AppTask::CancelTimer()
{
    if (xTimerStop(sFunctionTimer, 0) == pdFAIL)
    {
        ESP_LOGI(TAG, "app timer stop() failed");
        return;
    }

    mFunctionTimerActive = false;
}
void AppTask::StartTimer(uint32_t aTimeoutInMs)
{
    if (xTimerIsTimerActive(sFunctionTimer))
    {
        ESP_LOGI(TAG, "app timer already started!");
        CancelTimer();
    }

    // timer is not active, change its period to required value (== restart).
    // FreeRTOS- Block for a maximum of 100 ticks if the change period command
    // cannot immediately be sent to the timer command queue.
    if (xTimerChangePeriod(sFunctionTimer, aTimeoutInMs / portTICK_PERIOD_MS, 100) != pdPASS)
    {
        ESP_LOGI(TAG, "app timer start() failed");
        return;
    }

    mFunctionTimerActive = true;
}


void AppTask::LockActionEventHandler(AppEvent * aEvent)
{
   unsigned int Length;
   unsigned char *buffer;
   uint8_t bda[6];
   ESP_LOGI(TAG, "LockActionEventHandler, lock endpoint: 0x%x, lock status: 0x%x", aEvent->LockEvent.EndpointID,aEvent->LockEvent.Action);
   uint64_t timestamp = get_timestamp_us()/1000;
   
   ESP_LOGI(TAG, "MasterCode：");
   esp_log_buffer_hex(TAG, (const unsigned char *)MasterCode, MASTER_CODE_LEN);
   ESP_LOGI(TAG, "deviceUUID:");
   esp_log_buffer_hex(TAG, (const unsigned char *)deviceUUID, DEVICE_UUID_LEN);
   ESP_LOGI(TAG, "Password:");
   esp_log_buffer_hex(TAG, (const unsigned char *)Password, sizeof(Password));
   ESP_LOGI(TAG, "timestamp: %"PRIX64" ", timestamp);
   ESP_LOGI(TAG, "LockEvent.Action: %d", aEvent->LockEvent.Action);   
   ESP_LOGI(TAG, "PGCCmdSrc_alexa: %d", PGCCmdSrc_alexa);

   updatePgAesKey((const unsigned char *)MasterCode,MASTER_CODE_LEN,(const unsigned char *)deviceUUID,(DEVICE_UUID_LEN * 2));
   buffer = encodeOpenCloseCmd_Test(&Length,
									   2,
									   (unsigned char *)Password,
									   6,
									   0,
									   aEvent->LockEvent.Action,
									   PGCCmdSrc_alexa,
									   PGCEnType_alexa,
									   (unsigned char *)&timestamp);
   bda[0] = 0x58;
   bda[1] = 0x7a;
   bda[2] = 0x62;
   bda[3] = 0x11;
   bda[4] = 0x26;
   bda[5] = 0xa5;
   uart_sent_ble_data((esp_bd_addr_t *)bda, buffer, Length, 10000,10000);
   free(buffer);

}

void AppTask::ServiceInitActionEventHandler(AppEvent * aEvent)
{
   ESP_LOGI(TAG, "ServiceInitActionEventHandler, ServiceType: 0x%x", aEvent->ServiceInitEvent.ServiceType);
   app_mqtt_init();
   app_sntp_init();
   	if (ConnectivityMgr().IsWiFiStationProvisioned()){
        sRedLED.Set(true);
        sGreenLED.Set(false);
		}

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
        BaseType_t status;
        if (xPortInIsrContext())
        {
            BaseType_t higherPrioTaskWoken = pdFALSE;
            status                         = xQueueSendFromISR(sAppEventQueue, aEvent, &higherPrioTaskWoken);
        }
        else
        {
            status = xQueueSend(sAppEventQueue, aEvent, (TickType_t)1);
        }
        if (!status)
        	{
            	ESP_LOGE(TAG, "Failed to post event to app task event queue");
				return ESP_FAIL;
        	}
		else
			return ESP_OK;
    }
    else
    {
        ESP_LOGE(TAG, "Event Queue is NULL should never happen");
		return ESP_FAIL;
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


