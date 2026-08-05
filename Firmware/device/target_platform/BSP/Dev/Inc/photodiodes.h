#ifndef PHOTODIODES_H
#define PHOTODIODES_H

#include "signals.h"
#include "hal_io_adc.h"

status_signal_t pd_sensor_init(uint16_t sensor_id);

status_signal_t pd_sensor_get_distance_cm(uint16_t* distance);

status_signal_t pd_sensor_enable_ema(void);

status_signal_t pd_sensor_disable_ema(void);

status_signal_t pd_sensor_deinit(void);

#endif