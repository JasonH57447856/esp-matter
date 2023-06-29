/*
 * uart_util.c
 *
 *  Created on: 2018Äê12ÔÂ21ÈÕ
 *      Author: tony
 */
#include <stdio.h>
#include <string.h>

#include "esp_log.h"
//#include "esp_bt_defs.h"
#include "uart_util.h"
#include "app_uart_util.h"



#define UART_TAG 		"UART_UTIL"

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
#define BLE_SCAN_NAME_LEN		(40)
#define BLE_SCAN_DATA_LEN       (BLE_SCAN_BDA_LEN + BLE_SCAN_RSSI_LEN + BLE_SCAN_NAME_LEN)

uint8_t CMD_HEAD[4] = {0xf6,0xe5,0xd4,0xc3};

uint8_t data_line[BUF_LINE_SIZE];
typedef struct{
	size_t cmd_len;
    size_t cur_pos;
	uint8_t data[BUF_LINE_SIZE*4];
}uart_cmd_buf_t;
uart_cmd_buf_t input_cmd_buf;

uint8_t output_buf[BUF_LINE_SIZE*2];


typedef union{
	uint16_t value;
	uint8_t byte[2];
} cmd_len_t;

typedef union{
	int value;
	uint8_t byte[6];
} rssi_value_t;

void clear_output_buf(){
	memset(output_buf,0x0, sizeof(output_buf));
}

void add_cmd_head(uint16_t packet_len, uint8_t cmd_type, uint16_t *pos){
	memcpy(output_buf, CMD_HEAD, 4);
	memcpy(output_buf+4, &packet_len,sizeof(packet_len));
	output_buf[CMD_MODE_POS] = cmd_type;
	*pos = CMD_MODE_POS+1;
}

void uart_sent_ble_data(esp_bd_addr_t *bda, uint8_t *data, uint16_t data_len, uint16_t timeout, uint16_t max_idle_time){
	//printf("uart_send_ble:time=%d,max_idle_time=%d\r\n",timeout,max_idle_time);
	uint16_t pos = 0;
    uint16_t packet_len = CMD_MODE_POS + 1 +  sizeof(esp_bd_addr_t) + sizeof(timeout)+sizeof(max_idle_time) + data_len;
    add_cmd_head(packet_len,UART_CMD_SEND,&pos);
    memcpy(output_buf+pos,bda,sizeof(esp_bd_addr_t));
    pos+=sizeof(esp_bd_addr_t);
    memcpy(output_buf+pos,&timeout,sizeof(timeout));
    pos+=sizeof(timeout);
    memcpy(output_buf+pos,&max_idle_time,sizeof(max_idle_time));
    pos+=sizeof(max_idle_time);
    memcpy(output_buf+pos,data,data_len);
    app_uart_send((const char *) output_buf, packet_len);
}
/*
bool uart_read_data(ble_cb_param_pt cmd, uint16_t timeout){
	int cycle = (int)timeout/100;
	int cnt=0;
	int len = 0;
	//printf("Data wait cycle %d -------------------->\n", cycle);
	memset(data_line,0x0,sizeof(data_line));
	memset(&input_cmd_buf,0x0,sizeof(input_cmd_buf));
	do{
        len = uart_read_bytes(UART_NUM_2, data_line, BUF_LINE_SIZE, 100 / portTICK_RATE_MS);
        if(len>0){
        	//printf("Data input %d bytes, cur_pos=%d-------------------->\n", len,input_cmd_buf.cur_pos);
      		esp_log_buffer_hex(UART_TAG, data_line, len);
      		memcpy(input_cmd_buf.data+input_cmd_buf.cur_pos,data_line,len);
      		input_cmd_buf.cur_pos += len;
        }else{
        	vTaskDelay(100 / portTICK_RATE_MS);
            cnt++;
        }

	}while((len==0||(len>0&&input_cmd_buf.cur_pos<input_cmd_buf.cmd_len))&&cnt<cycle);

	if(input_cmd_buf.cur_pos==0&&cnt>=cycle){
		ESP_LOGE(UART_TAG, "Timout error!!! total read %d bytes, wait %d cycles", input_cmd_buf.cur_pos, cnt);
		return false;
	}
	cmd_len_t ack_len;
	ack_len.byte[0] = input_cmd_buf.data[PACKET_LEN_POS];
	ack_len.byte[1] = input_cmd_buf.data[PACKET_LEN_POS+1];

	switch(cmd->cmd_type){
	case BLE_SCAN:{
			uint8_t device_cnt = input_cmd_buf.data[BLE_SCAN_CNT_POS];
			rssi_value_t rssi;
			size_t index = 0;
			//int test_data=-93;
			ESP_LOGI(UART_TAG, "####ACK data len:%d, found %d devices", ack_len.value, device_cnt);
			cmd->para.scan.device_cnt = device_cnt;
			if(device_cnt > 0)
				cmd->para.scan.data = malloc(device_cnt*sizeof(rrpc_scan_device_t));
			for(int i=0; i<device_cnt&&index<ack_len.value; i++){
				index =BLE_SCAN_DATA_POS+i*BLE_SCAN_DATA_LEN;
				printf("#%d, index=%d\r\n",i,index);

				//esp_log_buffer_hex(UART_TAG,&test_data,sizeof(test_data));
				memcpy(&(cmd->para.scan.data[i].bda), input_cmd_buf.data+index, BLE_SCAN_BDA_LEN);
                index += BLE_SCAN_BDA_LEN;
                rssi.byte[0] = input_cmd_buf.data[index];
				rssi.byte[1] = input_cmd_buf.data[index+1];
				rssi.byte[2] = input_cmd_buf.data[index+2];
				rssi.byte[3] = input_cmd_buf.data[index+3];
				cmd->para.scan.data[i].rssi = rssi.value;
				index += BLE_SCAN_RSSI_LEN;
				memcpy(cmd->para.scan.data[i].name,input_cmd_buf.data+index,BLE_SCAN_NAME_LEN);
				snprintf(cmd->para.scan.data[i].bda_str,
						 sizeof(cmd->para.scan.data[i].bda_str),
						 "%02x:%02x:%02x:%02x:%02x:%02x",
						 BT_BD_ADDR_HEX(cmd->para.scan.data[i].bda));
				ESP_LOGI(UART_TAG,"#%d, %s, %d, %s", i, cmd->para.scan.data[i].bda_str, rssi.value, cmd->para.scan.data[i].name);
			}
		}
		break;
	case BLE_BIND_LOCK:
	case BLE_SEND_RRPC:
	case BLE_SEND_M2M:
	case BLE_SEND_M2S:
	case BLE_SEND_RPT:{
		uint16_t data_len = ack_len.value - BLE_SEND_ACK_POS;
		ESP_LOGI(UART_TAG, "####ACK data len:%d, data_len:%d", ack_len.value, data_len);
	    cmd->para.send.code = input_cmd_buf.data[BLE_SEND_RET_POS];

		if(data_len>0){
			esp_log_buffer_hex(UART_TAG,input_cmd_buf.data+BLE_SEND_ACK_POS,data_len);
			//Test first. maybe can be changed to malloc later
			cmd->para.send.data_buf = (unsigned char *)(input_cmd_buf.data + BLE_SEND_ACK_POS);
			cmd->para.send.data_len = data_len;
			//ESP_LOGI(UART_TAG, "alloc %d bytes", data_len);
			//memcpy(cmd->para.send.data_buf,input_cmd_buf.data+BLE_SEND_ACK_POS,data_len);
		}else{
			cmd->para.send.data_buf = NULL;
			cmd->para.send.data_len = 0;
		}
		break;
	}
	case BLE_CLOSE:
		cmd->para.close.code = input_cmd_buf.data[BLE_CLOSE_RET_POS];
		ESP_LOGI(UART_TAG,"Conn is closed!");
	}
    return true;
}*/

