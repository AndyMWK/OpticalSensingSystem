#ifndef BSP_ADC_H
#define BSP_ADC_H

#include "hal_signals.h"

typedef void (*adc_dma_cb_t)(void*);

typedef enum bsp_adc_ch
{
    BSP_PHOTODIODE_ADC_1,
    BSP_PHOTODIODE_ADC_2,
    BSP_ADC_CHANNEL_COUNT
} bsp_adc_ch;

typedef struct bsp_adc_ch_t
{
    void* adc_handle;
    void* dma_handle;

    adc_dma_cb_t dma_cb;
} bsp_adc_ch_t;

// Macros
#define CHECK_VALID_CHANNEL_ADC(adc_channel) (((adc_channel) < BSP_ADC_CHANNEL_COUNT) ? 1 : 0)

hal_signal_t bsp_start_adc_conv(const bsp_adc_ch ch);
hal_signal_t bsp_stop_adc_conv(const bsp_adc_ch ch);
hal_signal_t bsp_register_adc_dma_cb(const bsp_adc_ch, const adc_dma_cb_t cb);


#endif