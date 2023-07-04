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
#include <stdio.h>
#include <string.h>
#include "driver/gpio.h"

#define EX_UART_NUM UART_NUM_2
#define TXD2_PIN 			(GPIO_NUM_17)
#define RXD2_PIN 			(GPIO_NUM_16)


#define PATTERN_CHR_NUM    (3)         /*!< Set the number of consecutive and identical characters received by receiver which defines a UART pattern*/

#define BUF_SIZE (1024)
#define RD_BUF_SIZE (BUF_SIZE)
#define TX_BUF_SIZE (BUF_SIZE)

void app_uart_init(void);
void app_uart_process(uint8_t *buf, uint32_t length);


#ifdef __cplusplus
}
#endif
