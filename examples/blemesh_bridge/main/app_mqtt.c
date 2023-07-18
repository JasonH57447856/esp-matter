/* MQTT (over TCP) Example

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/
#include <stdlib.h>

#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include "esp_wifi.h"
#include "esp_system.h"
#include "nvs_flash.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "app_sntp.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "freertos/queue.h"

#include "lwip/sockets.h"
#include "lwip/dns.h"
#include "lwip/netdb.h"

#include "esp_log.h"
#include "mqtt_client.h"
#include "app_mqtt.h"
#include "mbedtls/aes.h"
#include "mbedtls/base64.h"

#include "lockly_c_header.h"




#ifdef MQTT_TASK_PRIORITY
#undef MQTT_TASK_PRIORITY
#define MQTT_TASK_PRIORITY 11
#else
#define MQTT_TASK_PRIORITY 11
#endif


#define CONFIG_BROKER_URL "mqtt://test.mosquitto.org"

#define CONFIG_HOST_NAME "52.22.176.18"


#define CONFIG_USER_NAME "test07"
#define CONFIG_CLIENT_ID "lc_test07"
#define CONFIG_PASSWORD "pw_Dkenq3jWzsF"

/*

#define CONFIG_CLIENT_ID "th_24yl85siaiv4"
#define CONFIG_USER_NAME "ce_24yl85x83mdc"
#define CONFIG_AES_KEY "iyVbAQHDzqN73cgmOnetqABLVqozzjrjLyKBwxyFaC8="
#define CONFIG_PASSWORD "iyVbAQHDzqN73cgmOnetqABLVqozzjrjLyKBwxyFaC8="
*/



static const char *TAG = "MQTT_TASK";

esp_mqtt_client_handle_t client;
bool IsMqttConnected = 0;
bool is_mqtt_connected(void)
{
	return IsMqttConnected;
}

static void log_error_if_nonzero(const char *message, int error_code)
{
    if (error_code != 0) {
        ESP_LOGE(TAG, "Last error %s: 0x%x", message, error_code);
    }
}
void ui64toa (uint64_t n,uint8_t * s) 
{ 
	uint8_t i=0,j,len,temp; 
	do{ 
	  s[i++]=n%10+'0';
	}while ((n/=10)>0);
	if(s[i-1]==0)
		len= i-1; 
	else
		len= i; 
	for (i = 0; i < len - 1; i++)
	{
		for (j = 0; j < len - 1- i; j++)
		{

				temp = s[j];
				s[j] = s[j + 1];
				s[j + 1] = temp;
		}
	}
	s[len]='\0'; 
}  

uint8_t get_password(uint8_t * client_id, uint8_t * user_name, uint8_t * aes_key, uint8_t * password)
{
	uint8_t plain[128]={0},timestr[16],cipher[128+16],iv[16],iv_buf[16],len,passwd_len;
	mbedtls_aes_context aes_ctx;
	int status;
	uint64_t timestamp = get_timestamp_ms()/1000;		
	ui64toa(timestamp, timestr);
	
	ESP_LOGI(TAG, "timestamp: %" PRIX64 " ",timestamp);
	snprintf((char *)plain, 128, "%s:%s:%s", client_id, user_name, timestr);	
    ESP_LOGI(TAG, "%s ",plain);
	
	esp_fill_random(iv,sizeof(iv));
	 ESP_LOGI(TAG, "iv");
	esp_log_buffer_hex(TAG, iv, 16);
	memcpy(iv_buf,iv,16);
	mbedtls_aes_init(&aes_ctx);
	mbedtls_aes_setkey_enc(&aes_ctx, aes_key, 256);
	len = strlen((const char *)plain);
	if(len%16){
		len = len + 16 - len%16;
	}
	status = mbedtls_aes_crypt_cbc(&aes_ctx, MBEDTLS_AES_ENCRYPT, len, iv, plain, cipher);
	ESP_LOGI(TAG, "mbedtls_aes_crypt_cbc status: %d ",status);
	memcpy(&cipher[len],iv_buf,16);
	ESP_LOGI(TAG, "plain");	
	esp_log_buffer_hex(TAG, plain, len);	
	ESP_LOGI(TAG, "cipher");	
	esp_log_buffer_hex(TAG, cipher, len+16);
	status = mbedtls_base64_encode(password,128, (size_t *)&passwd_len,
                          (const unsigned char *)cipher, len+16);	
	ESP_LOGI(TAG, "mbedtls_base64_encode status: %x ",status);
	ESP_LOGI(TAG, "password :%s ",password);	
	
	mbedtls_aes_free(&aes_ctx);
	return passwd_len;
}
/*
 * @brief Event handler registered to receive MQTT events
 *
 *  This function is called by the MQTT client event loop.
 *
 * @param handler_args user data registered to the event.
 * @param base Event base for the handler(always MQTT Base in this example).
 * @param event_id The id for the received event.
 * @param event_data The data for the event, esp_mqtt_event_handle_t.
 */
