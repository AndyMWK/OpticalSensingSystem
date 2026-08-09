#ifndef BSP_PWM_H
#define BSP_PWM_H

#include <stdint.h>
#include "hal_signals.h"

/// @brief defines the amount of available PWM Channels for any device apps
typedef enum
{
    BSP_PWM_IR_LED = 0,
    BSP_PWM_COUNT
} bsp_pwm_t;

/// @brief stores the pwm channel context.
typedef struct bsp_pwm_ch_t
{
    unsigned int duty;
    unsigned int frequency;

    // generic types to abstract any HAL implementation
    void* timer_handle;
    uint16_t timer_channel;
    void* gpio_handle;

} bsp_pwm_ch_t;

// Macros
#define CHECK_VALID_CHANNEL(pwm_channel) (pwm_channel >= BSP_PWM_COUNT ? 0 : 1)

// Defines
#define DUTY_CYCLE_CAP 95.0f
#define CCR_CHANNEL_ADDR_INCR 4

hal_signal_t bsp_pwm_start(bsp_pwm_t ch);
hal_signal_t bsp_pwm_stop(bsp_pwm_t ch);
hal_signal_t bsp_pwm_set_freq_hz(bsp_pwm_t ch, uint16_t hz);
hal_signal_t bsp_pwm_set_duty_percent(bsp_pwm_t ch, float percent);
float bsp_pwm_get_duty_percent(bsp_pwm_t ch, hal_signal_t* status);
uint16_t bsp_pwm_get_freq_hz(bsp_pwm_t ch, hal_signal_t* status);

#endif