void uart_scan_ble_device(uint16_t timeout, uint8_t cycle, char* target_name,uint8_t name_len){
	uint16_t pos = 0;
    uint16_t packet_len = CMD_MODE_POS + 1 +  sizeof(timeout) + sizeof(cycle) + sizeof(name_len)+name_len;
    add_cmd_head(packet_len,UART_CMD_SCAN,&pos);
    //printf("uart_send_ble:time=%d,cycle=%d\r\n",timeout,cycle);
    memcpy(output_buf+pos,&timeout,sizeof(timeout));
    pos+=sizeof(timeout);
    output_buf[pos++]=cycle;
    output_buf[pos++]=name_len;
    memcpy(output_buf+pos,target_name,name_len);
    app_uart_send((const char *) output_buf, packet_len);
}

void uart_close_ble_conn(esp_bd_addr_t *bda){
	uint16_t pos = 0;
    uint16_t packet_len = CMD_MODE_POS + 1 +  sizeof(esp_bd_addr_t);
    add_cmd_head(packet_len,UART_CMD_CLOSE,&pos);
    memcpy(output_buf+pos,bda,sizeof(esp_bd_addr_t));
    app_uart_send((const char *) output_buf, packet_len);
}

void uart_reset_ble(){
	uint16_t pos = 0;
    uint16_t packet_len = CMD_MODE_POS + 1 ;
    add_cmd_head(packet_len,UART_CMD_RESET,&pos);
    app_uart_send((const char *) output_buf, packet_len);
}



void uart_check_ble_status(){
	uint16_t pos = 0;
    uint16_t packet_len = CMD_MODE_POS + CMD_TYPE_FIELD_LEN;
    add_cmd_head(packet_len,UART_CMD_ECHO,&pos);
    app_uart_send((const char *) output_buf, packet_len);
}

void uart_flush_read_buf(){
    uart_flush(UART_NUM_2);
    //printf("Flush uart read buffer\r\n");
}
