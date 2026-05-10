#include "rs485.h"

static rs485_register_t rx_register = {
    .buffer = {0, 0, 0}, 
    .packet = {
        .sync_byte = 0, 
        .cmd_byte = 0, 
        .data_byte = 0
    }
}; 

static rs485_register_t tx_register = {
    .buffer = {0, 0, 0}, 
    .packet = {
        .sync_byte = RS485_SYNC_BYTE, 
        .cmd_byte = 0, 
        .data_byte = 0
    }
}; 

// should be called in a callback function or after an interrupt tick was set
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