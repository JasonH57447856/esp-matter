/*
   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/

#pragma once

#include "AppTask.h"

#ifdef __cplusplus
extern "C" {
#endif
#include "app_mqtt.h"

void MqttEventHandler(AppEvent * aEvent);




#ifdef __cplusplus
}
#endif
