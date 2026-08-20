#include "bmi323_imu.h"

// dma callback functions
void bmi323_imu_tx_cb(void);
void bmi323_imu_rx_cb(void);

/// @brief
/// @param self
/// @param ch
/// @return
status_signal_t bmi323_imu_init(bmi323_imu_t* self, const bsp_spi_ch ch)
{
    if (self == NULL)
    {
        return DEV_NOT_INIT;
    }

    if (!CHECK_VALID_CHANNEL_SPI(ch))
    {
        return DEV_CHANNEL_NOT_AVAILABLE;
    }

    self->spi_channel = ch;

    // Register DMA callbacks
    bsp_register_spi_tx_dma_cb(ch, bmi323_imu_tx_cb);
    bsp_register_spi_rx_dma_cb(ch, bmi323_imu_rx_cb);

    // all done on spi blocking mode

    // read chip ID

    // send dummy byte on spi

    // begin self calibration
    return DEV_OK;
}

status_signal_t bmi323_imu_read_accel(uint16_t* buffer, size_t buffer_size);

status_signal_t bmi323_imu_read_gyro(uint16_t* buffer, size_t buffer_size);

status_signal_t bmi323_imu_read_temp(uint16_t* buffer, size_t buffer_size);