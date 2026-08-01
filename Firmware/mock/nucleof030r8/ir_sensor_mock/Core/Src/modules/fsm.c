#include "fsm.h"

// To do: allow for async actions/calls from the FSM. 
// Architect PWM output from the FSM. 

// Private Helper Functions
static void update_state_rs485_cmd(comms_msg_t* comms);
static void handle_sensor_error(void);
static void handle_fifo_failed_error(void);
static void handle_sensor_saturated_error(void);
static void handle_sensor_out_of_range_error(void);
static void handle_internal_msg_failed(void);
static void create_log_msg(data_msg_t* incoming_fifo_data);


// inline functions for shorter operations
static inline uint32_t distance_to_centimeters(float distance);
static inline void clear_output_uart_buffer(void);
static inline uint8_t append_log_msg_to_uart(char* msg);
static inline void append_byte_to_uart(uint16_t byte);

//---Internal data structures initialization for the FSM---
static fsm_context_t ctx = {
    .state = STATE_IDLE, 
    .error_count = 0
};

static fsm_output_msg_t output_msg = {
    .uart_tx = "",
    .rs485_tx = "",
};

static ir_led_pwm_t ir_led_pwm = {
    .pwm_duty_cycle = 50, 
    .frequency = 1000,
    .htim = NULL
};

static uint8_t async_signal = 0;

static async_log_cb_t log_callback = NULL;

/// @brief main function loop that updates the FSM. 
/// @param fifo_pd1 fifo handle for photodiode 1
/// @param fifo_pd2 fifo handle for photodiode 2
/// @param comms    containers for the RS485 and UART TX/RX buffers
/// @param async_signal asynchronous signal that indicates to other modules that FSM output needs to be handled immediately. 
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
            
            if(comms->rs485_flag) {
                update_state_rs485_cmd(comms);
            }

        break;

        case SENSOR_SATURATED: 
            handle_sensor_saturated_error();
        break;

        case SENSOR_OUT_OF_RANGE: 
            handle_sensor_out_of_range_error();
        break;

        case SENSOR_RATE_LIMIT:
            handle_sensor_error();
        break;

        case SENSOR_FIFO_EMPTY: 
            handle_fifo_failed_error();
        break;

        case INTERNAL_MSG_NOT_SET: 
            handle_internal_msg_failed();
        break;

        default:
        break;
    }

    if(ctx.state != STATE_IDLE && ctx.state != STATE_STREAMING_DISABLED) {

        // clear previous messages
        memset(output_msg.uart_tx, 0, sizeof(output_msg.uart_tx));

        // stream log messages to the serial output
        create_log_msg(&incoming_fifo_data);

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
            
            // just for debugging: 
            ctx.state = STATE_STREAMING;
        break;

        case STATE_STREAMING: 
            append_log_msg_to_uart("Sensor State is healthy\r\n");
        break;

        case STATE_PWM_DUTY_CHANGE: 
            // control the pwm outputs

            ctx.state = STATE_PWM_DUTY_CHANGE;
            append_log_msg_to_uart("PWM Duty: \r\n");

            if(incoming_fifo_data.status == SENSOR_OK) {
                ctx.state = STATE_STREAMING;
            }
            
        break;

        case STATE_TOO_FAR:  
            append_log_msg_to_uart("Sensor too far, Brightening.\r\n");
            ctx.state = STATE_PWM_DUTY_CHANGE;
        break;

        case STATE_SATURATION: 
            append_log_msg_to_uart("Sensor too close, Dimming.\r\n");
            ctx.state = STATE_PWM_DUTY_CHANGE;
        break;  

        case STATE_ERROR: 
            append_log_msg_to_uart("Maximum Error Count Reached. Device Restart Recommended...\r\n");
        break;

        case STATE_FIFO_EMPTY: 
            append_log_msg_to_uart("ADC FIFO Empty\r\n");

            if(ctx.error_count < MAX_ERROR_COUNT) {
                ctx.state = STATE_STREAMING;
            }

        break;

        case STATE_INTERNAL_BUFFER_ERROR: 
            append_log_msg_to_uart("Internal Message Buffers uninitialized. Device Restart Recommended...\r\n");
        break;

        default: 

        break;
    }
}