static void mqtt_event_handler(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data)
{
    ESP_LOGD(TAG, "Event dispatched from event loop base=%s, event_id=%"PRIX32" ", base, event_id);
    esp_mqtt_event_handle_t event = event_data;
    esp_mqtt_client_handle_t client = event->client;
    int msg_id;
    switch ((esp_mqtt_event_id_t)event_id) {
    case MQTT_EVENT_CONNECTED:
        ESP_LOGI(TAG, "MQTT_EVENT_CONNECTED");
    /*    msg_id = esp_mqtt_client_publish(client, "/topic/qos1", "data_3", 0, 1, 0);
        ESP_LOGI(TAG, "sent publish successful, msg_id=%d", msg_id);

        msg_id = esp_mqtt_client_subscribe(client, "/topic/qos0", 0);
        ESP_LOGI(TAG, "sent subscribe successful, msg_id=%d", msg_id);

        msg_id = esp_mqtt_client_subscribe(client, "/topic/qos1", 1);
        ESP_LOGI(TAG, "sent subscribe successful, msg_id=%d", msg_id);

        msg_id = esp_mqtt_client_unsubscribe(client, "/topic/qos1");
        ESP_LOGI(TAG, "sent unsubscribe successful, msg_id=%d", msg_id);*/
        IsMqttConnected = true;	
        break;
    case MQTT_EVENT_DISCONNECTED:
        ESP_LOGI(TAG, "MQTT_EVENT_DISCONNECTED");		
        IsMqttConnected = false;
        break;

    case MQTT_EVENT_SUBSCRIBED:
        ESP_LOGI(TAG, "MQTT_EVENT_SUBSCRIBED, msg_id=%d", event->msg_id);
        msg_id = esp_mqtt_client_publish(client, "/topic/qos0", "data", 0, 0, 0);
        ESP_LOGI(TAG, "sent publish successful, msg_id=%d", msg_id);
        break;
    case MQTT_EVENT_UNSUBSCRIBED:
        ESP_LOGI(TAG, "MQTT_EVENT_UNSUBSCRIBED, msg_id=%d", event->msg_id);
        break;
    case MQTT_EVENT_PUBLISHED:
        ESP_LOGI(TAG, "MQTT_EVENT_PUBLISHED, msg_id=%d", event->msg_id);
        break;
    case MQTT_EVENT_DATA:
        ESP_LOGI(TAG, "MQTT_EVENT_DATA");	
        //msg_id = esp_mqtt_client_publish(client, "/topic/qos0", "data", 0, 0, 0);
		app_mqtt_process(event);
        break;
    case MQTT_EVENT_ERROR:
        ESP_LOGI(TAG, "MQTT_EVENT_ERROR");
        if (event->error_handle->error_type == MQTT_ERROR_TYPE_TCP_TRANSPORT) {
            log_error_if_nonzero("reported from esp-tls", event->error_handle->esp_tls_last_esp_err);
            log_error_if_nonzero("reported from tls stack", event->error_handle->esp_tls_stack_err);
            log_error_if_nonzero("captured as transport's socket errno",  event->error_handle->esp_transport_sock_errno);
            ESP_LOGI(TAG, "Last errno string (%s)", strerror(event->error_handle->esp_transport_sock_errno));

        }
        break;
    default:
        ESP_LOGI(TAG, "Other event id:%d", event->event_id);
        break;
    }
}

