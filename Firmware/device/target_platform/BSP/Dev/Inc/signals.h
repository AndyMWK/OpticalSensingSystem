#ifndef DEV_SIGNALS_H
#define DEV_SIGNALS_H

#include <stdint.h>
#include <stdio.h>
#include "hal_signals.h"

typedef enum status_signal_t
{

    DEV_OK = 1,
    DEV_CHANNEL_NOT_AVAILABLE,
    DEV_NOT_INIT,
    DEV_STOP_FAIL,
    DEV_TIMEOUT,
    DEV_INVALID_INPUT,
    DEV_STAT_UPDATE_FAILED,
    DEV_BUSY

} status_signal_t;

#endif