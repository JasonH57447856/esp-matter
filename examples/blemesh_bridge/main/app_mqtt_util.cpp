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
#include "esp_sntp.h"
#include <esp_matter.h>
#include <esp_matter_core.h>
#include <esp_matter_bridge.h>

#include <app_bridged_device.h>
#include <blemesh_bridge.h>
#include "esp_mac.h"
#include <platform/ConfigurationManager.h>
#include "uart_util.h"
#include "lockly_c_header.h"

using namespace chip::DeviceLayer;

MqttData_t sMqttData;
MqttData_t sMqttDataCache;

static const char * const TAG = "mqtt-util";

int mac_bin2str(const char *bin, char *str, size_t max_len)
{
	int len;
	len = snprintf(str, max_len, "%02x:%02x:%02x:%02x:%02x:%02x:%02x:%02x:"
			  "%02x:%02x:%02x:%02x:%02x:%02x:%02x:%02x",
			  bin[0], bin[1], bin[2], bin[3],
			  bin[4], bin[5], bin[6], bin[7],
			  bin[8], bin[9], bin[10], bin[11],
			  bin[12], bin[13], bin[14], bin[15]);
	if (len < 0 || (size_t) len >= max_len)
		return -1;
	return 0;

}


int uuid_bin2str(const char *bin, char *str, size_t max_len)
{
	int len;
	len = snprintf(str, max_len, "%02x%02x%02x%02x-%02x%02x-%02x%02x-"
			  "%02x%02x-%02x%02x%02x%02x%02x%02x",
			  bin[0], bin[1], bin[2], bin[3],
			  bin[4], bin[5], bin[6], bin[7],
			  bin[8], bin[9], bin[10], bin[11],
			  bin[12], bin[13], bin[14], bin[15]);
	if (len < 0 || (size_t) len >= max_len)
		return -1;
	return 0;
}

void get_uuid_str(char * uuid)
{
	char buf[32];
	esp_fill_random(buf,sizeof(buf));
	uuid_bin2str(buf,uuid,64);

}


