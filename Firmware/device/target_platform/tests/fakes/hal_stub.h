/**
 * @file hal_stub.h
 * @brief Replaces the STM32 HAL when firmware modules are compiled for the host PC.
 *
 * WHY THIS FILE EXISTS
 * --------------------
 * Every module header in Core/Inc starts with `#include "main.h"`, and
 * Core/Inc/main.h starts with `#include "stm32f0xx_hal.h"`. That HAL header
 * only compiles for an ARM Cortex-M target, so a normal x86 compiler chokes
 * on it long before it ever reaches fifo.c or fsm.c.
 *
 * We cannot simply put a different "main.h" earlier on the include path:
 * when fsm.h asks for "main.h" with quotes, the compiler looks in fsm.h's own
 * directory (Core/Inc) FIRST, so the real main.h always wins.
 *
 * The trick used here instead:
 *
 *   1. The host test build force-includes this file into every translation
 *      unit (the `-include` / `/FI` flag set in tests/CMakeLists.txt), so it
 *      is processed before the first line of any .c file.
 *   2. Line "#define __MAIN_H" below claims main.h's own include guard.
 *      When Core/Inc/main.h is later reached, its `#ifndef __MAIN_H` is
 *      already false, so the whole file - including the HAL include -
 *      collapses to nothing.
 *   3. This file then supplies the handful of HAL type names the module
 *      headers actually mention, so they still parse.
 *
 * The firmware sources are never edited, and the on-target build is untouched.
 *
 * ADDING TO THIS FILE
 * -------------------
 * If you pull another module into the host tests and the compiler complains
 * about an unknown HAL type or macro, add the smallest possible stand-in for
 * it here. Keep the stubs dumb: they exist to satisfy the compiler, not to
 * simulate the peripheral.
 */

#ifndef HOST_TEST_HAL_STUB_H
#define HOST_TEST_HAL_STUB_H

/* Neutralise Core/Inc/main.h by pre-claiming its include guard. */
#define __MAIN_H

#include <stdint.h>
#include <stddef.h>

/*
 * Peripheral handle types.
 *
 * The module headers only ever use these as pointers (ADC_HandleTypeDef*,
 * UART_HandleTypeDef*, ...), so an opaque one-field struct is enough for the
 * code to compile and link. Any test that genuinely needs peripheral
 * behaviour should inject a fake through a function pointer instead - which
 * is exactly what fsm.h already does with async_log_cb_t and pwm_control_t.
 */
typedef struct
{
    int unused;
} ADC_HandleTypeDef;
typedef struct
{
    int unused;
} DMA_HandleTypeDef;
typedef struct
{
    int unused;
} RTC_HandleTypeDef;
typedef struct
{
    int unused;
} UART_HandleTypeDef;
typedef struct
{
    int unused;
} TIM_HandleTypeDef;

/* Declared by the real main.h; nothing under test calls it, but keeping the
 * declaration means a module that does reference it still compiles. */
void Error_Handler(void);

/* Pin/port macros from main.h's "Private defines" section. Only add the ones
 * a module under test actually references. */
#define LED_Built_In_Pin ((uint16_t) 0x0020) /* GPIO_PIN_5 */
#define LED_Built_In_GPIO_Port ((void*) 0)

#endif /* HOST_TEST_HAL_STUB_H */
