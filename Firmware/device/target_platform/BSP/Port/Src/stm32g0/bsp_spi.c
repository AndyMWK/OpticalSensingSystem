#include "bsp_spi.h"

// Platform specific dependencies
#include "stm32g0xx_hal.h"

// external links to STM32 HAL API
extern SPI_HandleTypeDef hspi1;

// utility functions for SPI safety checks
static inline uint8_t bsp_spi_tx_fifo_level_check(bsp_spi_ch ch);
static inline uint8_t bsp_spi_rx_fifo_level_check(bsp_spi_ch ch);

bsp_spi_ch_t spi_ch[BSP_SPI_CHANNEL_COUNT] = {

    {.spi_handle = (void*) &hspi1,
     .cs_io = 0,
     .cs_io_port = 0,

     .dma_handle = NULL,
     .dma_tx_cb = NULL,
     .dma_rx_cb = NULL}};

/// @brief
/// @param ch
/// @param tx_data
/// @param size
/// @return
hal_signal_t bsp_spi_transmit_blocking(bsp_spi_ch ch, const uint8_t* tx_data, const size_t size)
{
    if (!CHECK_VALID_CHANNEL(ch))
    {
        return HW_CHANNEL_INVALID;
    }

    if (HAL_SPI_Transmit(spi_ch[ch].spi_handle, tx_data, (uint16_t) size, HAL_MAX_DELAY) != HAL_OK)
    {
        return HW_SPI_NOT_TRANSMIT;
    }

    return HW_OK;
}

/// @brief
/// @param ch
/// @param rx_data
/// @param size
/// @return
hal_signal_t bsp_spi_receive_blocking(bsp_spi_ch ch, uint8_t* rx_data, const size_t size)
{
    if (!CHECK_VALID_CHANNEL(ch))
    {
        return HW_CHANNEL_INVALID;
    }

    if (HAL_SPI_Receive(spi_ch[ch].spi_handle, rx_data, (uint16_t) size, HAL_MAX_DELAY) != HAL_OK)
    {
        return HW_SPI_NOT_RECEIVE;
    }

    return HW_OK;
}

/// @brief
/// @param ch
/// @param tx_data
/// @param size
/// @return
hal_signal_t bsp_spi_transmit_dma(const bsp_spi_ch ch, const uint8_t* tx_data, const size_t size)
{
    if (!CHECK_VALID_CHANNEL(ch))
    {
        return HW_CHANNEL_INVALID;
    }

    if (spi_ch[ch].dma_tx_cb == NULL)
    {
        return HW_NO_CALLBACK;
    }

    __disable_irq();

    if (HAL_SPI_Transmit_DMA(spi_ch[ch].spi_handle, tx_data, size) != HAL_OK)
    {
        __enable_irq();
        return HW_SPI_NOT_TRANSMIT;
    }

    __enable_irq();

    return HW_OK;
}

/// @brief
/// @param ch
/// @param rx_data
/// @param size
/// @return
hal_signal_t bsp_spi_receive_dma(const bsp_spi_ch ch, uint8_t* rx_data, const size_t size)
{
    if (!CHECK_VALID_CHANNEL(ch))
    {
        return HW_CHANNEL_INVALID;
    }

    if (spi_ch[ch].dma_rx_cb == NULL)
    {
        return HW_NO_CALLBACK;
    }

    // Check to see if the FIFO TX Buffer is full
    SPI_HandleTypeDef* hspi = spi_ch[ch].spi_handle;
    if (hspi->Instance->SR & (SPI_SR_FTLVL_0 | SPI_SR_FTLVL_1))
    {
        return HW_SPI_TX_FIFO_FULL;
    }

    __disable_irq();

    if (HAL_SPI_Receive_DMA(spi_ch[ch].spi_handle, rx_data, size) != HAL_OK)
    {
        __enable_irq();
        return HW_SPI_NOT_RECEIVE;
    }

    __enable_irq();

    return HW_OK;
}

/// @brief
/// @param ch
/// @param cb
/// @return
hal_signal_t bsp_register_spi_tx_dma_cb(const bsp_spi_ch ch, const spi_tx_dma_cb_t cb)
{
    if (!CHECK_VALID_CHANNEL(ch))
    {
        return HW_CHANNEL_INVALID;
    }

    if (cb == NULL)
    {
        return HW_BAD_CALLBACK;
    }

    spi_ch[ch].dma_tx_cb = cb;

    return HW_OK;
}

/// @brief
/// @param ch
/// @param cb
/// @return
hal_signal_t bsp_register_spi_rx_dma_cb(const bsp_spi_ch ch, const spi_rx_dma_cb_t cb)
{
    if (!CHECK_VALID_CHANNEL(ch))
    {
        return HW_CHANNEL_INVALID;
    }

    if (cb == NULL)
    {
        return HW_BAD_CALLBACK;
    }

    spi_ch[ch].dma_rx_cb = cb;

    return HW_OK;
}

static inline uint8_t bsp_spi_tx_fifo_level_check(bsp_spi_ch ch)
{
    // Check to see if the FIFO TX Buffer is full
    SPI_HandleTypeDef* hspi = spi_ch[ch].spi_handle;

    return (hspi->Instance->SR & (SPI_SR_FTLVL_0 | SPI_SR_FTLVL_1));
}

static inline uint8_t bsp_spi_rx_fifo_level_check(bsp_spi_ch ch)
{
    // Check to see if the FIFO RX Buffer is full
    SPI_HandleTypeDef* hspi = spi_ch[ch].spi_handle;

    return ~(hspi->Instance->SR & (SPI_SR_FRLVL_0 | SPI_SR_FRLVL_1));
}