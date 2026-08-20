#include "logger.h"

static char log_buffer[LOG_BUFFER_SIZE];
static uint16_t log_buffer_push_ptr;
static uint16_t log_buffer_pop_ptr;
static uint16_t message_count;

void init_log_buffer()
{
    log_buffer_push_ptr = 0;
    log_buffer_pop_ptr = 0;
    message_count = 0;
}

void push_log(char* msg, uint16_t msg_size)
{
    if (msg == NULL)
    {
        return;
    }

    // copy the message into log buffer RAM.
    memcpy(&log_buffer[log_buffer_push_ptr], msg, msg_size);

    // log_buffer_push_ptr = log_buffer_push_ptr + msg_size;
    // message_count++;
}

void pop_log(UART_HandleTypeDef* huart)
{
    // if(message_count == 0) {
    //     return; // buffer is empty, just exit.
    // }

    // uint16_t pop_index = log_buffer_pop_ptr;
    // uint16_t pop_size = 0;
    // char c = log_buffer[log_buffer_pop_ptr];

    // while(c != NULL_CHAR) {
    //     pop_size++;

    //     pop_index++;

    //     if(pop_index == LOG_BUFFER_SIZE - 1) {
    //         message_count--;
    //         return;
    //     }

    //     c = log_buffer[pop_index];
    // }

    // Unless using DMA this is a blocking function. Using this blocking version of the serial
    // transmission is alright but DMA is preferred to reduce CPU usage.
    HAL_UART_Transmit(huart, (uint8_t*) &log_buffer[log_buffer_pop_ptr], strlen(log_buffer),
                      HAL_MAX_DELAY);

    // log_buffer_pop_ptr = log_buffer_pop_ptr + pop_size + 1;

    // message_count--;

    // // reset the buffer parameters.
    // if(message_count == 0) {
    //     log_buffer_pop_ptr = 0;
    //     log_buffer_push_ptr = 0;
    // }

    memset(log_buffer, 0, strlen(log_buffer));
}