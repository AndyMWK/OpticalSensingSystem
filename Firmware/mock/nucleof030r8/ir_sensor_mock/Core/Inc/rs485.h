#ifndef RS485_H

#define RS485_H

#include "message_types.h"
#include "main.h"
#include <string.h>

#define RS485_SYNC_BYTE             0xAA

// ---- RS-485 CMD List ----
#define DIGITAL_LPF_ENABLE_CMD      0x01
#define LED_CONTROL_PWM_SET_CMD     0x03
#define STREAM_ENABLE_CMD           0x05
#define STREAM_DISABLE_CMD          0x07
#define DIGITAL_LPF_DISABLE_CMD     0x09

// ---- RS-485 Response List ----
#define ACK_HEARTBEAT               0x02
#define NACK                        0x04

#define MSG_SIZE 3

/// @brief register to store incoming and outgoing packets
typedef union rs485_register_t {

    // stores the received bytes into memory here. 
    uint8_t buffer[PACKET_LEN];

    // the memory organization automatically deserializes the received packet. 
    rs485_packet_t packet;

} rs485_register_t;

/// @brief just stores the uart handle and tx timeout for now. 
typedef struct rs485_ctx_t {
    UART_HandleTypeDef* huart;
    uint32_t tx_timeout;
} rs485_ctx_t;

// ---- Public API ----
void init_rs485(UART_HandleTypeDef* huart);
void read_packet(uint8_t rx_buffer[], rs485_interface_msg_t* msg);
uint8_t send_packet(uint8_t response, uint8_t data);

#endif