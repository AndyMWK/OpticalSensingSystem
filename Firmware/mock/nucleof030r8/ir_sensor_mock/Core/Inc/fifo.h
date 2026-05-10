#ifndef FIFO_H
#define FIFO_H

#include "main.h"
#include "message_types.h"

#define FIFO_SIZE 32

typedef struct fifo_t {
    adc_msg_t buffer[FIFO_SIZE];
    uint8_t head;
    uint8_t tail;
    uint8_t count;
} fifo_t;

// Public API functions
void fifo_init(fifo_t* fifo);
uint8_t fifo_enqueue(fifo_t* fifo, adc_msg_t* msg);
uint8_t fifo_dequeue(fifo_t* fifo, adc_msg_t* msg);
void fifo_reset(fifo_t* fifo);


#endif