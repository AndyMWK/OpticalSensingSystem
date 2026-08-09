#include "bsp_pwm.h"
#include "bsp_timer.h"

// Platform Specific Dependencies
#include "stm32g0xx_hal.h"

// external links to STM32 HAL API
extern TIM_HandleTypeDef htim16;

static volatile uint32_t* ret_ccr(const bsp_pwm_t ch);


static bsp_pwm_ch_t pwm_channels[BSP_PWM_COUNT] = {

    // BSP IR LED channel
    {.duty = 50,
     .frequency = 1000,

     .timer_handle = (void*) &htim16,

     /* In STM32 HAL API, TIM_CHANNEL_x already has address increments.

     Example:

     TIM_CHANNEL_1 evaluates to 0x00.

     &CCR1 + TIM_CHANNEL_1 = &CCR1 (0x34)

     TIM_CHANNEL_2 evaluates to 0x04

     &CCR1 + TIM_CHANNEL_2 = &CCR2 (0x38)

     */

     .timer_channel = TIM_CHANNEL_1,
     .gpio_handle = NULL}

};

/// @brief begins the PWM channel based on its initial configuration
/// @param ch PWM channel
/// @return returns HW_OK on success and HW_NOT_INIT if not initialzied
hal_signal_t bsp_pwm_start(const bsp_pwm_t ch)
{
    if (!CHECK_VALID_CHANNEL(ch))
    {
        return HW_CHANNEL_INVALID;
    }

    if (HAL_TIM_PWM_Start((TIM_HandleTypeDef*) pwm_channels[ch].timer_handle,
                          pwm_channels[ch].timer_channel) != HAL_OK)
    {
        return HW_NOT_INIT;
    }

    return HW_OK;
}

/// @brief
/// @param ch
/// @return
hal_signal_t bsp_pwm_stop(const bsp_pwm_t ch)
{
    if (!CHECK_VALID_CHANNEL(ch))
    {
        return HW_CHANNEL_INVALID;
    }

    if (HAL_TIM_PWM_Stop((TIM_HandleTypeDef*) pwm_channels[ch].timer_handle,
                         pwm_channels[ch].timer_channel) != HAL_OK)
    {
        return HW_DEINIT_FAIL;
    }

    return HW_OK;
}

/// @brief
/// @param ch
/// @param hz
/// @return
hal_signal_t bsp_pwm_set_freq_hz(const bsp_pwm_t ch, const uint32_t hz)
{
    if (!CHECK_VALID_CHANNEL(ch))
    {
        return HW_CHANNEL_INVALID;
    }

    TIM_HandleTypeDef* htim = (TIM_HandleTypeDef*) pwm_channels[ch].timer_handle;
}

/// @brief
/// @param ch
/// @param percent
/// @return
hal_signal_t bsp_pwm_set_duty_percent(const bsp_pwm_t ch, const float percent)
{
    if (!CHECK_VALID_CHANNEL(ch))
    {
        return HW_CHANNEL_INVALID;
    }

    // check for invalid percent values
    float pct = percent;

    if (!(pct >= 0.0f))
    {
        /*
        Just return from a bad CCR value.
        If NaN or negative percentage, the device should not just change its PWM to a given value.
        */
        return HW_CCR_INVALID;
    }

    if (pct > BSP_DUTY_CYCLE_CAP)
        pct = BSP_DUTY_CYCLE_CAP;

    // retrieve ccr register for the selected channel. ret_ccr validates the timer handle,
    // so this has to happen before htim is dereferenced below.
    volatile uint32_t* ccr = ret_ccr(ch);
    if (ccr == NULL)
    {
        return HW_NOT_INIT;
    }

    TIM_HandleTypeDef* htim = (TIM_HandleTypeDef*) pwm_channels[ch].timer_handle;

    const float period = (float) htim->Instance->ARR + 1.0f;

    *ccr = (uint32_t) (period * (pct / 100.0f));

    return HW_OK;
}

/// @brief
/// @param ch
/// @param status
/// @return
float bsp_pwm_get_duty_percent(const bsp_pwm_t ch, hal_signal_t* status)
{
    if (!CHECK_VALID_CHANNEL(ch))
    {
        if (status)
        {
            *status = HW_CHANNEL_INVALID;
        }

        return 0.0f;
    }

    // use a local variable to point to the timer handle
    TIM_HandleTypeDef* htim = (TIM_HandleTypeDef*) pwm_channels[ch].timer_handle;

    if (htim == NULL || htim->Instance == NULL || htim->Instance->ARR == 0u)
    {
        if (status)
            *status = HW_NOT_INIT;
        return 0.0f;
    }

    // retrieve period and ccr register values
    const uint32_t period = htim->Instance->ARR + 1u;

    volatile uint32_t* ccr = ret_ccr(ch);
    if (ccr == NULL)
    {
        if (status)
            *status = HW_NOT_INIT;
        return 0.0f;
    }

    if (status)
        *status = HW_OK;

    // return final value
    return (float) ((float) (*ccr) / (float) period) * 100.0f;
}

/// @brief reads back the PWM frequency the channel is currently running at.
///
/// Derived from the timer registers rather than from pwm_channels[ch].frequency, so the
/// value stays true even if the timer was configured outside this module (CubeMX does
/// exactly that for TIM16).
///
/// @param ch PWM channel
/// @param status optional out param. HW_OK on success, HW_CHANNEL_INVALID for an unknown
///               channel, HW_NOT_INIT if the timer is unbound or unconfigured.
/// @return frequency in Hz, saturated at UINT16_MAX. 0 on any failure, which is also
///         indistinguishable from a stopped timer.
uint32_t bsp_pwm_get_freq_hz(const bsp_pwm_t ch, hal_signal_t* status)
{
    if (!CHECK_VALID_CHANNEL(ch))
    {
        if (status)
        {
            *status = HW_CHANNEL_INVALID;
        }

        return 0u;
    }

    TIM_HandleTypeDef* htim = (TIM_HandleTypeDef*) pwm_channels[ch].timer_handle;

    if (htim == NULL || htim->Instance == NULL || htim->Instance->ARR == 0u)
    {
        if (status)
            *status = HW_NOT_INIT;
        return 0u;
    }

    const uint32_t prescale = (uint32_t) htim->Instance->PSC + 1u;
    const uint32_t period = htim->Instance->ARR + 1u;

    /* Divide in two steps rather than forming (prescale * period): that product overflows
       uint32_t at the top of both register ranges, and widening to uint64_t would drag the
       64 bit division helper into a Cortex-M0+ image. The cost is a truncation in the
       first divide, which only bites at prescaler values that do not divide CPU_FCLK
       evenly. */
    const uint32_t hz = ((uint32_t) CPU_FCLK / prescale) / period;

    if (status)
        *status = HW_OK;

    return hz;
}

/// @brief returns the CCR_x register for the selected pwm channel
/// @param ch PWM channel. Must already be range checked by the caller.
/// @return pointer to the channel's CCR register, or NULL if the channel has no timer
///         handle bound to it yet. Callers must check for NULL before dereferencing.
static volatile uint32_t* ret_ccr(const bsp_pwm_t ch)
{
    TIM_HandleTypeDef* htim = (TIM_HandleTypeDef*) pwm_channels[ch].timer_handle;

    if (htim == NULL || htim->Instance == NULL)
    {
        return NULL;
    }

    return (volatile uint32_t*) ((uintptr_t) &htim->Instance->CCR1 +
                                 pwm_channels[ch].timer_channel);
}