void MqttSendCommandResponse(MqttData_t * MqttData)
{   
	cJSON *root = cJSON_CreateObject();
	cJSON *header = cJSON_CreateObject();
	cJSON *payload = cJSON_CreateObject();
	cJSON_AddStringToObject(header, "namespace", MqttData->Header.nameSpace);
	if(strcmp(MqttData->Header.name,"lockCommandRequest")==0){
		cJSON_AddStringToObject(header, "name","lockCommandResponse");
		cJSON_AddNumberToObject(payload, "code",0);
		cJSON_AddStringToObject(payload, "errorMessage","");		
		cJSON_AddStringToObject(payload, "commandContent","");
		}
	else if(strcmp(MqttData->Header.name,"ping")==0){
		cJSON_AddStringToObject(header, "name", "pong");
		}
	else if(strcmp(MqttData->Header.name,"unbindHubAndLockRequest")==0){
		cJSON_AddStringToObject(header, "name", "unbindHubAndLockResponse");
		if(MqttData->status==ESP_OK)
			cJSON_AddStringToObject(payload, "status","success");
		else
			cJSON_AddStringToObject(payload, "status","fail");
		}	
	else if(strcmp(MqttData->Header.name,"updateMasterCodeRequest")==0){
		cJSON_AddStringToObject(header, "name", "updateMasterCodeResponse");		
		if(MqttData->status==ESP_OK)
			cJSON_AddStringToObject(payload, "status","success");
		else
			cJSON_AddStringToObject(payload, "status","fail");
		}	
	else if(strcmp(MqttData->Header.name,"bindHubAndLockRequest")==0){
		cJSON_AddStringToObject(header, "name", "bindHubAndLockResponse");		
		if(MqttData->status==ESP_OK)
			cJSON_AddStringToObject(payload, "status","success");
		else{
			cJSON_AddStringToObject(payload, "status","fail");
		}	
			cJSON_AddStringToObject(payload, "deviceId",MqttData->bindHubAndLockRequest.deviceId);		
			cJSON_AddStringToObject(payload, "macAddr",MqttData->bindHubAndLockRequest.macAddr);	
		}
	else if(strcmp(MqttData->Header.name,"bindHubRequest")==0){
			cJSON_AddStringToObject(header, "name","bindHubResponse");
			cJSON_AddStringToObject(payload, "status","success");
			cJSON_AddStringToObject(payload, "deviceId",MqttData->bindHubRequest.deviceId);	
			cJSON_AddStringToObject(payload, "userId",MqttData->bindHubRequest.userId);	
			char softwareVersionString[DeviceLayer::ConfigurationManager::kMaxSoftwareVersionStringLength+1] = { 0 };
			ConfigurationMgr().GetSoftwareVersionString(softwareVersionString, sizeof(softwareVersionString));
			cJSON_AddStringToObject(payload, "fwVersion",(char *)softwareVersionString);		
			uint8_t hub_mac_addr[6],hub_mac_str[20];
			esp_read_mac(hub_mac_addr, ESP_MAC_WIFI_STA);		
			mac_bin2str((char *)hub_mac_addr,(char *)hub_mac_str,18);
			cJSON_AddStringToObject(payload, "macAddr",(char *)hub_mac_str);
		}
	
	//char UUID[64];
	//get_uuid_str(UUID);
	//cJSON_AddStringToObject(header, "requestId", UUID);
	cJSON_AddStringToObject(header, "requestId",MqttData->Header.requestId);
	cJSON_AddNumberToObject(header, "timestamp",get_timestamp_ms());	
	cJSON_AddItemToObject(root, "header",header);
	cJSON_AddItemToObject(root, "payload",payload);
	char* buf = cJSON_Print(root);
	
	cJSON *pName = cJSON_GetObjectItem(header, "name");
	if(pName->valuestring){		
	    ESP_LOGI(TAG, "pName->valuestring=%s", pName->valuestring);
		int msg_id = esp_mqtt_client_publish(client, "server", buf, 0, 0, 0);
	    ESP_LOGI(TAG, "sent publish successful, msg_id=%d ,data=%s", msg_id,buf);
	}
	else{		
		ESP_LOGI(TAG, "Unsuported Mqtt Name");
	}
	cJSON_free((void *) buf);
	cJSON_Delete(root);
}

