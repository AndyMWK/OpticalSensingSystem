#include "ir_processing.h"

/// @brief convers the given ADC value into a measurable distance value
static inline float convert_to_distance(uint16_t adc);

/// @brief
static sensor_status_t pack_adc_msg_to_data_msg(adc_msg_t adc_msg[], data_msg_t* data_msg);

//-----PV Instantiations-----

static ema_lpf_cache_t ema_mem = {
    .pd1_adc_prev = 0,
    .pd2_adc_prev = 0
};

//-----API Functions for use-----

void stream_from_fifo(fifo_t* fifo_pd1, fifo_t* fifo_pd2, data_msg_t* data_msg) {
    if(fifo_pd1 == NULL || fifo_pd2 == NULL || data_msg == NULL) {
        return;
    }

    adc_msg_t adc_msg[NUM_PHOTODIODES];

    // Dequeue one sample from each photodiode-specific FIFO.
    if(!fifo_dequeue(fifo_pd1, &adc_msg[PHOTODIODE_1]) ||
       !fifo_dequeue(fifo_pd2, &adc_msg[PHOTODIODE_2])) {
        data_msg->status = SENSOR_FIFO_EMPTY;
        return;
    }

    sensor_status_t status = pack_adc_msg_to_data_msg(adc_msg, data_msg);
    data_msg->status = status;
}

void stream_from_fifo_ema_lpf(fifo_t* fifo_pd1, fifo_t* fifo_pd2, data_msg_t* data_msg) {

    if(fifo_pd1 == NULL || fifo_pd2 == NULL || data_msg == NULL) {
        return;
    }

    adc_msg_t adc_msg[NUM_PHOTODIODES];

    // Dequeue one sample from each photodiode-specific FIFO.
    if(!fifo_dequeue(fifo_pd1, &adc_msg[PHOTODIODE_1]) ||
       !fifo_dequeue(fifo_pd2, &adc_msg[PHOTODIODE_2])) {
        data_msg->status = SENSOR_FIFO_EMPTY;
        return;
    }

    adc_msg[PHOTODIODE_1].adc_value = (float)SMOOTHING_FACTOR_EMA * (float)ema_mem.pd1_adc_prev + (1.0f - (float)SMOOTHING_FACTOR_EMA) * (float)adc_msg[PHOTODIODE_1].adc_value;
    adc_msg[PHOTODIODE_2].adc_value = (float)SMOOTHING_FACTOR_EMA * (float)ema_mem.pd2_adc_prev + (1.0f - (float)SMOOTHING_FACTOR_EMA) * (float)adc_msg[PHOTODIODE_2].adc_value;

    ema_mem.pd1_adc_prev = adc_msg[PHOTODIODE_1].adc_value;
    ema_mem.pd2_adc_prev = adc_msg[PHOTODIODE_2].adc_value;

    sensor_status_t status = pack_adc_msg_to_data_msg(adc_msg, data_msg);
    data_msg->status = status;
}

static inline float convert_to_distance(uint16_t adc) {

    return (float)GAIN * sqrtf((float)COEFF_A / (((float)adc / 4095.0f) * (float)VREF)) + (float)OFFSET;
}

// static uint8_t verify_rate_limit(fifo_t* fifo, uint8_t size) {

//     if(fifo == NULL || fifo->count < size) {
//         return 0;
//     }

//     // logic to go through the entire fifo to check the rate limit. 
//     adc_msg_t prev = fifo->buffer[fifo->tail];

//     uint8_t index = fifo->tail;
//     uint8_t inspected_count = 0;

//     while(inspected_count < size) {

//         adc_msg_t curr = fifo->buffer[index];
//         uint16_t rate_change = 0;

//         if(curr.adc_value >= prev.adc_value) {
//             rate_change = curr.adc_value - prev.adc_value;
//         } else {
//             rate_change = prev.adc_value - curr.adc_value;
//         }

//         if(rate_change >= RATE_LIMIT) {
//             return 0;
//         }

//         inspected_count++;
//         index = (index + 1) % FIFO_SIZE;
//     }

//     return 1;
// }

// /// @brief calculates the average value in the ADC
// static float calculate_average_adc(fifo_t* fifo, uint8_t size) {

//     if(size == 0) {
//         return 0.0;
//     }

//     float sum = 0.0;
//     adc_msg_t adc_msg;
//     for(int i = 0; i < size; i++) {
//         if(fifo_dequeue(fifo, &adc_msg) == 1) {
//             sum += (float)adc_msg.adc_value;
//         }
//     }

//     return sum / (float)size;
// }

static sensor_status_t pack_adc_msg_to_data_msg(adc_msg_t adc_msg[], data_msg_t* data_msg)
{
    if (adc_msg == NULL || data_msg == NULL) {
        return INTERNAL_BUFFERS_FAILED;
    }

    // Checks to ensure that the correct photodiode is being read. 
    if( adc_msg[PHOTODIODE_1].photodiode == PHOTODIODE_1 ) {
        data_msg->distance_pd_1 = convert_to_distance(adc_msg->adc_value);
        data_msg->timestamp_pd_1 = adc_msg->timestamp;
    }

    if( adc_msg[PHOTODIODE_2].photodiode == PHOTODIODE_2) {
        data_msg->distance_pd_2 = convert_to_distance( (adc_msg + 1)->adc_value );
        data_msg->timestamp_pd_2 = (adc_msg + 1)->timestamp;
    }

    // check if any of the photodiodes are saturated.
    if( adc_msg->adc_value >= SATURATION_LIMIT || (adc_msg + 1)->adc_value >= SATURATION_LIMIT ) {
        return SENSOR_SATURATED;
    }

    // check if any of the photodiodes are currently out of range.
    if( adc_msg->adc_value <= OUT_OF_RANGE_LIMIT || (adc_msg + 1)->adc_value <= OUT_OF_RANGE_LIMIT ) {
        return SENSOR_OUT_OF_RANGE;
    }
    
    return SENSOR_OK;
}