static void mqtt_app_start(void)
{
/*	uint8_t passwd[128]={0},aes_key[128]={0};
	uint8_t passwd_len=0,olen; 
	
	mbedtls_base64_decode(aes_key, 128, (size_t *)&olen,(const unsigned char *)CONFIG_AES_KEY, strlen(CONFIG_AES_KEY));
	ESP_LOGI(TAG, "aes_key ");
	esp_log_buffer_hex(TAG, aes_key, olen);	
	passwd_len = get_password((uint8_t *)CONFIG_CLIENT_ID, (uint8_t *)CONFIG_USER_NAME,aes_key, passwd);
	passwd[passwd_len] = '\0';
	
    ESP_LOGI(TAG, "mqtt_app_start passwd_len:%d, passwd: %s ",passwd_len, passwd);
	esp_mqtt_client_config_t mqtt_cfg = {
        //.broker.address.uri = CONFIG_BROKER_URL,
		//.session.keepalive = 5,
		.broker.address.hostname = CONFIG_HOST_NAME,
		.broker.address.port = 1883,
		.broker.address.transport = MQTT_TRANSPORT_OVER_TCP,
		.credentials = {
		.username = CONFIG_USER_NAME,
		.client_id =CONFIG_CLIENT_ID,
        .authentication = {
        .password = (const char *)passwd
        	}
      },
    };*/
    esp_mqtt_client_config_t mqtt_cfg = {
        //.broker.address.uri = CONFIG_BROKER_URL,
		//.session.keepalive = 5,
		.broker.address.hostname = CONFIG_HOST_NAME,
		.broker.address.port = 1883,
		.broker.address.transport = MQTT_TRANSPORT_OVER_TCP,
		.credentials = {
		.username = CONFIG_USER_NAME,
		.client_id =CONFIG_CLIENT_ID,
        .authentication = {
        .password = CONFIG_PASSWORD
        	}
      },
    };

    client = esp_mqtt_client_init(&mqtt_cfg);
    /* The last argument may be used to pass data to the event handler, in this example mqtt_event_handler */
    esp_mqtt_client_register_event(client, ESP_EVENT_ANY_ID, mqtt_event_handler, NULL);
    esp_mqtt_client_start(client);
}

void app_mqtt_init(void)
{
	static bool initialised = false;
	if(initialised == false)
	{
	    ESP_LOGI(TAG, "[APP] Startup..");
	    ESP_LOGI(TAG, "[APP] Free memory: %"PRIX32" bytes", esp_get_free_heap_size());
	    ESP_LOGI(TAG, "[APP] IDF version: %s", esp_get_idf_version());

	    esp_log_level_set("*", ESP_LOG_INFO);
	    esp_log_level_set("mqtt_client", ESP_LOG_VERBOSE);
	    esp_log_level_set("MQTT_EXAMPLE", ESP_LOG_VERBOSE);
	    esp_log_level_set("TRANSPORT_BASE", ESP_LOG_VERBOSE);
	    esp_log_level_set("esp-tls", ESP_LOG_VERBOSE);
	    esp_log_level_set("TRANSPORT", ESP_LOG_VERBOSE);
	    esp_log_level_set("outbox", ESP_LOG_VERBOSE);

    	mqtt_app_start();
		initialised = true;
	}
}