/// @brief Based on the selected FSM output type, the FSM output message gets filled. 
/// @param out_type         select the type of output. Either RS485 or UART
/// @param message          message buffer
/// @param filled_len       the filled length of the message buffer
/// @param message_max_size maximum message size
void output_from_fsm(fsm_output_types_t out_type, char* message, uint16_t* filled_len, const uint16_t message_max_size) {

    if(message == NULL || filled_len == NULL || message_max_size == 0) {
        return;
    }

    
    switch(out_type) {

        case UART_TX: 

            // Guards against copying a message that will exceed the message buffer size
            if(strlen(output_msg.uart_tx) > message_max_size) {
                return;
            }

            memcpy(message, output_msg.uart_tx, strlen(output_msg.uart_tx));
            *filled_len = strlen(output_msg.uart_tx);
        break;

        case RS485_TX: 
            
            // Guards against copying a message that will exceed the message buffer size
            if(strlen(output_msg.rs485_tx) > message_max_size) {
                return;
            }
            
            memcpy(message, output_msg.rs485_tx, strlen(output_msg.rs485_tx));
            *filled_len = strlen(output_msg.rs485_tx);
        break;
            
        default:
            // Should just ignore any invalid output types or can use a debug print statement here. 
        break;
    }
}

/// @brief Resets the FSM state to IDLE and its error count. Also clears the output uart buffer. 
void reset_fsm(void) {
    ctx.state = STATE_IDLE;
    ctx.error_count = 0;
    clear_output_uart_buffer();
}

/// @brief initializes the PWM interface with the HAL timer handle
/// @param htim timer handle
void init_pwm_interface(TIM_HandleTypeDef* htim) {

    ir_led_pwm.htim = htim;

}

