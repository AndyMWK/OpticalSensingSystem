#ifndef BMI323_H
#define BMI323_H

#include "signals.h"
#include "bsp_spi.h"
#include "bsp_i2c.h"
#include "bsp_timer.h"

typedef enum bmi323_imu_acc_range_t
{
    BMI323_IMU_2G,
    BMI323_IMU_4G,
    BMI323_IMU_8G,
    BMI323_IMU_16G
} bmi323_imu_acc_range_t;

typedef enum bmi323_imu_gyro_range_t
{
    BMI323_IMU_125DEG,
    BMI323_IMU_250DEG,
    BMI323_IMU_500DEG,
    BMI323_IMU_1000DEG,
    BMI323_IMU_2000DEG
} bmi323_imu_gyro_range_t;

typedef enum bmi323_imu_power_mode_t
{
    BMI323_IMU_SUSPEND_MODE,
    BMI323_IMU_LOW_POWER_MODE,
    BMI323_IMU_HIGH_PERFORMANCE_MODE,
    BMI323_IMU_NORMAL_MODE
} bmi323_imu_power_mode_t;

typedef enum bmi323_imu_interrupt_mode_t
{
    INTERRUPT_DISABLED = 0b00,
    INTERRUPT_INT1,
    INTERRUPT_INT2,
    INTERRUPT_I3C_IBI
} bmi323_imu_interrupt_mode_t;


typedef struct bmi323_imu_t
{
    bsp_spi_ch spi_channel;

    // state indicators
    bmi323_imu_acc_range_t acc_range;
    bmi323_imu_gyro_range_t gyro_range;
    bmi323_imu_power_mode_t power_mode;
    bmi323_imu_interrupt_mode_t isr_mode;

} bmi323_imu_t;

status_signal_t bmi323_imu_init(bmi323_imu_t* self, const bsp_spi_ch ch);

status_signal_t bmi323_imu_read_accel(uint16_t* buffer, size_t buffer_size);

status_signal_t bmi323_imu_read_gyro(uint16_t* buffer, size_t buffer_size);

status_signal_t bmi323_imu_read_temp(uint16_t* buffer, size_t buffer_size);

#endif