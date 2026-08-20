#ifndef FSM_H
#define FSM_H

#include "main.h"
#include "fifo.h"
#include "logger.h"
#include "ir_processing.h"
#include "message_types.h"
#include <stdio.h>
#include <string.h>
#include <math.h>

#define MAX_ERROR_COUNT                 10 

#define PWM_DELTA                       1   // in %
#define FREQUENCY_DELTA                 10  // Hz
#define PWM_DIM_TARGET                  35  // in %
#define PWM_BRIGHTEN_TARGET             65  // in %

/// @brief All FSM States
typedef enum fsm_state_t {
    STATE_IDLE = 0, 
    STATE_STREAMING, 
    STATE_STREAMING_DISABLED,
    STATE_PWM_DIM,
    STATE_PWM_BRIGHTEN,
    STATE_SATURATION,
    STATE_TOO_FAR,
    STATE_FIFO_EMPTY,
    STATE_INTERNAL_BUFFER_ERROR,
    STATE_ERROR
} fsm_state_t;

/// @brief records the current state of the FSM and its error count
typedef struct fsm_context_t {
    fsm_state_t state;
    uint16_t error_count;
} fsm_context_t;

/// @brief the interface that the FSM uses for serial messages
typedef struct comms_msg_t {
    uint8_t serial_log_tick;

    volatile uint8_t rs485_flag;
    rs485_interface_msg_t rs485_msg;
} comms_msg_t;

/// @brief interface that the FSM will use for controlling the PWM duty and frequency
typedef struct ir_led_pwm_t {

    int pwm_duty_cycle;    // resolution of 1%
    int target_duty;

} ir_led_pwm_t;

typedef void (*async_log_cb_t)(char* msg_buf, uint16_t msg_len);
typedef void (*pwm_control_t)(uint32_t duty);

// Public API functions for the FSM module.

/// @brief main function used for updating the actual FSM
void update_fsm(fifo_t* fifo_pd1, fifo_t* fifo_pd2, comms_msg_t* comms);

/// @brief used to collect outputs from the FSM. Various message types are available. 
void output_from_fsm(fsm_output_types_t out_type, char* message, uint16_t* filled_len, uint16_t message_max_size);

/// @brief resets the state of the FSM. To Do: also wipe the cached logs and input messages as well. 
void reset_fsm(void);

/// @brief Handles the asynchronous signal request
/// @param void
uint8_t handle_async_request(void);

/// @brief 
/// @param cb_func 
/// @param pwm_func 
void register_fsm_callbacks(async_log_cb_t cb_func, pwm_control_t pwm_func);

#endif
