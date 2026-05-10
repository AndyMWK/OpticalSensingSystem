#include "fifo.h"

void fifo_init(fifo_t* fifo) {
    if (fifo == NULL) {
        return;
    }

    fifo->head = 0;
    fifo->tail = 0;
    fifo->count = 0;
}

uint8_t fifo_enqueue(fifo_t* fifo, adc_msg_t* msg) {
    if (fifo == NULL || msg == NULL) {
        return 0;
    }

    fifo->buffer[fifo->head] = *msg;
    fifo->head = (uint8_t)((fifo->head + 1U) % FIFO_SIZE);

    if (fifo->count == FIFO_SIZE) {
        fifo->tail = (uint8_t)((fifo->tail + 1U) % FIFO_SIZE);
    } else {
        fifo->count++;
    }

    return 1;
}

uint8_t fifo_dequeue(fifo_t* fifo, adc_msg_t* msg) {
    if (fifo == NULL || msg == NULL || fifo->count == 0U) {
        return 0;
    }

    *msg = fifo->buffer[fifo->tail];
    fifo->tail = (uint8_t)((fifo->tail + 1U) % FIFO_SIZE);
    fifo->count--;
    return 1;
}

void fifo_reset(fifo_t* fifo) {
    if (fifo == NULL) {
        return;
    }

    fifo->head = 0;
    fifo->tail = 0;
    fifo->count = 0;
}
