# IR Sensor Firmware Design Overview

## Introduction

This document provides a brief design overview of the IR sensor firmware implemented in the mock directory (`mock/nucleof030r8/ir_sensor_mock/`). The mock firmware serves as a prototype for the actual target device firmware, utilizing the same core inner workings and architecture. The firmware is designed for STM32F030 microcontroller and simulates IR sensor functionality using two photodiodes.

## Architecture

The firmware follows a modular architecture built on STM32CubeMX-generated code with custom modules for sensor processing. Key components include:

- **Microcontroller**: Mock Device: STM32F030R8 Target Device: STM32G030
- **Peripherals**: ADC (with DMA), UART1/UART2, TIM1/TIM3, RTC
- **Communication**: RS485 for sensor data transmission, UART for logging/debugging
- **Power Management**: Basic HAL-based initialization

## Core Modules

### 1. IR ADC Module (`ir_adc.c`)
- Initializes ADC peripheral with DMA for continuous conversion
- Handles two photodiode channels (PHOTODIODE_1, PHOTODIODE_2)
- Provides callback mechanism for conversion completion
- Packs ADC readings with timestamps into message structures

### 2. IR Processing Module (`ir_processing.c`)
- Converts raw ADC values to distance measurements
- Implements Exponential Moving Average Low Pass Filter (EMA LPF) for noise reduction
- Processes data from FIFO buffers
- Handles sensor status detection (OK, saturated, out of range)

### 3. FIFO Module (`fifo.c`)
- Provides circular buffer implementation for ADC data queuing
- Supports enqueue/dequeue operations for message passing between modules
- Used to decouple ADC acquisition from processing

### 4. Finite State Machine (FSM) Module (`fsm.c`)
- Manages sensor operational states (IDLE, ACTIVE, ERROR)
- Processes incoming data and communication messages
- Handles error conditions and recovery
- Generates output messages for transmission

### 5. Communication Modules
- **RS485 Module**: Handles Modbus-style communication protocol
- **Logger Module**: Provides debug logging via UART

## Data Flow

1. **ADC Acquisition**: Timer-triggered ADC conversions read photodiode voltages via DMA
2. **Buffering**: ADC values with timestamps are enqueued into FIFO buffers
3. **Processing**: FSM dequeues data, applies EMA LPF filtering, converts to distances
4. **State Management**: FSM evaluates sensor status and handles errors
5. **Output**: Processed data is formatted and transmitted via RS485/UART

## Key Features

- **Real-time Processing**: Interrupt-driven ADC with DMA for low-latency acquisition
- **Noise Filtering**: EMA LPF reduces sensor noise for stable distance measurements
- **Error Handling**: Comprehensive status checking and error recovery
- **Modular Design**: Clean separation of concerns for maintainability
- **Communication**: Dual UART interfaces for logging and data transmission

## Build System

- **CMake-based**: Cross-platform build configuration
- **Toolchains**: GCC ARM None EABI, ST ARM Clang
- **STM32CubeMX Integration**: Automated peripheral configuration

This design provides a robust foundation for IR distance sensing applications, with the mock implementation serving as a development and testing platform for the production firmware.