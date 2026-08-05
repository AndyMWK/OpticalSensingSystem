#ifndef DEV_SIGNALS_H
#define DEV_SIGNALS_H

#include <stdint.h>
#include <stdio.h>

typedef enum status_signal_t
{

    OK = 1,
    DEV_NOT_INIT,
    HARDWARE_TIMEOUT,
    HARDWARE_BUSY,
    BUFFER_NOT_INIT

} status_signal_t;

#endif