#ifndef BSP_SPI_H
#define BSP_SPI_H

#include <stdint.h>
#include "hal_signals.h"

// Macros
#define CHECK_VALID_CHANNEL_SPI(channel) (channel >= BSP_SPI_CHANNEL_COUNT ? 0 : 1)

// SPI DMA Callback Functions
typedef void (*spi_tx_dma_cb_t)(void);
typedef void (*spi_rx_dma_cb_t)(void);

typedef void (*spi_tx_isr_cb_t)(void);

typedef enum bsp_spi_ch
{
    BSP_SPI_BMI323_IMU,
    BSP_SPI_CHANNEL_COUNT
} bsp_spi_ch;

/// @brief stores the spi bus context.
typedef struct bsp_spi_ch_t
{
    uint8_t cs_io;
    uint8_t cs_io_port;

    // generic types to abstract any HAL implementation
    void* spi_handle;
    void* dma_handle;

    // register some DMA and Interrupt callback functions using a function pointer.
    spi_tx_dma_cb_t dma_tx_cb;
    spi_rx_dma_cb_t dma_rx_cb;

    // states to indicate whether the Transmission/Reception for DMA is available
    uint8_t tx_dma_empty;

} bsp_spi_ch_t;

hal_signal_t bsp_spi_transmit_blocking(const bsp_spi_ch ch, const uint8_t* tx_data,
                                       const size_t size);
hal_signal_t bsp_spi_receive_blocking(const bsp_spi_ch ch, uint8_t* rx_data, const size_t size);

hal_signal_t bsp_spi_transmit_receive(const bsp_spi_ch ch, const uint8_t* tx_data, uint8_t* rx_data,
                                      const size_t tx_size, size_t* rx_size);

hal_signal_t bsp_spi_transmit_dma(const bsp_spi_ch ch, const uint8_t* tx_data, const size_t size);
hal_signal_t bsp_spi_receive_dma(const bsp_spi_ch ch, uint8_t* tx_data, const size_t size);

hal_signal_t bsp_register_spi_tx_dma_cb(const bsp_spi_ch ch, const spi_tx_dma_cb_t cb);
hal_signal_t bsp_register_spi_rx_dma_cb(const bsp_spi_ch ch, const spi_rx_dma_cb_t cb);

void bsp_spi_tx_dma_cb_pipe(void* hspi);

#endif