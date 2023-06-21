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

void app_mqtt_init(void);
void app_mqtt_process(esp_mqtt_event_handle_t event_data);


#ifdef __cplusplus
}
#endif
