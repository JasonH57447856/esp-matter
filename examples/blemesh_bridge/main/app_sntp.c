/* LwIP SNTP example

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/
#include <string.h>
#include <time.h>
#include <sys/time.h>
#include "esp_log.h"
#include "esp_attr.h"
#include "esp_sntp.h"
#include "app_mqtt.h"
#include "esp_netif_sntp.h"

static const char *TAG = "sntp";

#ifndef INET6_ADDRSTRLEN
#define INET6_ADDRSTRLEN 48
#endif


#define CONFIG_SNTP_TIME_SERVER "pool.ntp.org"

/* Variable holding number of times ESP32 restarted since first boot.
 * It is placed into RTC memory using RTC_DATA_ATTR and
 * maintains its value when ESP32 wakes from deep sleep.
 */

static void initialize_sntp(void);

void time_sync_notification_cb(struct timeval *tv)
{
	ESP_LOGI(TAG, "Notification of a time synchronization event");
	app_mqtt_init();

/*   time_t now;
    struct tm timeinfo;

    ESP_LOGI(TAG, "Notification of a time synchronization event");
	time(&now);

    char strftime_buf[64];

    // Set timezone to Eastern Standard Time and print local time
    setenv("TZ", "EST5EDT,M3.2.0/2,M11.1.0", 1);
    tzset();
    localtime_r(&now, &timeinfo);
    strftime(strftime_buf, sizeof(strftime_buf), "%c", &timeinfo);
    ESP_LOGI(TAG, "The current date/time in New York is: %s", strftime_buf);

    // Set timezone to China Standard Time
    setenv("TZ", "CST-8", 1);
    tzset();
    localtime_r(&now, &timeinfo);
    strftime(strftime_buf, sizeof(strftime_buf), "%c", &timeinfo);
    ESP_LOGI(TAG, "The current date/time in Shanghai is: %s", strftime_buf);*/
    
}

void initialize_sntp(void)
{
    ESP_LOGI(TAG, "Initializing SNTP");
    esp_sntp_config_t config = ESP_NETIF_SNTP_DEFAULT_CONFIG_MULTIPLE(2,
                               ESP_SNTP_SERVER_LIST("cn.pool.ntp.org","pool.ntp.org") );	
    esp_netif_sntp_init(&config);
}

static esp_err_t obtain_time(void)
{
    // wait for time to be set
    int retry = 0;
    const int retry_count = 10;
    while (esp_netif_sntp_sync_wait(pdMS_TO_TICKS(2000)) != ESP_OK && ++retry < retry_count) {
        ESP_LOGI(TAG, "Waiting for system time to be set... (%d/%d)", retry, retry_count);
    }
    if (retry == retry_count) {
        return ESP_FAIL;
    }
    return ESP_OK;
}

void app_sntp_init(void)
{
    initialize_sntp();	
    sntp_set_time_sync_notification_cb(time_sync_notification_cb);
	//obtain_time();
}

uint64_t get_timestamp_ms(void)
{
	struct timeval tv_now;
	uint64_t time_us; 
	gettimeofday(&tv_now, NULL);	
	time_us	= (uint64_t)tv_now.tv_sec * 1000000L + (uint64_t)tv_now.tv_usec;
	return time_us/1000;
}



