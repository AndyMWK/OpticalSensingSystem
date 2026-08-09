#ifndef DEV_SIGNALS_H
#define DEV_SIGNALS_H

#include <stdint.h>
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

/// @brief maps a Port layer HAL status onto the Dev layer status it corresponds to.
///
/// Every Dev driver translates errors here rather than keeping its own switch, so the
/// mapping stays consistent as either enum grows.
///
/// @param s status returned by any bsp_* call
/// @return the equivalent status_signal_t. HAL values this layer does not model (the
///         HW_SPI_* and callback failures) collapse to DEV_STAT_UPDATE_FAILED.
static inline status_signal_t dev_status_from_hal(hal_signal_t s)
{
    switch (s)
    {
        case HW_OK:
            return DEV_OK;

        case HW_CHANNEL_INVALID:
            return DEV_CHANNEL_NOT_AVAILABLE;

        case HW_NOT_INIT:
            return DEV_NOT_INIT;

        case HW_BUSY:
            return DEV_BUSY;

        case HW_DEINIT_FAIL:
            return DEV_STOP_FAIL;

        default:
            return DEV_STAT_UPDATE_FAILED;
    }
}

#endif