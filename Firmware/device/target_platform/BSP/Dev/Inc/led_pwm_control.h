#ifndef LED_PWM_CONTROL_H
#define LED_PWM_CONTROL_H

#include "signals.h"
#include "bsp_pwm.h"
#include "bsp_timer.h"

#define IR_LED_MAX_DUTY 90.0f
#define IR_LED_MIN_DUTY 10.0f

// define behaviour between electrical PWM constraint and device/policy contstraint
_Static_assert(IR_LED_MAX_DUTY < BSP_DUTY_CYCLE_CAP,
               "IR LED Max Duty Cycle is greater than electrical PWM constraints");

#define IR_LED_DUTY_RAMP_UP_DELTA 1
#define IR_LED_DUTY_RAMP_DOWN_DELTA 1

/// @brief One IR LED channel. The caller owns the instance; pass the same one to every
///        call. Treat the fields as read only - they are a cache of what the hardware
///        last reported, refreshed on every call, not a place to write a new setpoint.
///
/// Not ISR safe. ir_led_pwm_brighten and ir_led_pwm_dim are read-modify-write sequences
/// over self, so a single instance must be driven from one execution context.
typedef struct ir_led_pwm_t
{
    bsp_pwm_t pwm_channel;

    // timer channel

    float curr_duty;
    uint32_t curr_freq;

} ir_led_pwm_t;

status_signal_t ir_led_pwm_init(ir_led_pwm_t* self, const bsp_pwm_t ch);
status_signal_t ir_led_pwm_stop(ir_led_pwm_t* self);

/* Both getters take an optional status out param. On failure they return 0, which is not
   distinguishable from a genuine 0% / 0 Hz reading - pass a status pointer if the caller
   needs to tell those apart. */
float ir_led_pwm_get_duty(ir_led_pwm_t* self, status_signal_t* status);
uint32_t ir_led_pwm_get_freq(ir_led_pwm_t* self, status_signal_t* status);

/* Notes:
    Deltas must be strictly positive. A step that would cross IR_LED_MAX_DUTY / IR_LED_MIN_DUTY is
    rejected rather than clamped, so the caller decides what to do at the limit.
*/
status_signal_t ir_led_pwm_brighten(ir_led_pwm_t* self, const float increment);
status_signal_t ir_led_pwm_dim(ir_led_pwm_t* self, const float decrement);

status_signal_t ir_led_override_duty(ir_led_pwm_t* self, const float duty);

status_signal_t ir_led_pwm_max_brightness(ir_led_pwm_t* self, uint16_t increment,
                                          uint16_t ramp_time);
status_signal_t ir_led_pwm_min_brightness(ir_led_pwm_t* self, uint16_t decrement,
                                          uint16_t ramp_time);


#endif
