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
#include "AppTask.h"
#include "driver/uart.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "freertos/queue.h"
#include "freertos/timers.h"

#define UART_SEND_TIMEOUT_MS 300

#define UART_SEND_TASK_PRIORITY           13
#define UART_SEND_EVENT_QUEUE_SIZE		  20


#define UART_ACK


static const char * const TAG = "uart-util";

BaseType_t sUartSendTaskHandle;
QueueHandle_t sUartSendEventQueue;
SemaphoreHandle_t sUartSendSemaphore = NULL;
TimerHandle_t sUartSendTimeoutTimer;

uint8_t SendingBuffer[TX_BUF_SIZE];
uint32_t SendingBufferLen;


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

void UartReceiveEventHandler(AppEvent * aEvent)
{
	if(aEvent->UartEvent.buf){	
		ESP_LOGI(TAG, "UART Data=%s", aEvent->UartEvent.buf);
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
			if(xQueueSend(sUartSendEventQueue, &event, (TickType_t)0)==ESP_FAIL){
				 vPortFree(buffer);
			     buffer = NULL;
				}
		}else{
			ESP_LOGE(TAG, " uart send pvPortMalloc error");				    
		}
   }
}

void app_uart_resend(const void *src, size_t size)
{
	
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
			if(xQueueSendToFront(sUartSendEventQueue, &event, (TickType_t)0)==ESP_FAIL){
				 vPortFree(buffer);
			     buffer = NULL;
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

esp_err_t uart_send_task_init(void)
{
	sUartSendEventQueue = xQueueCreate(UART_SEND_EVENT_QUEUE_SIZE, sizeof(UartSendEvent));
    if (sUartSendEventQueue == NULL)
    {
        ESP_LOGE(TAG, "Failed to allocate uart send event queue");
        return ESP_FAIL;
    }
    sUartSendSemaphore = xSemaphoreCreateBinary();
    if (sUartSendSemaphore == NULL)
    {
        ESP_LOGE(TAG, "Failed to create UartSendSemaphore");
    }

	sUartSendTimeoutTimer = xTimerCreate("UartSendTimeoutTimer", pdMS_TO_TICKS(UART_SEND_TIMEOUT_MS), pdFALSE, NULL, uart_timeout_callback);
	

    sUartSendTaskHandle = xTaskCreate(uart_send_task, "uart_send_task", 2048, NULL, UART_SEND_TASK_PRIORITY, NULL);
    return sUartSendTaskHandle ? ESP_OK : ESP_FAIL;
}


