/**
 * @file test_fifo.c
 * @brief Unit tests for Core/Src/modules/fifo.c
 *
 * HOW A UNITY TEST FILE IS STRUCTURED
 * -----------------------------------
 *  - setUp()    runs before every single test function.
 *  - tearDown() runs after every single test function.
 *    Both names are fixed - Unity calls them for you, and the linker fails if
 *    you forget to define them.
 *  - Each test is a `void test_something(void)` function containing
 *    TEST_ASSERT_* macros. The first failing assertion aborts that test and
 *    moves on to the next one.
 *  - main() lists the tests between UNITY_BEGIN() and UNITY_END().
 *    UNITY_END() returns the number of failures, which becomes the process
 *    exit code, which is how CTest knows whether the test passed.
 *
 * This file is a good starting point because fifo.c is pure logic: no
 * hardware, no static state hidden inside the module, no dependencies.
 */

#include "unity.h"
#include "fifo.h"

/* One FIFO shared by all tests in this file, re-initialised in setUp(). */
static fifo_t fifo;

void setUp(void)
{
    fifo_init(&fifo);
}

void tearDown(void)
{
}

/* Small helper so the tests read as data, not as struct plumbing. */
static adc_msg_t make_sample(uint16_t adc_value, uint32_t timestamp, photodiode_t pd)
{
    adc_msg_t msg;
    msg.adc_value = adc_value;
    msg.timestamp = timestamp;
    msg.photodiode = pd;
    return msg;
}

/* --------------------------------------------------------------------- */
/* Basic behaviour                                                        */
/* --------------------------------------------------------------------- */

void test_init_leaves_the_fifo_empty(void)
{
    /* setUp() already called fifo_init(). */
    TEST_ASSERT_EQUAL_UINT8(0, fifo.head);
    TEST_ASSERT_EQUAL_UINT8(0, fifo.tail);
    TEST_ASSERT_EQUAL_UINT8(0, fifo.count);
}

void test_enqueue_then_dequeue_returns_the_same_sample(void)
{
    adc_msg_t in = make_sample(1234, 42, PHOTODIODE_1);
    adc_msg_t out = make_sample(0, 0, PHOTODIODE_2);

    TEST_ASSERT_EQUAL_UINT8(1, fifo_enqueue(&fifo, &in));
    TEST_ASSERT_EQUAL_UINT8(1, fifo.count);

    TEST_ASSERT_EQUAL_UINT8(1, fifo_dequeue(&fifo, &out));
    TEST_ASSERT_EQUAL_UINT8(0, fifo.count);

    TEST_ASSERT_EQUAL_UINT16(1234, out.adc_value);
    TEST_ASSERT_EQUAL_UINT32(42, out.timestamp);
    TEST_ASSERT_EQUAL_INT(PHOTODIODE_1, out.photodiode);
}

void test_dequeue_from_an_empty_fifo_fails_and_leaves_the_output_untouched(void)
{
    adc_msg_t out = make_sample(0xBEEF, 7, PHOTODIODE_2);

    TEST_ASSERT_EQUAL_UINT8(0, fifo_dequeue(&fifo, &out));

    /* The caller's buffer must not be clobbered on failure. */
    TEST_ASSERT_EQUAL_UINT16(0xBEEF, out.adc_value);
}

void test_samples_come_out_in_the_order_they_went_in(void)
{
    for (uint16_t i = 0; i < 5; i++) {
        adc_msg_t in = make_sample(i, i, PHOTODIODE_1);
        TEST_ASSERT_EQUAL_UINT8(1, fifo_enqueue(&fifo, &in));
    }

    for (uint16_t i = 0; i < 5; i++) {
        adc_msg_t out;
        TEST_ASSERT_EQUAL_UINT8(1, fifo_dequeue(&fifo, &out));
        TEST_ASSERT_EQUAL_UINT16(i, out.adc_value);
    }
}

/* --------------------------------------------------------------------- */
/* Edge cases - the reason unit tests are worth writing at all            */
/* --------------------------------------------------------------------- */

void test_filling_to_capacity_keeps_every_sample(void)
{
    for (uint16_t i = 0; i < FIFO_SIZE; i++) {
        adc_msg_t in = make_sample(i, i, PHOTODIODE_1);
        TEST_ASSERT_EQUAL_UINT8(1, fifo_enqueue(&fifo, &in));
    }

    TEST_ASSERT_EQUAL_UINT8(FIFO_SIZE, fifo.count);

    adc_msg_t out;
    TEST_ASSERT_EQUAL_UINT8(1, fifo_dequeue(&fifo, &out));
    TEST_ASSERT_EQUAL_UINT16(0, out.adc_value); /* oldest sample still there */
}

