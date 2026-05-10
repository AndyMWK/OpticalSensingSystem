#include "fsm.h"

static void handle_sensor_error();
static void handle_fifo_failed_error();
static void handle_sensor_saturated_error();
static void handle_sensor_out_of_range_error();

static void create_log_msg(data_msg_t* incoming_fifo_data);
static uint32_t distance_to_centimeters(float distance);

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

void update_fsm(fifo_t* fifo, comms_msg_t* comms) {

    data_msg_t incoming_fifo_data = {
        .distance_pd_1 = 0.0, 
        .distance_pd_2 = 0.0,

        .timestamp_pd_1 = 0,
        .timestamp_pd_2 = 0, 

        .status = INTERNAL_MSG_NOT_SET
    };

    stream_from_fifo_ema_lpf(fifo, &incoming_fifo_data);

    // logic to transition the sensor state
    switch(incoming_fifo_data.status) {
        case SENSOR_OK:
            ctx.error_count = 0;
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

    // state outputs
    switch(ctx.state) {
        case STATE_IDLE: 
            ctx.state = STATE_STREAMING;
        break;

        case STATE_STREAMING:
            // strcpy(output_msg.uart_tx, "hello\n\r\0");
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
}

void output_from_fsm(fsm_output_types_t out_type, char* message, uint16_t* filled_len, uint16_t message_max_size) {

    if(message == NULL || filled_len == NULL || message_max_size == 0) {
        return;
    }

    switch(out_type) {

        case UART_TX: 

            // Can't copy in the message when the uart message exceeds the maximum length. 
            if(output_msg.uart_tx_len > message_max_size) {
                return;
            }

            memcpy(message, output_msg.uart_tx, output_msg.uart_tx_len);
            *filled_len = output_msg.uart_tx_len;
        break;

        case RS485_TX: 

        break;
            
        default:
            // Should just ignore any invalid output types. 
        break;
    }
}

void reset_fsm() {
    ctx.state = STATE_IDLE;

    // To Do: Need to wipe the allocated logs and msesages. 
}

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
}   

static uint32_t distance_to_centimeters(float distance) {
    if(distance <= 0.0f) {
        return 0U;
    }

    return (uint32_t)((distance * 100.0f) + 0.5f);
}

static void handle_sensor_error() {

    if(ctx.error_count == MAX_ERROR_COUNT) {
        ctx.state = STATE_ERROR;

        return;
    }

    ctx.error_count++;
}
