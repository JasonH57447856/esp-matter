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

static const char * const TAG = "uart-util";



void UartEventHandler(AppEvent * aEvent)
{
	if(aEvent->UartEvent.buf){	
		ESP_LOGI(TAG, "UART Data=%s", aEvent->UartEvent.buf);
		
		vPortFree(aEvent->UartEvent.buf);
		aEvent->UartEvent.buf = NULL;
	}
}



