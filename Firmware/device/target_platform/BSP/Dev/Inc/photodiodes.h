#ifndef PHOTODIODES_H
#define PHOTODIODES_H

#include "signals.h"
#include "bsp_adc.h"

#define ADC_BUFFER_SIZE 4

typedef struct photodiode_sensor_t
{
    bsp_adc_ch ch;

    uint16_t* adc_cached;
    size_t buffer_size;

} photodiode_sensor_t;

status_signal_t pd_sensor_init(photodiode_sensor_t* self, const bsp_adc_ch ch);

status_signal_t pd_sensor_get_distance_cm(photodiode_sensor_t* self, uint16_t* distance);

status_signal_t pd_sensor_enable_ema(void);

status_signal_t pd_sensor_disable_ema(void);

status_signal_t pd_sensor_deinit(void);

#endif