void MqttCommandHandler(MqttData_t * MqttData)
{
	if(strcmp(MqttData->Header.name,"lockCommandRequest")==0){
		ESP_LOGI(TAG, "lockCommandRequest");
		if(strcmp(MqttData->lockCommandRequest.commandName,"forward")==0){
			size_t out_len;
			app_uart_send((const char*)base64_decode(MqttData->lockCommandRequest.commandContent,strlen(MqttData->lockCommandRequest.commandContent),&out_len),out_len);
			//app_uart_send((const char*)MqttData->lockCommandRequest.commandContent,strlen(MqttData->lockCommandRequest.commandContent));
/*
            #define deviceUUID "250045003437470734383732"
            #define MasterCode "78957036"
            #define Password "111555"
			uint8_t mac_addr[6] = {0x58,0x7a,0x62,0x11,0x26,0xa5};
			app_bridged_device_info_t bridged_device_info;
			strcpy((char *)bridged_device_info.master_code, (const char *)MasterCode);	
			strcpy((char *)bridged_device_info.password, (const char *)Password);
			strcpy((char *)bridged_device_info.device_uuid, (const char *)deviceUUID);
			//bridged_device_info.password_len = strlen(Password);	
			blemesh_bridge_match_bridged_door_lock(mac_addr,bridged_device_info);*/
	
			}
		}
	else if(strcmp(MqttData->Header.name,"bindHubAndLockRequest")==0){
			ESP_LOGI(TAG, "bindHubAndLockRequest");
			uart_scan_ble_device(3000,1,MqttData->bindHubAndLockRequest.deviceName,strlen(MqttData->bindHubAndLockRequest.deviceName));
			sMqttDataCache = sMqttData;
			sMqttDataCache.bindHubAndLockRequest.cacheActive = true;
			return;
		}
	else if(strcmp(MqttData->Header.name,"unbindHubAndLockRequest")==0){
			ESP_LOGI(TAG, "unbindHubAndLockRequest");			
			app_bridged_device_t *device;
			device = app_bridge_get_device_by_device_uuid((uint8_t *)MqttData->unbindHubAndLockRequest.deviceId);
			if(device){
				MqttData->status = app_bridge_remove_device(device);				
			}
			else{
				MqttData->status =ESP_FAIL;
			}
		}
	else if(strcmp(MqttData->Header.name,"updateMasterCodeRequest")==0){
			ESP_LOGI(TAG, "updateMasterCodeRequest");			
			app_bridged_device_t *device;
			device = app_bridge_get_device_by_device_uuid((uint8_t *)MqttData->unbindHubAndLockRequest.deviceId);
			if(device){
				strcpy((char *)device->dev_info.device_uuid, (const char *)MqttData->updateMasterCodeRequest.deviceId);
				strcpy((char *)device->dev_info.master_code, (const char *)MqttData->updateMasterCodeRequest.masterCode);
				strcpy((char *)device->dev_info.password, (const char *)MqttData->updateMasterCodeRequest.hostCode);		
				MqttData->status = app_bridge_store_bridged_device_info(device);
			}
			else
				MqttData->status =ESP_FAIL;
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
				pMqttData->Header.timeStamp = pTimeStamp->valuedouble;	
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
				else if(strcmp(pMqttData->Header.name,"bindHubRequest")==0){
					cJSON *pDeviceId = cJSON_GetObjectItem(pPayload, "deviceId");
					strcpy(pMqttData->bindHubRequest.deviceId, pDeviceId->valuestring);
					cJSON *pUserId = cJSON_GetObjectItem(pPayload, "userId");
					strcpy(pMqttData->bindHubRequest.userId, pUserId->valuestring);
					}
				else if(strcmp(pMqttData->Header.name,"bindHubAndLockRequest")==0){
					cJSON *pDeviceName = cJSON_GetObjectItem(pPayload, "deviceName");
					strcpy(pMqttData->bindHubAndLockRequest.deviceName, pDeviceName->valuestring);
					cJSON *pDeviceId = cJSON_GetObjectItem(pPayload, "deviceId");
					strcpy(pMqttData->bindHubAndLockRequest.deviceId, pDeviceId->valuestring);
					cJSON *pMasterCode = cJSON_GetObjectItem(pPayload, "masterCode");
					strcpy(pMqttData->bindHubAndLockRequest.masterCode, pMasterCode->valuestring);
					cJSON *pHostCode = cJSON_GetObjectItem(pPayload, "hostCode");
					strcpy(pMqttData->bindHubAndLockRequest.hostCode, pHostCode->valuestring);
					}
				else if(strcmp(pMqttData->Header.name,"unbindHubAndLockRequest")==0){
					cJSON *pDeviceId = cJSON_GetObjectItem(pPayload, "deviceId");
					strcpy(pMqttData->unbindHubAndLockRequest.deviceId, pDeviceId->valuestring);
					}	
				else if(strcmp(pMqttData->Header.name,"updateMasterCodeRequest")==0){
					cJSON *pDeviceId = cJSON_GetObjectItem(pPayload, "deviceId");
					strcpy(pMqttData->updateMasterCodeRequest.deviceId, pDeviceId->valuestring);
					cJSON *pMasterCode = cJSON_GetObjectItem(pPayload, "masterCode");
					strcpy(pMqttData->updateMasterCodeRequest.masterCode, pMasterCode->valuestring);
					cJSON *pHostCode = cJSON_GetObjectItem(pPayload, "hostCode");
					strcpy(pMqttData->updateMasterCodeRequest.hostCode, pHostCode->valuestring);
					}				
				}else{
				ESP_LOGE(TAG, "MQTT Data no payload error");}
			if(strcmp(pMqttData->Header.nameSpace,"com.lockly")==0){	
				MqttCommandHandler(pMqttData);
			}
			cJSON_Delete(pJsonRoot);
			}
		else{			
			ESP_LOGI(TAG, "cJSON Parse Failed");
		}
		vPortFree(aEvent->MqttEvent.buf);
		aEvent->MqttEvent.buf = NULL;
	}
}


