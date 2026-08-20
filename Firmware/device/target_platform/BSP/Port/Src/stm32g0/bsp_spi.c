#include "bsp_spi.h"

// Platform specific dependencies
#include "stm32g0xx_hal.h"

// external links to STM32 HAL API
extern SPI_HandleTypeDef hspi1;

// utility functions for SPI safety checks
static inline uint8_t bsp_spi_tx_fifo_level_check(const bsp_spi_ch ch);
static inline uint8_t bsp_spi_rx_fifo_level_check(const bsp_spi_ch ch);

bsp_spi_ch_t spi_ch[BSP_SPI_CHANNEL_COUNT] = {

    {.spi_handle = (void*) &hspi1,
     .cs_io = 0,
     .cs_io_port = 0,

     .dma_handle = NULL,
     .dma_tx_cb = NULL,
     .dma_rx_cb = NULL,
     .tx_dma_empty = 1}};

/// @brief
/// @param ch
/// @param tx_data
/// @param size
/// @return
hal_signal_t bsp_spi_transmit_blocking(const bsp_spi_ch ch, const uint8_t* tx_data,
                                       const size_t size)
{
    if (!CHECK_VALID_CHANNEL_SPI(ch))
    {
        return HW_CHANNEL_INVALID;
    }

    if (tx_data == NULL || size == 0)
    {
        return HW_SPI_TX_NULL;
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
hal_signal_t bsp_spi_receive_blocking(const bsp_spi_ch ch, uint8_t* rx_data, const size_t size)
{
    if (!CHECK_VALID_CHANNEL_SPI(ch))
    {
        return HW_CHANNEL_INVALID;
    }

    if (rx_data == NULL || size == 0)
    {
        return HW_SPI_RX_NULL;
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
    if (!CHECK_VALID_CHANNEL_SPI(ch))
    {
        return HW_CHANNEL_INVALID;
    }

    if (tx_data == NULL || size == 0)
    {
        return HW_SPI_TX_NULL;
    }

    if (spi_ch[ch].dma_tx_cb == NULL)
    {
        return HW_NO_CALLBACK;
    }

    if (!spi_ch[ch].tx_dma_empty)
    {
        return HW_BUSY;
    }

    __disable_irq();

    if (HAL_SPI_Transmit_DMA(spi_ch[ch].spi_handle, tx_data, size) != HAL_OK)
    {
        __enable_irq();
        return HW_SPI_NOT_TRANSMIT;
    }

    spi_ch[ch].tx_dma_empty = 0;

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
    if (!CHECK_VALID_CHANNEL_SPI(ch))
    {
        return HW_CHANNEL_INVALID;
    }

    if (rx_data == NULL || size == 0)
    {
        return HW_SPI_RX_NULL;
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
    if (!CHECK_VALID_CHANNEL_SPI(ch))
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
    if (!CHECK_VALID_CHANNEL_SPI(ch))
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

/// @brief
/// @param hspi spi handle given to the Callback function in STM32's API
void bsp_spi_tx_dma_cb_pipe(void* hspi)
{
    if (hspi == NULL)
    {
        return;
    }

    SPI_HandleTypeDef* hspi_casted = (SPI_HandleTypeDef*) hspi;

    // check if any of the channels in the listed spi channel has the same spi handle as the one
    // that invoked the interrupt. To Do: not sure how efficient this method may be. Might be worth
    // it to just hard code the handlers.
    for (uint8_t channel = BSP_SPI_BMI323_IMU; channel < BSP_SPI_CHANNEL_COUNT; channel++)
    {
        SPI_HandleTypeDef* channel_spi_handle = (SPI_HandleTypeDef*) spi_ch[channel].spi_handle;

        if (channel_spi_handle == NULL)
        {
            continue;
        }

        if (channel_spi_handle->Instance == hspi_casted->Instance)
        {
            spi_ch[channel].tx_dma_empty = 1;

            if (spi_ch[channel].dma_tx_cb == NULL)
            {
                return;
            }
            else
            {
                // finally call the DMA handler function associated with the SPI channel
                spi_ch[channel].dma_tx_cb();
            }
        }
    }
}

/// @brief checks the SPI TX FIFO level
/// @param ch SPI Channel that needs to be checked
/// @return returns 0 if TX FIFO full otherwise returns value > 0.

/*
FTLVL[1:0]: FIFO transmission level
These bits are set and cleared by hardware.
00: FIFO empty
01: 1/4 FIFO
10: 1/2 FIFO
*/

static inline uint8_t bsp_spi_tx_fifo_level_check(const bsp_spi_ch ch)
{
    // Check to see if the FIFO TX Buffer is full
    SPI_HandleTypeDef* hspi = spi_ch[ch].spi_handle;

    return ~(hspi->Instance->SR & (SPI_SR_FTLVL_0 | SPI_SR_FTLVL_1)) >> 11;
}

/// @brief checks the SPI RX FIFO level
/// @param ch
/// @return returns 0 if RX FIFO full otherwise returns value > 0

/*
FRLVL[1:0]: FIFO reception level
These bits are set and cleared by hardware.
00: FIFO empty
01: 1/4 FIFO
10: 1/2 FIFO
11: FIFO full
*/

static inline uint8_t bsp_spi_rx_fifo_level_check(const bsp_spi_ch ch)
{
    // Check to see if the FIFO RX Buffer is full
    SPI_HandleTypeDef* hspi = spi_ch[ch].spi_handle;

    return ~(hspi->Instance->SR & (SPI_SR_FRLVL_0 | SPI_SR_FRLVL_1)) >> 9;
}