void test_overflowing_drops_the_oldest_sample_not_the_newest(void)
{
    /* This pins down a real design decision in fifo_enqueue(): when the
     * buffer is full it still accepts the new sample and advances `tail`,
     * overwriting the oldest entry. For an ADC stream that is usually what
     * you want - fresh data matters more than stale data - but it is exactly
     * the kind of choice that should be locked down by a test so a future
     * refactor cannot silently flip it. */

    for (uint16_t i = 0; i < FIFO_SIZE + 1; i++) {
        adc_msg_t in = make_sample(i, i, PHOTODIODE_1);
        TEST_ASSERT_EQUAL_UINT8(1, fifo_enqueue(&fifo, &in));
    }

    /* Capacity is never exceeded... */
    TEST_ASSERT_EQUAL_UINT8(FIFO_SIZE, fifo.count);

    /* ...and sample 0 is the one that was sacrificed. */
    adc_msg_t out;
    TEST_ASSERT_EQUAL_UINT8(1, fifo_dequeue(&fifo, &out));
    TEST_ASSERT_EQUAL_UINT16(1, out.adc_value);

    /* The newest sample survived and is last in line. */
    for (uint16_t i = 2; i < FIFO_SIZE; i++) {
        TEST_ASSERT_EQUAL_UINT8(1, fifo_dequeue(&fifo, &out));
    }
    TEST_ASSERT_EQUAL_UINT8(1, fifo_dequeue(&fifo, &out));
    TEST_ASSERT_EQUAL_UINT16(FIFO_SIZE, out.adc_value);
    TEST_ASSERT_EQUAL_UINT8(0, fifo.count);
}

void test_indices_wrap_around_instead_of_running_off_the_buffer(void)
{
    /* Push far more samples than the buffer holds, one at a time, draining
     * as we go. If the modulo arithmetic in fifo.c were wrong this would
     * either read garbage or walk off the end of the array. */
    for (uint16_t i = 0; i < (FIFO_SIZE * 3); i++) {
        adc_msg_t in = make_sample(i, i, PHOTODIODE_2);
        adc_msg_t out;

        TEST_ASSERT_EQUAL_UINT8(1, fifo_enqueue(&fifo, &in));
        TEST_ASSERT_EQUAL_UINT8(1, fifo_dequeue(&fifo, &out));
        TEST_ASSERT_EQUAL_UINT16(i, out.adc_value);
    }

    TEST_ASSERT_EQUAL_UINT8(0, fifo.count);
    TEST_ASSERT_TRUE(fifo.head < FIFO_SIZE);
    TEST_ASSERT_TRUE(fifo.tail < FIFO_SIZE);
}

void test_reset_discards_buffered_samples(void)
{
    adc_msg_t in = make_sample(500, 1, PHOTODIODE_1);
    fifo_enqueue(&fifo, &in);
    fifo_enqueue(&fifo, &in);

    fifo_reset(&fifo);

    TEST_ASSERT_EQUAL_UINT8(0, fifo.count);
    TEST_ASSERT_EQUAL_UINT8(0, fifo.head);
    TEST_ASSERT_EQUAL_UINT8(0, fifo.tail);

    adc_msg_t out;
    TEST_ASSERT_EQUAL_UINT8(0, fifo_dequeue(&fifo, &out));
}

/* --------------------------------------------------------------------- */
/* Defensive guards                                                       */
/* --------------------------------------------------------------------- */

void test_null_pointers_are_rejected_without_crashing(void)
{
    adc_msg_t msg = make_sample(1, 1, PHOTODIODE_1);

    TEST_ASSERT_EQUAL_UINT8(0, fifo_enqueue(NULL, &msg));
    TEST_ASSERT_EQUAL_UINT8(0, fifo_enqueue(&fifo, NULL));
    TEST_ASSERT_EQUAL_UINT8(0, fifo_dequeue(NULL, &msg));
    TEST_ASSERT_EQUAL_UINT8(0, fifo_dequeue(&fifo, NULL));

    /* These must not fault - if they did, the test binary would crash and
     * CTest would report the whole file as failed. */
    fifo_init(NULL);
    fifo_reset(NULL);
}

/* --------------------------------------------------------------------- */

int main(void)
{
    UNITY_BEGIN();

    RUN_TEST(test_init_leaves_the_fifo_empty);
    RUN_TEST(test_enqueue_then_dequeue_returns_the_same_sample);
    RUN_TEST(test_dequeue_from_an_empty_fifo_fails_and_leaves_the_output_untouched);
    RUN_TEST(test_samples_come_out_in_the_order_they_went_in);
    RUN_TEST(test_filling_to_capacity_keeps_every_sample);
    RUN_TEST(test_overflowing_drops_the_oldest_sample_not_the_newest);
    RUN_TEST(test_indices_wrap_around_instead_of_running_off_the_buffer);
    RUN_TEST(test_reset_discards_buffered_samples);
    RUN_TEST(test_null_pointers_are_rejected_without_crashing);

    return UNITY_END();
}
