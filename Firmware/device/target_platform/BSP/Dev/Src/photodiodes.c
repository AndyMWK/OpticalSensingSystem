#include "photodiodes.h"

// statically allocate all the needed memory initially.
static uint16_t adc_cache[BSP_ADC_CHANNEL_COUNT][ADC_BUFFER_SIZE];

status_signal_t pd_sensor_init(photodiode_sensor_t* self, const bsp_adc_ch ch)
{
    if (self == NULL)
    {
        return DEV_NOT_INIT;
    }

    if (!CHECK_VALID_CHANNEL_ADC(ch))
    {
        return DEV_INVALID_INPUT;
    }

    self->ch = ch;

    // this is potentially corruptable memory. Might be better to use a 2D array instead
    self->adc_cached = &adc_cache[ch];
    self->buffer_size = ADC_BUFFER_SIZE;

    return DEV_OK;
}