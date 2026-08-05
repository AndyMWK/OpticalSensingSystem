#ifndef BSP_SPI_H
#define BSP_SPI_H

#include <stdint.h>
#include "hal_signals.h"

typedef enum bsp_spi_ch
{
    SPI_BMI323_IMU,
    SPI_CHANNEL_COUNT
} bsp_spi_ch;

/// @brief stores the spi bus context.
typedef struct bsp_spi_ch_t
{
    uint16_t* tx_buf;
    uint16_t* rx_buf;

    // generic types to abstract any HAL implementation
    void* spi_handle;
    void* dma_handle;

} bsp_spi_ch_t;


#endif