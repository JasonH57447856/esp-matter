/* MQTT (over TCP) Example

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/

#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include "esp_log.h"
#include "mqtt_client.h"
#include "cJSON.h"
#include "app_uart.h"
#include "app_uart_util.h"
#include "uart_util.h"
#include "AppTask.h"
#include "driver/uart.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "freertos/queue.h"
#include "freertos/timers.h"
#include "esp_sntp.h"
#include <esp_matter.h>
#include <esp_matter_core.h>
#include <esp_matter_bridge.h>

#include <app_bridged_device.h>
#include <blemesh_bridge.h>
#include "app_mqtt_util.h"

#define UART_SEND_TIMEOUT_MS 300

#define UART_SEND_TASK_PRIORITY           13
#define UART_SEND_EVENT_QUEUE_SIZE		  20


//#define UART_ACK


static const char * const TAG = "app-uart-util";

BaseType_t sUartSendTaskHandle;
QueueHandle_t sUartSendEventQueue;
#ifdef UART_ACK
SemaphoreHandle_t sUartSendSemaphore = NULL;
TimerHandle_t sUartSendTimeoutTimer;
#endif


uint8_t SendingBuffer[TX_BUF_SIZE];
uint32_t SendingBufferLen;

#ifdef UART_ACK
void CancelUartTimer(void)
{
	if (xTimerStop(sUartSendTimeoutTimer, 0) == pdFAIL)
    {
    	ESP_LOGI(TAG, "uart timer stop() failed");
	}
}

void StartUartTimer(uint32_t aTimeoutInMs)
{
	if (xTimerIsTimerActive(sUartSendTimeoutTimer))
    {
    	ESP_LOGI(TAG, "uart timer already started!");
        CancelUartTimer();
	}
    // timer is not active, change its period to required value (== restart).
    // FreeRTOS- Block for a maximum of 100 ticks if the change period command
    // cannot immediately be sent to the timer command queue.
    if (xTimerChangePeriod(sUartSendTimeoutTimer, pdMS_TO_TICKS(aTimeoutInMs), pdMS_TO_TICKS(100)) != pdPASS)
    {
    	ESP_LOGI(TAG, "uart timer start() failed");
	}
}

void app_uart_resend(const void *src, size_t size)
{
		BaseType_t status;
		ESP_LOGI(TAG, "app_uart_send: %d", size);
		//uart_write_bytes(EX_UART_NUM, src, size);
	
		if (sUartSendEventQueue != NULL){
			UartSendEvent event;
			event.Type				= UartSendEvent::kEventType_Data;
			event.len  = (uint32_t)size;
			uint8_t* buffer = (uint8_t*) pvPortMalloc(event.len);
			if(buffer){
				memcpy(buffer, (uint8_t*)src, event.len);
				event.buf = buffer;			
			if (xPortInIsrContext())
			{
				BaseType_t higherPrioTaskWoken = pdFALSE;
				status						   = xQueueSendToFrontFromISR(sUartSendEventQueue, &event, &higherPrioTaskWoken);
			}
			else
			{
				status = xQueueSendToFront(sUartSendEventQueue, &event, (TickType_t)1);
			}
			if (!status)
				{
					ESP_LOGE(TAG, "Failed to post event to uart send event queue");
				}
			
			}else{
				ESP_LOGE(TAG, " uart send pvPortMalloc error"); 				
			}
	   }

}

static void uart_timeout_callback(TimerHandle_t xTimer)
{
	static uint8_t retry_count = 0;
	retry_count++;
	if(retry_count<3){
		app_uart_resend(SendingBuffer,SendingBufferLen);
		}
	else{
		retry_count=0;
		}	
	xSemaphoreGive(sUartSendSemaphore);
}

#endif

void UartReceiveEventHandler(AppEvent * aEvent)
{
	if(aEvent->UartEvent.buf){	
		ESP_LOGI(TAG, "UartReceiveEventHandler");
		esp_log_buffer_hex(TAG, aEvent->UartEvent.buf, aEvent->UartEvent.len);
		if(!memcmp(aEvent->UartEvent.buf,CMD_HEAD,HEAD_FIELD_LEN)){
			switch (aEvent->UartEvent.buf[CMD_MODE_POS])
			{
			case UART_CMD_SCAN:
				 uint8_t device_cnt = aEvent->UartEvent.buf[BLE_SCAN_CNT_POS];				 
				 uint8_t mac_addr[6]={0};
				 if(device_cnt > 0){
					app_bridged_device_info_t bridged_device_info;			
					memcpy(mac_addr,&aEvent->UartEvent.buf[BLE_SCAN_DATA_POS],BLE_SCAN_BDA_LEN);
					strcpy((char *)bridged_device_info.master_code, (const char *)&sMqttDataCache.bindHubAndLockRequest.masterCode);	
					strcpy((char *)bridged_device_info.password, (const char *)&sMqttDataCache.bindHubAndLockRequest.hostCode);
					strcpy((char *)bridged_device_info.device_uuid, (const char *)&sMqttDataCache.bindHubAndLockRequest.deviceId);
					blemesh_bridge_match_bridged_door_lock(mac_addr,bridged_device_info);
					sMqttDataCache.status = ESP_OK;					
				 }
				 else{
				 	sMqttDataCache.status = ESP_FAIL;	
				 }
			 	mac_bin2str((char *)mac_addr,(char *)&sMqttDataCache.bindHubAndLockRequest.macAddr, 18);
				if(sMqttDataCache.bindHubAndLockRequest.cacheActive==true){
					if(strcmp((const char *)&sMqttDataCache.bindHubAndLockRequest.deviceName[0],(const char *)&aEvent->UartEvent.buf[BLE_SCAN_Name_POS])==0)
						MqttSendCommandResponse(&sMqttDataCache);
				}
				memset(&sMqttDataCache, 0, sizeof(sMqttDataCache));
			 break;
			}
		}
		else{
			ESP_LOGE(TAG, " Received Unexpected Data"); 
		}
			
#ifdef UART_ACK
		xSemaphoreGive(sUartSendSemaphore);
		CancelUartTimer();
#endif		
		vPortFree(aEvent->UartEvent.buf);
		aEvent->UartEvent.buf = NULL;
	}
}

void app_uart_send(const void *src, size_t size)
{
	BaseType_t status;
	ESP_LOGI(TAG, "app_uart_send: %d", size);
	//uart_write_bytes(EX_UART_NUM, src, size);

    if (sUartSendEventQueue != NULL){
		UartSendEvent event;
	    event.Type              = UartSendEvent::kEventType_Data;
	    event.len  = (uint32_t)size;
		uint8_t* buffer = (uint8_t*) pvPortMalloc(event.len);
		if(buffer){
			memcpy(buffer, (uint8_t*)src, event.len);
			event.buf = buffer;
			
        if (xPortInIsrContext())
        {
            BaseType_t higherPrioTaskWoken = pdFALSE;
            status                         = xQueueSendFromISR(sUartSendEventQueue, &event, &higherPrioTaskWoken);
        }
        else
        {
            status = xQueueSend(sUartSendEventQueue, &event, (TickType_t)1);
        }
        if (!status)
        	{
            	ESP_LOGE(TAG, "Failed to post event to uart send event queue");
        	}
		
		}else{
			ESP_LOGE(TAG, " uart send pvPortMalloc error");				    
		}
   }
}

static void uart_send_task(void * pvParameter)
{
    UartSendEvent event;

    ESP_LOGI(TAG, "Uart Send Task started");

    while (true)
    {
        BaseType_t eventReceived = xQueueReceive(sUartSendEventQueue, &event, portMAX_DELAY);
        while (eventReceived == pdTRUE)
        {        
			esp_log_buffer_hex(TAG, event.buf, event.len);
            uart_write_bytes(EX_UART_NUM, event.buf, event.len);
			SendingBufferLen = event.len;			
			memcpy(SendingBuffer, event.buf, event.len);
			vPortFree(event.buf);
			event.buf = NULL;
#ifdef UART_ACK
			StartUartTimer(UART_SEND_TIMEOUT_MS);
			xSemaphoreTake(sUartSendSemaphore, portMAX_DELAY);
#endif
            eventReceived = xQueueReceive(sUartSendEventQueue, &event, 0);
        }

    }
}

esp_err_t uart_send_task_init(void)
{
	sUartSendEventQueue = xQueueCreate(UART_SEND_EVENT_QUEUE_SIZE, sizeof(UartSendEvent));
    if (sUartSendEventQueue == NULL)
    {
        ESP_LOGE(TAG, "Failed to allocate uart send event queue");
        return ESP_FAIL;
    }
#ifdef UART_ACK	
    sUartSendSemaphore = xSemaphoreCreateBinary();
    if (sUartSendSemaphore == NULL)
    {
        ESP_LOGE(TAG, "Failed to create UartSendSemaphore");
    }

	sUartSendTimeoutTimer = xTimerCreate("UartSendTimeoutTimer", pdMS_TO_TICKS(UART_SEND_TIMEOUT_MS), pdFALSE, NULL, uart_timeout_callback);
#endif	

    sUartSendTaskHandle = xTaskCreate(uart_send_task, "uart_send_task", 2048, NULL, UART_SEND_TASK_PRIORITY, NULL);
    return sUartSendTaskHandle ? ESP_OK : ESP_FAIL;
}


