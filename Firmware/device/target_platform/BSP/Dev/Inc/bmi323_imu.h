#ifndef BMI323_H
#define BMI323_H

#include "signals.h"
#include "bsp_spi.h"
#include "bsp_i2c.h"
#include "bsp_timer.h"

status_signal_t bmi323_imu_init(void);

status_signal_t bmi323_imu_read_accel(uint16_t* buffer, uint16_t buffer_size);

status_signal_t bmi323_imu_read_gyro(uint16_t* buffer, uint16_t buffer_size);


#endif