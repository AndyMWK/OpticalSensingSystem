#ifndef IR_ADC_H
#define IR_ADC_H

#include "main.h"
#include "message_types.h"
#include "fifo.h"

#define NUM_PHOTODIODES 2
//---Internal data structures for the IR ADC module---

/// @brief stores the internal peripheral handles of the ADC. 
typedef struct IrAdcConfig_t {

    ADC_HandleTypeDef* hadc;
    DMA_HandleTypeDef* hdma_adc;
    RTC_HandleTypeDef* hrtc;

} IrAdcConfig_t;

/// @brief stores the internal state of the ADC. 
typedef enum IrAdcState_t {
    IR_ADC_STATE_ACTIVE,
    IR_ADC_STATE_ERROR
} IrAdcState_t;

/// @brief overall structure to represent the ADC context. 
/// @param config is the configuration of the current adc, containing the peripheral handles
/// @param state indicates the current state of the ADC. 
/// @param adc_buffer stores the most recent adc readings. 
typedef struct IrAdcContext_t {
    IrAdcConfig_t config;
    IrAdcState_t state;
    uint16_t adc_buffer[NUM_PHOTODIODES];
} IrAdcContext_t;

// Public API functions

/// @brief Initializes the IR ADC module with the given configuration. 
/// @param config Wrapper for the peripherals that the ADC module uses. 
/// @param context Pointer to the context structure for the ADC module. 
void ir_adc_init(IrAdcConfig_t *config, IrAdcContext_t* context);

/// @brief is called during the callback function for when the ADC DMA completes. 
/// @param context is stores the ADC representative information.
/// @param fifo is the global fifo to store incoming ADC.
/// @param buffer is the buffer where DMA allocates incoming ADC data. 
void adc_conversion_complete_callback(IrAdcContext_t* context, fifo_t* fifo_pd1, fifo_t* fifo_pd2, uint32_t* buffer);

#endif
