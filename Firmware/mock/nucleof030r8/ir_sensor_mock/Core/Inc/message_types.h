#ifndef MESSAGE_TYPES_H
#define MESSAGE_TYPES_H

#include "stdint.h"

//----- ADC/IR Sensor Message Types-----
typedef enum photodiode_t {
    PHOTODIODE_1 = 0, 
    PHOTODIODE_2
} photodiode_t;

typedef struct adc_msg_t {
    uint16_t adc_value;
    uint32_t timestamp;
    photodiode_t photodiode;
} adc_msg_t;

//----- Data Message Types-----
typedef enum sensor_status_t {
    SENSOR_OK, 
    SENSOR_SATURATED,
    SENSOR_OUT_OF_RANGE, 
    SENSOR_RATE_LIMIT, 
    SENSOR_FIFO_FAILED, 
    INTERNAL_BUFFERS_FAILED, 
    INTERNAL_MSG_NOT_SET
} sensor_status_t;

typedef struct data_msg_t {

    float distance_pd_1;
    float distance_pd_2;
    uint32_t timestamp_pd_1;
    uint32_t timestamp_pd_2;
    sensor_status_t status;

} data_msg_t;

//----- FSM Output Message Types -----
#define TX_MSG_BUFFER 64
#define RS485_MSG_BUFFER 32

typedef enum fsm_output_types_t {
    UART_TX, 
    RS485_TX
} fsm_output_types_t;

typedef struct fsm_output_msg_t {
    char uart_tx[TX_MSG_BUFFER];
    uint16_t uart_tx_len;

    char rs485_tx[RS485_MSG_BUFFER];
    uint16_t rs485_tx_len;
} fsm_output_msg_t;

//----- RS485 Message Types -----

#define PACKET_LEN 3

/// @brief this messaging structure will be used for the actual protocol/control functions
typedef struct rs485_packet_t {

    uint8_t sync_byte; 

    uint8_t cmd_byte; 
    
    uint8_t data_byte; 

} rs485_packet_t;

/// @brief actions that will be communicated between the actual rs485 interface and the fsm. 
typedef enum rs485_fsm_actions_t {
    START_STREAMING_RS485,
    LPF_ENABLE, 
    LPF_DISABLE,
    STREAM_ENABLE, 
    STREAM_DISABLE,
    LED_BRIGHTNESS_SET, 
    SYNC_BYTE_NOT_RECEIVED, 
    INVALID_DATA_BYTE, 
    INVALID_CMD_BYTE, 
    RS485_ERROR
} rs485_fsm_actions_t;

/// @brief interface between the FSM and the RS485 comms interface. Only used for RS485 to FSM at this moment. 
typedef struct rs485_interface_msg_t {

    rs485_fsm_actions_t fsm_action; 
    uint16_t data;

} rs485_interface_msg_t;

#endif