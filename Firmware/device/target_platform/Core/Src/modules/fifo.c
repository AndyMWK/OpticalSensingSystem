#include "fifo.h"

/// @brief initializes the fifo circular buffer
/// @param fifo fifo type handler
void fifo_init(fifo_t* fifo)
{
    if (fifo == NULL)
    {
        return;
    }

    fifo->head = 0;
    fifo->tail = 0;
    fifo->count = 0;
}

/// @brief Enqueues an adc message data type onto the circular buffer fifo.
/// @param fifo fifo handler data type
/// @param msg the actual adc message to be stored
/// @return 1 on successful enqueue and 0 on unsuccessful enqueue
uint8_t fifo_enqueue(fifo_t* fifo, adc_msg_t* msg)
{
    if (fifo == NULL || msg == NULL)
    {
        return 0;
    }

    fifo->buffer[fifo->head] = *msg;
    fifo->head = (uint8_t) ((fifo->head + 1U) % FIFO_SIZE);

    if (fifo->count == FIFO_SIZE)
    {
        fifo->tail = (uint8_t) ((fifo->tail + 1U) % FIFO_SIZE);
    }
    else
    {
        fifo->count++;
    }

    return 1;
}

/// @brief dequeues the oldest stored adc message.
/// @param fifo fifo handler data type
/// @param msg the actual adc message to be released
/// @return returns 1 on successful dequeue and 0 on unsuccessful dequeue
uint8_t fifo_dequeue(fifo_t* fifo, adc_msg_t* msg)
{
    if (fifo == NULL || msg == NULL || fifo->count == 0U)
    {
        return 0;
    }

    *msg = fifo->buffer[fifo->tail];
    fifo->tail = (uint8_t) ((fifo->tail + 1U) % FIFO_SIZE);
    fifo->count--;
    return 1;
}

/// @brief resets the fifo attributes to the beginning of the buffer
/// @param fifo fifo handler data type
void fifo_reset(fifo_t* fifo)
{
    if (fifo == NULL)
    {
        return;
    }

    fifo->head = 0;
    fifo->tail = 0;
    fifo->count = 0;
}
