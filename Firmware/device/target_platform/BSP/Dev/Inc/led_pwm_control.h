#ifndef LED_PWM_CONTROL_H
#define LED_PWM_CONTROL_H

#include "signals.h"
#include "bsp_pwm.h"
#include "bsp_timer.h"

#define MAX_DUTY 90
#define MIN_DUTY 10

#define DUTY_RAMP_UP_DELTA 1
#define DUTY_RAMP_DOWN_DELTA 1

typedef struct ir_led_pwm_t
{
    bsp_pwm_t pwm_channel;

    // timer channel

    float curr_duty;
    uint16_t curr_freq;

} ir_led_pwm_t;

status_signal_t ir_led_pwm_init(void);
status_signal_t ir_led_pwm_stop(void);

uint16_t ir_led_pwm_get_duty(status_signal_t* status);
uint16_t ir_led_pwm_get_freq(status_signal_t* status);

status_signal_t ir_led_pwm_brighten(float increment);
status_signal_t ir_led_pwm_dim(float decrement);

status_signal_t ir_led_pwm_max_brightness(uint16_t increment, uint16_t ramp_time);
status_signal_t ir_led_pwm_min_birghtness(uint16_t decrement, uint16_t ramp_time);


#endif