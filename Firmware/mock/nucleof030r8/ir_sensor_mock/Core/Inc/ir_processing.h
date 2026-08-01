#ifndef IR_PROCESSING_H
#define IR_PROCESSING_H

#include "main.h"
#include "message_types.h"
#include "fifo.h"
#include "ir_adc.h"
#include <math.h>
#include <float.h>

/*

IR distance equation should give a decaying function that is proportional to 1/d^2

We model the sensor with the equation: 

Distance = (GAIN x sqrt(COEFF_A / VOLTAGE_TO_IRRADIANCE * V)) + OFFSET

Should be able to check the actual math in the Vishay Optics Docs. 

*/

#define GAIN 1.0f
#define OFFSET 3.0f
#define VOLTAGE_TO_IRRADIANCE  0.5f
#define COEFF_A 0.7f
#define VREF 3.3f

#define RATE_LIMIT 700
#define OUT_OF_RANGE_LIMIT 100    // 40mV is the out of range limit voltage  (40mV / 3.3V) * 4095 LSB
#define SATURATION_LIMIT 3980    // 3.2V is the saturation voltage          (3.2V / 3.3v) * 4095 LSB

#define SMOOTHING_FACTOR_EMA 0.5f

/// @brief stores the previous value recorded by the EMA LPF 
typedef struct ema_lpf_cache_t {

    uint16_t pd1_adc_prev;
    uint16_t pd2_adc_prev;

} ema_lpf_cache_t;

/// @brief stores previous values and coefficients for the FIR LPF
typedef struct fir_lpf_cache_t {

    // to be completed
    
} fir_lpf_cache_t;

// Public API functions

/// @brief function to stream the latest element from each photodiode FIFO.
void stream_from_fifo(fifo_t* fifo_pd1, fifo_t* fifo_pd2, data_msg_t* data_msg);

/// @brief function to average the fifo data and pack into a data message. 
void stream_average_fifo(fifo_t* fifo_pd1, fifo_t* fifo_pd2, data_msg_t* data_msg);

/// @brief function to stream the ADC values via FIR LPF
void stream_from_fifo_fir_lpf(fifo_t* fifo_pd1, fifo_t* fifo_pd2, data_msg_t* data_msg);

void stream_from_fifo_ema_lpf(fifo_t* fifo_pd1, fifo_t* fifo_pd2, data_msg_t* data_msg);

#endif
