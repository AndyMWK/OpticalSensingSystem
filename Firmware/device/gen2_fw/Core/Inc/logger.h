#ifndef LOGGER_H
#define LOGGER_H

#include "main.h"
#include <string.h>

#define LOG_BUFFER_SIZE 128
#define NULL_CHAR '\0'


// Public API functions
void init_log_buffer();

/// @brief records the log messages to the stack.
void push_log(char* msg, uint16_t msg_size);

/// @brief pushes the logs out to UART indivdually.
void pop_log(UART_HandleTypeDef* huart);


#endif
