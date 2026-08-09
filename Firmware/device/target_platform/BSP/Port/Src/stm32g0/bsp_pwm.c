#include "bsp_pwm.h"
#include "bsp_timer.h"

// Platform Specific Dependencies
#include "stm32g0xx_hal.h"

// external links to STM32 HAL API
extern TIM_HandleTypeDef htim16;

static volatile uint32_t* ret_ccr(const bsp_pwm_t ch, uint8_t* status);


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
hal_signal_t bsp_pwm_set_freq_hz(const bsp_pwm_t ch, const uint16_t hz)
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
        pct = 0.0f; /* also catches NaN */
    if (pct > DUTY_CYCLE_CAP)
        pct = DUTY_CYCLE_CAP;

    // use a local variable to point to the timer handle
    TIM_HandleTypeDef* htim = (TIM_HandleTypeDef*) pwm_channels[ch].timer_handle;

    // retrieve ccr register for the selected channel
    uint8_t status = 1;
    volatile uint32_t* ccr = ret_ccr(ch, &status);
    if (!status)
        return HW_NOT_INIT;

    *ccr = (uint32_t) ((float) htim->Instance->ARR * ((float) pct / 100.0f));

    return HW_OK;
}

/// @brief
/// @param ch
/// @param status
/// @return
float bsp_pwm_get_duty_percent(bsp_pwm_t ch, hal_signal_t* status)
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

    if (htim == NULL || htim->Instance->ARR == 0u)
    {
        if (status)
            *status = HW_NOT_INIT;
        return 0.0f;
    }

    // retrieve period and ccr register values
    const uint32_t period = htim->Instance->ARR + 1u;

    uint8_t ccr_call_status = 1;
    volatile uint32_t* ccr = ret_ccr(ch, &ccr_call_status);

    // check ccr register call
    if (!ccr_call_status)
    {
        // should have more failure modes.
        if (status)
            *status = HW_NOT_INIT;
        return 0.0f;
    }

    if (status)
        *status = HW_OK;

    // return final value
    return (float) ((float) (*ccr) / (float) period) * 100.0f;
}

/// @brief returns the CCR_x registers for the selected pwm channel
/// @param ch
/// @param status
/// @return
static volatile uint32_t* ret_ccr(const bsp_pwm_t ch, uint8_t* status)
{
    TIM_HandleTypeDef* htim = (TIM_HandleTypeDef*) pwm_channels[ch].timer_handle;
    return (volatile uint32_t*) ((uintptr_t) &htim->Instance->CCR1 +
                                 pwm_channels[ch].timer_channel);
}
