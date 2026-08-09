#include "led_pwm_control.h"

static status_signal_t update_pwm_status(ir_led_pwm_t* self);

/// @brief binds an LED instance to a PWM channel and starts it.
/// @param self caller owned instance, zeroed and populated by this call
/// @param ch PWM channel to drive
/// @return DEV_OK on success. DEV_INVALID_INPUT for a NULL self,
///         DEV_CHANNEL_NOT_AVAILABLE for an unknown channel, otherwise whatever the
///         Port layer reported.
status_signal_t ir_led_pwm_init(ir_led_pwm_t* self, const bsp_pwm_t ch)
{
    if (self == NULL)
    {
        return DEV_INVALID_INPUT;
    }

    if (!CHECK_VALID_CHANNEL(ch))
    {
        return DEV_CHANNEL_NOT_AVAILABLE;
    }

    self->pwm_channel = ch;
    self->curr_duty = 0.0f;
    self->curr_freq = 0;

    hal_signal_t hw = bsp_pwm_start(self->pwm_channel);
    if (hw != HW_OK)
    {
        return dev_status_from_hal(hw);
    }

    // updates the PWM status to its initial state
    return update_pwm_status(self);
}

/// @brief stops the PWM output on the instance's channel.
/// @param self instance previously passed to ir_led_pwm_init
/// @return DEV_OK on success, DEV_INVALID_INPUT for a NULL self, otherwise whatever the
///         Port layer reported.
status_signal_t ir_led_pwm_stop(ir_led_pwm_t* self)
{
    if (self == NULL)
    {
        return DEV_INVALID_INPUT;
    }

    hal_signal_t hw = bsp_pwm_stop(self->pwm_channel);
    if (hw != HW_OK)
    {
        return dev_status_from_hal(hw);
    }

    return update_pwm_status(self);
}

/// @brief raises the duty cycle by increment.
/// @param self instance previously passed to ir_led_pwm_init
/// @param increment percentage points to add. Must be strictly positive.
/// @return DEV_OK on success. DEV_INVALID_INPUT for a NULL self, a non-positive or NaN
///         increment, or a step that would reach IR_LED_MAX_DUTY.
status_signal_t ir_led_pwm_brighten(ir_led_pwm_t* self, const float increment)
{
    if (self == NULL)
    {
        return DEV_INVALID_INPUT;
    }

    /* Written negated so NaN is rejected too. Without this a negative increment slips
       past the IR_LED_MAX_DUTY check below and dims straight through the IR_LED_MIN_DUTY floor. */
    if (!(increment > 0.0f))
    {
        return DEV_INVALID_INPUT;
    }

    if (self->curr_duty + increment >= IR_LED_MAX_DUTY)
    {
        return DEV_INVALID_INPUT;
    }

    hal_signal_t hw = bsp_pwm_set_duty_percent(self->pwm_channel, self->curr_duty + increment);

    if (hw != HW_OK)
    {
        return dev_status_from_hal(hw);
    }

    return update_pwm_status(self);
}

/// @brief lowers the duty cycle by decrement.
/// @param self instance previously passed to ir_led_pwm_init
/// @param decrement percentage points to subtract. Must be strictly positive.
/// @return DEV_OK on success. DEV_INVALID_INPUT for a NULL self, a non-positive or NaN
///         decrement, or a step that would reach IR_LED_MIN_DUTY.
status_signal_t ir_led_pwm_dim(ir_led_pwm_t* self, const float decrement)
{
    if (self == NULL)
    {
        return DEV_INVALID_INPUT;
    }

    // see the note in ir_led_pwm_brighten
    if (!(decrement > 0.0f))
    {
        return DEV_INVALID_INPUT;
    }

    if (self->curr_duty - decrement <= IR_LED_MIN_DUTY)
    {
        return DEV_INVALID_INPUT;
    }

    hal_signal_t hw = bsp_pwm_set_duty_percent(self->pwm_channel, self->curr_duty - decrement);

    if (hw != HW_OK)
    {
        return dev_status_from_hal(hw);
    }

    return update_pwm_status(self);
}

/// @brief reads back the duty cycle the channel is currently running at.
/// @param self instance previously passed to ir_led_pwm_init
/// @param status optional out param carrying the reason for a failed read
/// @return duty cycle as a percentage, or 0.0f on failure. 0.0f is also a legitimate
///         reading, so pass status if the caller needs to tell the two apart.
float ir_led_pwm_get_duty(ir_led_pwm_t* self, status_signal_t* status)
{
    const status_signal_t s = update_pwm_status(self);

    if (status != NULL)
    {
        *status = s;
    }

    // s is never DEV_OK when self is NULL, so the dereference is guarded
    return (s == DEV_OK) ? self->curr_duty : 0.0f;
}

/// @brief reads back the frequency the channel is currently running at.
/// @param self instance previously passed to ir_led_pwm_init
/// @param status optional out param carrying the reason for a failed read
/// @return frequency in Hz, or 0 on failure. 0 is also what a stopped timer reads, so
///         pass status if the caller needs to tell the two apart.
uint32_t ir_led_pwm_get_freq(ir_led_pwm_t* self, status_signal_t* status)
{
    const status_signal_t s = update_pwm_status(self);

    if (status != NULL)
    {
        *status = s;
    }

    // s is never DEV_OK when self is NULL, so the dereference is guarded
    return (s == DEV_OK) ? self->curr_freq : 0u;
}

/// @brief refreshes the cached duty and frequency from the hardware.
/// @param self instance to refresh
/// @return DEV_OK if both reads succeeded, otherwise the mapped reason for the first
///         one that failed.
static status_signal_t update_pwm_status(ir_led_pwm_t* self)
{
    if (self == NULL)
    {
        return DEV_INVALID_INPUT;
    }

    hal_signal_t duty_status = HW_OK;
    hal_signal_t freq_status = HW_OK;

    self->curr_duty = bsp_pwm_get_duty_percent(self->pwm_channel, &duty_status);
    if (duty_status != HW_OK)
    {
        return dev_status_from_hal(duty_status);
    }

    self->curr_freq = bsp_pwm_get_freq_hz(self->pwm_channel, &freq_status);
    if (freq_status != HW_OK)
    {
        return dev_status_from_hal(freq_status);
    }

    return DEV_OK;
}
