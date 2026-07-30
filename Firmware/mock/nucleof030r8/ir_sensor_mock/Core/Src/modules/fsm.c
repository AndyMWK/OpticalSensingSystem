#include "fsm.h"

// Private Helper Functions
static void handle_sensor_error();
static void handle_fifo_failed_error();
static void handle_sensor_saturated_error();
static void handle_sensor_out_of_range_error();

static inline void create_log_msg(data_msg_t* incoming_fifo_data);
static inline uint32_t distance_to_centimeters(float distance);

//---Internal data structures initialization for the FSM---
static fsm_context_t ctx = {
    .state = STATE_IDLE, 
    .error_count = 0
};

static fsm_output_msg_t output_msg = {
    .uart_tx = "",
    .uart_tx_len = 0, 

    .rs485_tx = "",
    .rs485_tx_len = 0
};

void update_fsm(fifo_t* fifo_pd1, fifo_t* fifo_pd2, comms_msg_t* comms) {

    // local buffer to stores the incoming fifo adc messages. 
    data_msg_t incoming_fifo_data = {
        .distance_pd_1 = 0.0, 
        .distance_pd_2 = 0.0,

        .timestamp_pd_1 = 0,
        .timestamp_pd_2 = 0, 

        .status = INTERNAL_MSG_NOT_SET
    };

    // feeds the incoming fifo data to the signal processing stage
    stream_from_fifo_ema_lpf(fifo_pd1, fifo_pd2, &incoming_fifo_data);

    // based on streaming fifo signal processing stage, handle sensor error
    switch(incoming_fifo_data.status) {
        case SENSOR_OK:
            // do nothing
        break;

        case SENSOR_SATURATED: 
            //handle_sensor_saturated_error();
        break;

        case SENSOR_OUT_OF_RANGE: 
            //handle_sensor_out_of_range_error();
        break;

        case SENSOR_RATE_LIMIT:
            //handle_sensor_error();
        break;

        case SENSOR_FIFO_FAILED: 
            //handle_fifo_failed_error();
        break;

        case INTERNAL_MSG_NOT_SET: 
            return;
        break;

        default:
        break;
    }

    // based on device state, send out appropriate output messages
    switch(ctx.state) {

        // state transition from idle to stream
        case STATE_IDLE: 

            if(comms->rs485_msg.fsm_action == STREAM_ENABLE) {
                ctx.state = STATE_STREAMING;
            } else {
                ctx.state = STATE_IDLE;
            }
            
        break;

        case STATE_STREAMING:

            // stream log messages via serial console
            create_log_msg(&incoming_fifo_data);
            output_msg.uart_tx_len = (uint16_t)strlen(output_msg.uart_tx) + 1U;

            ctx.state = STATE_STREAMING;
        break;

        case STATE_DIMMING: 
            
        break;

        case STATE_SATURATION: 

            // stream state
            // go to state streaming?
        break;

        case STATE_TOO_FAR: 
        
            // undo dimming?
        break;

        case STATE_ERROR: 
            // restart device...
        break;

        default: 

        break;
    }

    // debug statement
    if(comms->rs485_msg.fsm_action == RS485_ERROR) {

        strcat(output_msg.uart_tx, "RS485 Transmit Failed\r\n");

        // magic number for now deal with it later
        output_msg.uart_tx_len = sizeof(output_msg.uart_tx) + 23;
    }
}

/// @brief Based on the selected FSM output type, the FSM output message gets filled. 
/// @param out_type 
/// @param message 
/// @param filled_len 
/// @param message_max_size 
void output_from_fsm(fsm_output_types_t out_type, char* message, uint16_t* filled_len, uint16_t message_max_size) {

    if(message == NULL || filled_len == NULL || message_max_size == 0) {
        return;
    }

    
    switch(out_type) {

        case UART_TX: 

            // Guards against copying a message that will exceed the message buffer size
            if(output_msg.uart_tx_len > message_max_size) {
                return;
            }

            memcpy(message, output_msg.uart_tx, output_msg.uart_tx_len);
            *filled_len = output_msg.uart_tx_len;
        break;

        case RS485_TX: 
            
            // Guards against copying a message that will exceed the message buffer size
            if(output_msg.rs485_tx_len > message_max_size) {
                return;
            }
            
            memcpy(message, output_msg.rs485_tx, output_msg.rs485_tx_len);
            *filled_len = output_msg.rs485_tx_len;
        break;
            
        default:
            // Should just ignore any invalid output types or can use a debug print statement here. 
        break;
    }
}

void reset_fsm() {
    ctx.state = STATE_IDLE;

    // To Do: Need to wipe the allocated logs and msesages. 
}

/// @brief creates a log message based on incoming sensor data. The log message is meant to be printed on the serial console. 
/// @param incoming_fifo_data packaged fifo data containing adc data, timestamps, and sensor statuses
static void create_log_msg(data_msg_t* incoming_fifo_data) {
    uint32_t distance_pd_1_cm = distance_to_centimeters(incoming_fifo_data->distance_pd_1);
    uint32_t distance_pd_2_cm = distance_to_centimeters(incoming_fifo_data->distance_pd_2);

    // clear the uart tx buffer: 
    memset(output_msg.uart_tx, 0, sizeof(output_msg.uart_tx));

    // format and push the logs into the uart tx buffer
    snprintf(   output_msg.uart_tx, sizeof(output_msg.uart_tx), 
                "PD%u D:%lu.%02lu T:%lus PD%u D:%lu.%02lu T:%lus\r\n",
                (unsigned int)PHOTODIODE_1,
                (unsigned long)(distance_pd_1_cm / 100U),
                (unsigned long)(distance_pd_1_cm % 100U),
                (unsigned long)incoming_fifo_data->timestamp_pd_1,
                (unsigned int)PHOTODIODE_2,
                (unsigned long)(distance_pd_2_cm / 100U),
                (unsigned long)(distance_pd_2_cm % 100U),
                (unsigned long)incoming_fifo_data->timestamp_pd_2);
    
    output_msg.uart_tx_len = sizeof(output_msg.uart_tx);
}   

/// @brief inline function to convert measured measured distance into centimeters. 
/// @param distance calculated distance based on optics physics. 
/// @return integer value representing the distance in centimeters. 
static inline uint32_t distance_to_centimeters(float distance) {
   if (!isfinite(distance) || distance <= 0.0f) {
        return 0U;
    }

    float scaled = (distance * 100.0f) + 0.5f;
    if (scaled >= (float)UINT32_MAX) {
        return UINT32_MAX;
    }

    return (uint32_t)scaled;
}

/// @brief increments the sensor error count until max threshold is reached. Once the max error count is reached, 
// the fsm state changes to error handling. 
static void handle_sensor_error() {

    if(ctx.error_count == MAX_ERROR_COUNT) {
        ctx.state = STATE_ERROR;

        return;
    }

    ctx.error_count++;
}
