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
    HW_CHANNEL_INVALID
} hal_signal_t;

#endif