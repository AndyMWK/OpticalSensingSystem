#ifndef BSP_I2C_H
#define BSP_I2C_H

#include "hal_signals.h"

typedef enum bsp_i2c_ch
{
    BSP_I2C_BQ76925,
    BSP_I2C_CHANNEL_COUNT
} bsp_i2c_ch;

typedef void (*i2c_tx_dma_cb)(void*);
typedef void (*i2c_rx_dma_cb)(void*);

typedef struct bsp_i2c_ch_t
{
    // Peripheral Handles
    void* i2c_handle;
    void* dma_handle;

    // DMA callbacks
    i2c_tx_dma_cb tx_dma_cb;
    i2c_rx_dma_cb rx_dma_cb;
} bsp_i2c_ch_t;

hal_signal_t bsp_i2c_transmit_blocking(const bsp_i2c_ch ch, const uint16_t* tx_data,
                                       const size_t size);
hal_signal_t bsp_i2c_receive_blocking(const bsp_i2c_ch ch, uint16_t* rx_data, const size_t size);

hal_signal_t bsp_i2c_transmit_dma(const bsp_i2c_ch ch, const uint16_t* tx_data, const size_t size);
hal_signal_t bsp_i2c_receive_dma(const bsp_i2c_ch ch, uint16_t* rx_data, const size_t size);

hal_signal_t bsp_register_i2c_tx_dma_cb(const i2c_tx_dma_cb tx_cb, const i2c_rx_dma_cb rx_cb);

#endif