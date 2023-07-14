/*
 * uart_util.h
 *
 *  Created on: 2018Äê12ÔÂ21ÈÕ
 *      Author: tony
 */

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include "driver/uart.h"
//#include "ble_util.h"
typedef uint8_t esp_bd_addr_t[6];


#define UART_CMD_SCAN		(1)
#define UART_CMD_SEND		(2)
#define UART_CMD_CLOSE		(3)
#define UART_CMD_RESET		(4)
#define UART_CMD_INIT		(5)
#define UART_CMD_ECHO		(8)
#define UART_CMD_ERR_ACK	(9)

#define MIN_SEND_CMD_LEN    (15)
#define MIN_CLOSE_CMD_LEN	(13)
#define MIN_RESET_CMD_LEN	(7)
#define MIN_SCAN_CMD_LEN	(11)


#define BUF_LINE_SIZE 	(1024)

#define UART_BUF_SIZE 		(2048)

#define CMD_MODE_POS		(6)

#define PACKET_LEN_POS			(4)
#define HEAD_FIELD_LEN			(4)
#define PACKET_LEN_FIELD_LEN	(2)
#define CMD_TYPE_FIELD_LEN		(1)
#define CMD_RET_FIELD_LEN		(1)
#define BLE_SEND_RET_POS		(HEAD_FIELD_LEN + PACKET_LEN_FIELD_LEN + CMD_TYPE_FIELD_LEN)
#define BLE_SEND_ACK_POS		(BLE_SEND_RET_POS + CMD_RET_FIELD_LEN)

#define BLE_CLOSE_RET_POS		BLE_SEND_RET_POS

#define BLE_SCAN_CNT_LEN		(1)
#define BLE_SCAN_RET_POS		(HEAD_FIELD_LEN + PACKET_LEN_FIELD_LEN + CMD_TYPE_FIELD_LEN)
#define BLE_SCAN_CNT_POS		(BLE_SCAN_RET_POS + CMD_RET_FIELD_LEN)
#define BLE_SCAN_DATA_POS       (BLE_SCAN_CNT_POS + BLE_SCAN_CNT_LEN)
#define BLE_SCAN_BDA_LEN        (6)
#define BLE_SCAN_RSSI_LEN       (4)
#define BLE_SCAN_Name_POS		(BLE_SCAN_DATA_POS+BLE_SCAN_BDA_LEN+BLE_SCAN_RSSI_LEN)
#define BLE_SCAN_NAME_LEN		(40)
#define BLE_SCAN_DATA_LEN       (BLE_SCAN_BDA_LEN + BLE_SCAN_RSSI_LEN + BLE_SCAN_NAME_LEN)


extern uint8_t CMD_HEAD[];

void uart_scan_ble_device(uint16_t timeout, uint8_t cycle, char* target_name, uint8_t name_len);
void uart_sent_ble_data(esp_bd_addr_t *bda, uint8_t *data, uint16_t data_len, uint16_t timeout, uint16_t max_idle_time);
void uart_close_ble_conn(esp_bd_addr_t *bda);
void uart_reset_ble();
void uart_check_ble_status();

//bool uart_read_data(ble_cb_param_pt cmd, uint16_t timeout);
void uart_flush_read_buf();
#ifdef __cplusplus
}
#endif

