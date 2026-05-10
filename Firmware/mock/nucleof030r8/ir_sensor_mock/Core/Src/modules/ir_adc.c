#include "ir_adc.h"

// private helper functions
static void pack_adc_msg(IrAdcContext_t* context, adc_msg_t* msg, photodiode_t sensor);

// Definitions of Public API functions
void ir_adc_init(IrAdcConfig_t *config, IrAdcContext_t* context) {

    // check for any null pointers
    if(config == NULL || context == NULL) {
        return;
    }

    // check for any null pointers in the config struct
    if(config->hadc == NULL || config->hdma_adc == NULL || config->hrtc == NULL) {
        return;
    }

    // initialize all elements of the context structure. 
    context->config = *config;
    context->state = IR_ADC_STATE_ACTIVE;

    // all readings are set to zero so far. 
    for(int i = 0; i < NUM_PHOTODIODES; i++) {
        context->adc_buffer[i] = 0;
    }
}

void adc_conversion_complete_callback(IrAdcContext_t* context, fifo_t* fifo, uint32_t* buffer) {

    // check for any null pointers
    if(context == NULL || fifo == NULL || buffer == NULL) {
        return;
    }

    if(context->state == IR_ADC_STATE_ERROR) {
        return;
    }

    // update internal buffer with the new ADC values.
    for(int i = 0; i < NUM_PHOTODIODES; i++) {
        context->adc_buffer[i] = buffer[i];
    }

    // pack the ADC readings into a message struct. 
    adc_msg_t msg_sensor_1;
    adc_msg_t msg_sensor_2;

    pack_adc_msg(context, &msg_sensor_1, PHOTODIODE_1);
    pack_adc_msg(context, &msg_sensor_2, PHOTODIODE_2);

    
    // store the message to the fifo. 
    fifo_enqueue(fifo, &msg_sensor_1);
    fifo_enqueue(fifo, &msg_sensor_2);
}

static void pack_adc_msg(IrAdcContext_t* context, adc_msg_t* msg, photodiode_t sensor) {
    RTC_TimeTypeDef time = {0};
    RTC_DateTypeDef date = {0};

    // get the current timestamp from the RTC peripheral. 
    HAL_RTC_GetTime(context->config.hrtc, &time, RTC_FORMAT_BIN);
    HAL_RTC_GetDate(context->config.hrtc, &date, RTC_FORMAT_BIN);
    uint32_t timestamp = ((uint32_t)time.Hours * 3600U) + ((uint32_t)time.Minutes * 60U) + time.Seconds;

    // get the ADC value from the buffer. 
    uint16_t adc_value = context->adc_buffer[sensor];

    // fill in the message struct. 
    msg->timestamp = timestamp;
    msg->adc_value = adc_value;
    msg->photodiode = sensor;
}

