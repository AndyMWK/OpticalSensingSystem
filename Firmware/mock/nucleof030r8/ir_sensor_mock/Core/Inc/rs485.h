#include "message_types.h"
#include <string.h>

#define RS485_SYNC_BYTE             0xAA

// ---- RS-485 CMD List ----
#define DIGITAL_LPF_ENABLE_CMD      0x01
#define LED_CONTROL_PWM_SET_CMD     0x03
#define STREAM_ENABLE_CMD           0x05
#define STREAM_DISABLE_CMD          0x07
#define DIGITAL_LPF_DISABLE_CMD     0x09

typedef union rs485_register_t {

    // stores the received bytes into memory here. 
    uint16_t buffer[PACKET_LEN];

    // the memory organization automatically deserializes the received packet. 
    rs485_packet_t packet;

} rs485_register_t;

// ---- Public API ----

void read_packet(uint8_t rx_buffer[], rs485_interface_msg_t* msg);
void send_packet(rs485_interface_msg_t* msg);