#ifndef HAL_SIGNALS_H
#define HAL_SIGNALS_H

#include <stdio.h>
#include <stdint.h>

typedef enum hal_signal_t
{
    HW_OK = 1,
    HW_BUSY,
    HW_NOT_INIT,
    HW_DEINIT_FAIL,
    HW_CHANNEL_INVALID,
    HW_SPI_NOT_TRANSMIT,
    HW_SPI_TX_NULL,
    HW_SPI_RX_NULL,
    HW_SPI_NOT_RECEIVE,
    HW_NO_CALLBACK,
    HW_BAD_CALLBACK,
    HW_SPI_TX_FIFO_FULL,
    HW_CCR_INVALID,
    HW_ARR_UNSAFE
} hal_signal_t;

#endif