#include "led_pwm_control.h"

static inline uint8_t update_pwm_status(void);

// Wire up all the peripheral channels to the device.
static ir_led_pwm_t ir_led = {

    .pwm_channel = BSP_PWM_IR_LED,

    .curr_duty = 0.0,
    .curr_freq = 0};

/// @brief
/// @param
/// @return
status_signal_t ir_led_pwm_init(void)
{
    if (bsp_pwm_start(ir_led.pwm_channel) != HW_OK)
    {
        return DEV_NOT_INIT;
    }

    // updates the PWM status to its initial state
    if (!update_pwm_status())
    {
        return DEV_NOT_INIT;
    }

    return DEV_OK;
}

/// @brief
/// @param
/// @return
status_signal_t ir_led_pwm_stop(void)
{
    if (bsp_pwm_stop(ir_led.pwm_channel) != HW_OK)
    {
        return DEV_STOP_FAIL;
    }

    if (!update_pwm_status())
    {
        return DEV_STAT_UPDATE_FAILED;
    }

    return DEV_OK;
}

/// @brief
/// @param increment
/// @return
status_signal_t ir_led_pwm_brighten(float increment)
{
    if (ir_led.curr_duty + increment >= MAX_DUTY)
    {
        return DEV_INVALID_INPUT;
    }

    hal_signal_t status =
        bsp_pwm_set_duty_percent(ir_led.pwm_channel, ir_led.curr_duty + increment);

    switch (status)
    {
        case HW_CHANNEL_INVALID:
            return DEV_CHANNEL_NOT_AVAILABLE;
            break;

        case HW_NOT_INIT:
            return DEV_NOT_INIT;
            break;

        default:
            break;
    }

    if (!update_pwm_status())
    {
        return DEV_STAT_UPDATE_FAILED;
    }

    return DEV_OK;
}

/// @brief
/// @param decrement
/// @return
status_signal_t ir_led_pwm_dim(float decrement)
{
    if (ir_led.curr_duty - decrement <= MIN_DUTY)
    {
        return DEV_INVALID_INPUT;
    }

    hal_signal_t status =
        bsp_pwm_set_duty_percent(ir_led.pwm_channel, ir_led.curr_duty - decrement);

    switch (status)
    {
        case HW_CHANNEL_INVALID:
            return DEV_CHANNEL_NOT_AVAILABLE;
            break;

        case HW_NOT_INIT:
            return DEV_NOT_INIT;
            break;

        default:
            break;
    }

    if (!update_pwm_status())
    {
        return DEV_STAT_UPDATE_FAILED;
    }

    return DEV_OK;
}

/// @brief
/// @param status
/// @return
uint16_t ir_led_pwm_get_duty(status_signal_t* status)
{
    if (!update_pwm_status())
    {
        if (status == NULL)
            return 0;

        *status = DEV_STAT_UPDATE_FAILED;
        return 0;
    }
    else
    {
        if (status == NULL)
            return ir_led.curr_duty;
        *status = DEV_OK;
    }

    return ir_led.curr_duty;
}

/// @brief
/// @param status
/// @return
uint16_t ir_led_pwm_get_freq(status_signal_t* status)
{
    if (!update_pwm_status())
    {
        if (status == NULL)
            return 0;

        *status = DEV_STAT_UPDATE_FAILED;
        return 0;
    }
    else
    {
        if (status == NULL)
            return ir_led.curr_freq;
        *status = DEV_OK;
    }

    return ir_led.curr_freq;
}

/// @brief
/// @param
/// @return
static inline uint8_t update_pwm_status(void)
{
    hal_signal_t init_status = HW_OK;

    ir_led.curr_duty = bsp_pwm_get_duty_percent(ir_led.pwm_channel, &init_status);
    ir_led.curr_freq = bsp_pwm_get_freq_hz(ir_led.pwm_channel, &init_status);

    if (init_status != HW_OK)
    {
        return 0;
    }

    return 1;
}
