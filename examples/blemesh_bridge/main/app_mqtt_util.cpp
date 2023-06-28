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
#include "app_uart_util.h"
#include "app_mqtt_util.h"
#include "AppTask.h"
//#include "c_base64.h"


MqttData_t sMqttData;

static const char * const TAG = "mqtt-util";

void MqttSendCommandResponse(MqttData_t * MqttData)
{
	cJSON *root = cJSON_CreateObject();
	cJSON *header = cJSON_CreateObject();
	cJSON *payload = cJSON_CreateObject();
	cJSON_AddStringToObject(header, "namespace", MqttData->Header.nameSpace);
	if(strcmp(MqttData->Header.name,"lockCommandRequest")==0){
		cJSON_AddStringToObject(header, "name", "lockCommandResponse");
		cJSON_AddNumberToObject(payload, "code", 0);
		cJSON_AddStringToObject(payload, "errorMessage", "");		
		cJSON_AddStringToObject(payload, "commandContent", "");
		}
	else if(strcmp(MqttData->Header.name,"ping")==0){
		cJSON_AddStringToObject(header, "name", "pong");
		}
	cJSON_AddStringToObject(header, "requestId", MqttData->Header.requestId);
	cJSON_AddStringToObject(header, "timestamp", MqttData->Header.timeStamp);	
	cJSON_AddItemToObject(root, "header", header);
	cJSON_AddItemToObject(root, "payload", payload);
	char* buf = cJSON_Print(root);
	int msg_id = esp_mqtt_client_publish(client, "server", buf, 0, 0, 0);
    ESP_LOGI(TAG, "sent publish successful, msg_id=%d ,data=%s", msg_id,buf);
	cJSON_free((void *) buf);
	cJSON_Delete(root);
}

void MqttCommandHandler(MqttData_t * MqttData)
{
	if(strcmp(MqttData->Header.name,"lockCommandRequest")==0){
		ESP_LOGI(TAG, "lockCommandRequest");
		if(strcmp(MqttData->lockCommandRequest.commandName,"forward")==0){
			size_t out_len;
			//app_uart_send((const char*)base64_decode(MqttData->lockCommandRequest.commandContent,strlen(MqttData->lockCommandRequest.commandContent),&out_len),out_len);
			app_uart_send((const char*)MqttData->lockCommandRequest.commandContent,strlen(MqttData->lockCommandRequest.commandContent));
			}
		}

    MqttSendCommandResponse(MqttData);

}

void MqttEventHandler(AppEvent * aEvent)
{
	MqttData_t * pMqttData;
	pMqttData = &sMqttData;
	if(aEvent->MqttEvent.buf){
		ESP_LOGI(TAG, "MQTT Data=%s", aEvent->MqttEvent.buf);
		cJSON *pJsonRoot = cJSON_Parse((const char*)aEvent->MqttEvent.buf);
		if(pJsonRoot){
			cJSON *pHeader = cJSON_GetObjectItem(pJsonRoot, "header"); 
			if(pHeader){
				cJSON *pNamespace = cJSON_GetObjectItem(pHeader, "namespace");
				strcpy(pMqttData->Header.nameSpace, pNamespace->valuestring);
				cJSON *pName = cJSON_GetObjectItem(pHeader, "name");
				strcpy(pMqttData->Header.name, pName->valuestring);
				cJSON *pRequestId = cJSON_GetObjectItem(pHeader, "requestId");
				strcpy(pMqttData->Header.requestId, pRequestId->valuestring);	
				cJSON *pTimeStamp = cJSON_GetObjectItem(pHeader, "timestamp");
				strcpy(pMqttData->Header.timeStamp, pTimeStamp->valuestring);	
				}
			else{
				ESP_LOGE(TAG, "MQTT Data no header error");}
			
			cJSON *pPayload = cJSON_GetObjectItem(pJsonRoot, "payload"); 
			if(pPayload){
				if(strcmp(pMqttData->Header.name,"lockCommandRequest")==0){
					cJSON *pDeviceId = cJSON_GetObjectItem(pPayload, "deviceId");
					strcpy(pMqttData->lockCommandRequest.deviceId, pDeviceId->valuestring);
					cJSON *pCommandName = cJSON_GetObjectItem(pPayload, "commandName");
					strcpy(pMqttData->lockCommandRequest.commandName, pCommandName->valuestring);
					cJSON *pCommandContent = cJSON_GetObjectItem(pPayload, "commandContent");
					strcpy(pMqttData->lockCommandRequest.commandContent, pCommandContent->valuestring);
					}
				else if(strcmp(pMqttData->Header.name,"ping")==0){
					cJSON *pDeviceId = cJSON_GetObjectItem(pPayload, "deviceId");
					strcpy(pMqttData->ping.deviceId, pDeviceId->valuestring);
					}
				}else{
				ESP_LOGE(TAG, "MQTT Data no payload error");}
				
			MqttCommandHandler(pMqttData);
			cJSON_Delete(pJsonRoot);
			}
		vPortFree(aEvent->MqttEvent.buf);
		aEvent->MqttEvent.buf = NULL;
	}
}


