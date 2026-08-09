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
#define CHECK_VALID_CHANNEL(pwm_channel) (((pwm_channel) < BSP_PWM_COUNT) ? 1 : 0)

// Defines
#define BSP_DUTY_CYCLE_CAP 95.0f

hal_signal_t bsp_pwm_start(const bsp_pwm_t ch);
hal_signal_t bsp_pwm_stop(const bsp_pwm_t ch);

// These functions directly write to the CCR and ARR registers when called.
hal_signal_t bsp_pwm_set_freq_hz(const bsp_pwm_t ch, const uint32_t hz);
hal_signal_t bsp_pwm_set_duty_percent(const bsp_pwm_t ch, float percent);

float bsp_pwm_get_duty_percent(const bsp_pwm_t ch, hal_signal_t* status);
uint32_t bsp_pwm_get_freq_hz(const bsp_pwm_t ch, hal_signal_t* status);

#endif