/*
   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include "mqtt_client.h"

#define MAX_Header_LENGTH 		(64)
#define MAX_Payload_LENGTH 		(256)


typedef struct MqttHeader
{
	char nameSpace[MAX_Header_LENGTH];
	char name[MAX_Header_LENGTH];
	char requestId[MAX_Header_LENGTH];
	char timeStamp[MAX_Header_LENGTH];	
}MqttHeader_t;

typedef struct MqttData
{
    MqttHeader_t Header;
    //MqttPayload_t Payload;
    union 
	{
		struct
		{
			char deviceId[MAX_Header_LENGTH];
		}ping;
		struct
		{
			char deviceId[MAX_Header_LENGTH];
			char commandName[MAX_Header_LENGTH];
			char commandContent[MAX_Payload_LENGTH];
		}lockCommandRequest;	
	};
}MqttData_t;

void app_mqtt_init(void);
void app_mqtt_process(esp_mqtt_event_handle_t event_data);
extern esp_mqtt_client_handle_t client;


#ifdef __cplusplus
}
#endif