/// @brief creates a log message based on incoming sensor data. The log message is meant to be printed on the serial console. 
/// @param incoming_fifo_data   packaged fifo data containing adc data, timestamps, and sensor statuses
static void create_log_msg(data_msg_t* incoming_fifo_data) {
    uint32_t distance_pd_1_cm = distance_to_centimeters(incoming_fifo_data->distance_pd_1);
    uint32_t distance_pd_2_cm = distance_to_centimeters(incoming_fifo_data->distance_pd_2);
    
    size_t used = strlen(output_msg.uart_tx);
    if(used >= sizeof(output_msg.uart_tx)) {
        return;
    }

    // format and push the logs into the uart tx buffer
    snprintf(   output_msg.uart_tx + used, sizeof(output_msg.uart_tx) - used, 
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

uint8_t handle_async_request(void) {

    if(log_callback == NULL) {
        return 0;
    }

    if(async_signal) {

        // directly output elements stored on the uart log. 
        log_callback(output_msg.uart_tx, strlen(output_msg.uart_tx));
        async_signal = 0;

        return 1;
    }

    return 0;

}

void register_log_callback(async_log_cb_t cb_func) {

    if(cb_func == NULL) {
        return;
    }

    log_callback = cb_func;

}

static void update_state_rs485_cmd(comms_msg_t* comms) {

    /*
        FSM interface for the RS485 commands: 

        LPF_ENABLE, 
        LPF_DISABLE,
        STREAM_ENABLE, 
        STREAM_DISABLE,
        LED_BRIGHTNESS_SET, 
        SYNC_BYTE_NOT_RECEIVED, 
        INVALID_DATA_BYTE, 
        INVALID_CMD_BYTE, 
        RS485_ERROR
    */

    // update the async signal to indicate incoming RS485 message
    async_signal = 1;

    switch(comms->rs485_msg.fsm_action) {
        case LPF_ENABLE: 
            append_log_msg_to_uart("LPF Enable Requested\r\n");
            break;

        case LPF_DISABLE: 
            append_log_msg_to_uart("LPF Disable Requested\r\n");
            break;

        case STREAM_ENABLE: 
            append_log_msg_to_uart("Streaming Enabled\r\n");
            ctx.state = STATE_STREAMING;
            break;

        case STREAM_DISABLE: 
            append_log_msg_to_uart("Streaming Disabled\r\n");
            ctx.state = STATE_STREAMING_DISABLED;
            break;
        case LED_BRIGHTNESS_SET: 
            append_log_msg_to_uart("LED Brightness Set to: ");
            append_byte_to_uart(comms->rs485_msg.data);
            append_log_msg_to_uart("\r\n");
            ctx.state = STATE_PWM_DUTY_CHANGE;
            break;
        case SYNC_BYTE_NOT_RECEIVED: 
            append_log_msg_to_uart("RS485: SYNC Byte not received\r\n");
            break;
        case INVALID_DATA_BYTE: 
            append_log_msg_to_uart("RS485: Invalid Data Byte:");
            append_byte_to_uart(comms->rs485_msg.data);
            append_log_msg_to_uart("\r\n");
            break;
        case INVALID_CMD_BYTE: 
            append_log_msg_to_uart("RS485: Invalid Command Byte\r\n");
            break;
        case RS485_ERROR: 
              
            break;
        
        default: 
            break;
        
    }   
}

/// @brief increments the sensor error count until max threshold is reached. Once the max error count is reached, 
// the fsm state changes to error handling. 
static void handle_sensor_error(void) {

    if(ctx.error_count == MAX_ERROR_COUNT) {
        ctx.state = STATE_ERROR;

        return;
    }

    ctx.error_count++;
}

/// @brief  state transition when sensor is saturated. 
/// @param  void
static void handle_sensor_saturated_error(void) {
    ctx.error_count++;

    if (ctx.state == STATE_PWM_DUTY_CHANGE) {
        async_signal = 0;
        return;
    }

    async_signal = 1;
    ctx.state = STATE_SATURATION;
}

/// @brief  state transition when the fifo failed to dequeue
/// @param  void
static void handle_fifo_failed_error(void) {
    ctx.error_count++;
    async_signal = 1;

    ctx.state = STATE_FIFO_EMPTY;
}

/// @brief  state transition when the sensor adc value is below threshold. 
/// @param  void
static void handle_sensor_out_of_range_error() {
    ctx.error_count++;

    if (ctx.state == STATE_PWM_DUTY_CHANGE) {
        async_signal = 0;
        return;
    }

    async_signal = 1;
    ctx.state = STATE_TOO_FAR;
}

/// @brief  state transition when the internal message buffers are uninitalized. 
/// @param  void
static void handle_internal_msg_failed() {
    ctx.error_count++;
    async_signal = 1;
    // This error means that one of the message buffers are unallocated. 
    ctx.state = STATE_INTERNAL_BUFFER_ERROR;
}

//----- Inline Functions -----

/// @brief appends a string to the output buffer for the logger. 
/// @param msg string to be appended
/// @return 0 on unsuccessful append operation, 1 on successful append operation
static inline uint8_t append_log_msg_to_uart(char* msg) {
    if(msg == NULL) {
        return 0;
    }

    size_t used = strlen(output_msg.uart_tx);
    if(used >= sizeof(output_msg.uart_tx)) {
        return 0;
    }

    snprintf(output_msg.uart_tx + used, sizeof(output_msg.uart_tx) - used, "%s", msg);

    return 1;
}

/// @brief appends a byte to the output buffer string
/// @param byte the byte to append
static inline void append_byte_to_uart(uint16_t byte) {
    size_t used = strlen(output_msg.uart_tx);
    if(used >= sizeof(output_msg.uart_tx)) {
        return;
    }

    snprintf(output_msg.uart_tx + used, sizeof(output_msg.uart_tx) - used, "%x", byte);
}

/// @brief sets the output uart buffer to zero
/// @param void 
static inline void clear_output_uart_buffer(void) {

    memset(output_msg.uart_tx, 0, sizeof(output_msg.uart_tx));

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

