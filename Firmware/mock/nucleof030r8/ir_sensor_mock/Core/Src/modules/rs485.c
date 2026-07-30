#include "rs485.h"

/// @brief instantiate an rx register for receiving bytes from RS485 interface. 
static rs485_register_t rx_register = {
    .buffer = {0, 0, 0}, 
    .packet = {
        .sync_byte = 0, 
        .cmd_byte = 0, 
        .data_byte = 0
    }
}; 

/// @brief instantiate a tx register for transmitting bytes form RS485 interface. 
static rs485_register_t tx_register = {
    .buffer = {RS485_SYNC_BYTE, 0, 0}, 
    .packet = {
        .sync_byte = RS485_SYNC_BYTE, 
        .cmd_byte = 0, 
        .data_byte = 0
    }
}; 

/// @brief rs485 context mainly used for the uart handle. 
static rs485_ctx_t ctx = {
    .huart = NULL, 
    .tx_timeout = HAL_MAX_DELAY
};

/// @brief loads bytes onto the SRAM via DMA using the STM32 HAL API call. 
/// @param msg pointer to the message buffer
/// @param msg_len length of the message buffer
/// @return 0 on unsuccessful transmission, 1 on successful transmission
static uint8_t load_msg_dma(uint8_t* msg, uint16_t msg_len);

/// @brief initializes the rs485 context with the selected uart handle. 
/// @param huart uart handle based on STM32 HAL API
void init_rs485(UART_HandleTypeDef* huart) {
    if(huart == NULL) {
        return;
    }

    ctx.huart = huart;
}

/// @brief parses the received packets in the rx buffer and stores into a message
/// @param rx_buffer buffer bytes
/// @param msg stores the parsed messages
void read_packet(uint8_t rx_buffer[], rs485_interface_msg_t* msg) {

    // load the received buffer into a formatted register for rx.
    memcpy(rx_register.buffer, rx_buffer, PACKET_LEN);

    // check for the sync byte
    if(rx_register.packet.sync_byte != RS485_SYNC_BYTE) {
        msg->fsm_action = SYNC_BYTE_NOT_RECEIVED;

        return;
    }

    // check for command and the given data
    switch(rx_register.packet.cmd_byte) {

        case DIGITAL_LPF_ENABLE_CMD: 
            msg->fsm_action = LPF_ENABLE;
        break;

        case LED_CONTROL_PWM_SET_CMD: 

            if(rx_register.packet.data_byte == 0 || rx_register.packet.data_byte >= 95) {
                msg->fsm_action = INVALID_DATA_BYTE;
            } else {
                msg->fsm_action = LED_BRIGHTNESS_SET;
            }

        break;

        case STREAM_ENABLE_CMD:
            msg->fsm_action = STREAM_ENABLE;
        break;

        case STREAM_DISABLE_CMD: 
            msg->fsm_action = STREAM_DISABLE;
        break;

        case DIGITAL_LPF_DISABLE_CMD: 
            msg->fsm_action = LPF_DISABLE;
        break;

        default: 
            msg->fsm_action = INVALID_CMD_BYTE;
        break;

    }
}

uint8_t send_packet(uint8_t response, uint8_t data) {

    // check for valid response byte
    if(response > DIGITAL_LPF_DISABLE_CMD || response == 0x00 || (response % 2U) == 1U) {
        return 0;
    }

    // load data and response onto the tx register
    tx_register.packet.cmd_byte = response;
    tx_register.packet.data_byte = data;

    // send rs485 packet via dma
    return load_msg_dma(tx_register.buffer, MSG_SIZE);
}

static uint8_t load_msg_dma(uint8_t* msg, uint16_t msg_len) {

    // bound checking
    if(msg_len == 0 || msg == NULL) {
        return 0;
    }
    
    // DMA transmit call
    if(HAL_UART_Transmit_DMA(ctx.huart, msg, msg_len) != HAL_OK) {
        return 0;
    }

    return 1;

}