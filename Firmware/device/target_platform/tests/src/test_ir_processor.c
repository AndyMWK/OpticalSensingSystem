/**
 * @file test_ir_processor.c
 * @brief Unit tests for Core/Src/modules/ir_processing.c
 *
 * WHAT MAKES THIS MODULE HARDER TO TEST THAN THE FIFO
 * ---------------------------------------------------
 * ir_processing.c owns a file-scope `static ema_lpf_cache_t ema_mem`, and
 * there is no public function to reset it. That state survives from one test
 * to the next inside this executable, so a test that assumed "the filter
 * starts at zero" would pass or fail depending on which tests ran before it.
 *
 * Two ways to deal with that, both used below:
 *   1. Test the unfiltered path (stream_from_fifo) for the exact numbers -
 *      it is stateless, so the assertions can be precise.
 *   2. For the EMA path, drive the filter to a known settled value first
 *      (settle_ema_at), then assert on a *property* of the response rather
 *      than an exact figure.
 *
 * If you ever want exact EMA assertions, the fix is on the firmware side:
 * add something like `void ir_processing_reset(void);` that zeroes ema_mem,
 * and call it from setUp(). Needing that hook is a normal outcome of writing
 * the first tests for a module.
 */

#include "unity.h"
#include "ir_processing.h"

/* Number of samples needed for the 0.5-weighted EMA to converge from any
 * starting point. Each step halves the error and the ADC range is 12 bits,
 * so 20 iterations is comfortably more than enough. */
#define EMA_SETTLE_ITERATIONS 20

static fifo_t fifo_pd1;
static fifo_t fifo_pd2;
static data_msg_t data_msg;

void setUp(void)
{
    fifo_init(&fifo_pd1);
    fifo_init(&fifo_pd2);

    /* A caller-supplied message always starts in this state; update_fsm()
     * in fsm.c initialises it the same way. */
    data_msg.distance_pd_1 = 0.0f;
    data_msg.distance_pd_2 = 0.0f;
    data_msg.timestamp_pd_1 = 0;
    data_msg.timestamp_pd_2 = 0;
    data_msg.status = INTERNAL_MSG_NOT_SET;
}

void tearDown(void)
{
}

/* Queues one sample into each photodiode FIFO. */
static void push_pair(uint16_t adc_pd1, uint16_t adc_pd2, uint32_t timestamp)
{
    adc_msg_t s1 = {.adc_value = adc_pd1, .timestamp = timestamp, .photodiode = PHOTODIODE_1};
    adc_msg_t s2 = {.adc_value = adc_pd2, .timestamp = timestamp, .photodiode = PHOTODIODE_2};

    fifo_enqueue(&fifo_pd1, &s1);
    fifo_enqueue(&fifo_pd2, &s2);
}

/* Runs the EMA filter until its internal cache holds `adc_value`, so the test
 * that follows starts from a known filter state instead of whatever the
 * previous test left behind. */
static void settle_ema_at(uint16_t adc_value)
{
    data_msg_t scratch;

    for (int i = 0; i < EMA_SETTLE_ITERATIONS; i++)
    {
        push_pair(adc_value, adc_value, 0);
        stream_from_fifo_ema_lpf(&fifo_pd1, &fifo_pd2, &scratch);
    }
}

/* --------------------------------------------------------------------- */
/* stream_from_fifo - the unfiltered path                                 */
/* --------------------------------------------------------------------- */

void test_empty_fifos_report_sensor_fifo_empty(void)
{
    stream_from_fifo(&fifo_pd1, &fifo_pd2, &data_msg);

    TEST_ASSERT_EQUAL_INT(SENSOR_FIFO_EMPTY, data_msg.status);
}

void test_mid_range_sample_converts_to_the_expected_distance(void)
{
    /* Reference value computed from the documented model in ir_processing.h:
     *   distance = GAIN * sqrt(COEFF_A / ((adc/4095) * VREF)) + OFFSET
     *            = 1.0  * sqrt(0.7 / ((2048/4095) * 3.3)) + 3.0
     *            = 3.65126
     * Hard-coding the expected number (rather than recomputing the formula
     * here) is what makes this a real test: if someone changes GAIN, OFFSET
     * or the equation itself, this fails and forces a deliberate decision. */
    push_pair(2048, 2048, 1000);

    stream_from_fifo(&fifo_pd1, &fifo_pd2, &data_msg);

    TEST_ASSERT_EQUAL_INT(SENSOR_OK, data_msg.status);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 3.65126f, data_msg.distance_pd_1);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 3.65126f, data_msg.distance_pd_2);
    TEST_ASSERT_EQUAL_UINT32(1000, data_msg.timestamp_pd_1);
    TEST_ASSERT_EQUAL_UINT32(1000, data_msg.timestamp_pd_2);
}

void test_distance_decreases_as_the_adc_reading_rises(void)
{
    /* The physical model is a 1/sqrt relationship: more light means the
     * target is closer. Checking the monotonic direction catches a sign or
     * reciprocal error that a single-point test could miss. */
    float near_distance;
    float far_distance;

    push_pair(3000, 3000, 0);
    stream_from_fifo(&fifo_pd1, &fifo_pd2, &data_msg);
    near_distance = data_msg.distance_pd_1;

    push_pair(500, 500, 0);
    stream_from_fifo(&fifo_pd1, &fifo_pd2, &data_msg);
    far_distance = data_msg.distance_pd_1;

    TEST_ASSERT_TRUE(near_distance < far_distance);
}

void test_reading_at_or_above_the_saturation_limit_flags_saturation(void)
{
    push_pair(SATURATION_LIMIT, 2048, 0);

    stream_from_fifo(&fifo_pd1, &fifo_pd2, &data_msg);

    TEST_ASSERT_EQUAL_INT(SENSOR_SATURATED, data_msg.status);
}

void test_saturation_on_either_photodiode_is_enough(void)
{
    push_pair(2048, SATURATION_LIMIT + 50, 0);

    stream_from_fifo(&fifo_pd1, &fifo_pd2, &data_msg);

    TEST_ASSERT_EQUAL_INT(SENSOR_SATURATED, data_msg.status);
}

void test_reading_at_or_below_the_out_of_range_limit_flags_out_of_range(void)
{
    push_pair(OUT_OF_RANGE_LIMIT, 2048, 0);

    stream_from_fifo(&fifo_pd1, &fifo_pd2, &data_msg);

    TEST_ASSERT_EQUAL_INT(SENSOR_OUT_OF_RANGE, data_msg.status);
}

void test_readings_just_inside_both_limits_are_healthy(void)
{
    /* Boundary test: one LSB inside each threshold must still read OK.
     * Off-by-one errors in the comparisons live exactly here. */
    push_pair(OUT_OF_RANGE_LIMIT + 1, SATURATION_LIMIT - 1, 0);

    stream_from_fifo(&fifo_pd1, &fifo_pd2, &data_msg);

    TEST_ASSERT_EQUAL_INT(SENSOR_OK, data_msg.status);
}

void test_saturation_is_reported_in_preference_to_out_of_range(void)
{
    /* One photodiode blinded, the other seeing nothing.
     * pack_adc_msg_to_data_msg() checks saturation first, so that is what the
     * caller is told. Documenting the priority order matters because the FSM
     * reacts to the two very differently (dim vs. brighten). */
    push_pair(SATURATION_LIMIT, OUT_OF_RANGE_LIMIT, 0);

    stream_from_fifo(&fifo_pd1, &fifo_pd2, &data_msg);

    TEST_ASSERT_EQUAL_INT(SENSOR_SATURATED, data_msg.status);
}

void test_one_empty_fifo_reports_empty_even_if_the_other_has_data(void)
{
    adc_msg_t s1 = {.adc_value = 2048, .timestamp = 0, .photodiode = PHOTODIODE_1};
    fifo_enqueue(&fifo_pd1, &s1);
    /* fifo_pd2 deliberately left empty. */

    stream_from_fifo(&fifo_pd1, &fifo_pd2, &data_msg);

    TEST_ASSERT_EQUAL_INT(SENSOR_FIFO_EMPTY, data_msg.status);

    /* Worth knowing: the short-circuit in stream_from_fifo() dequeued from
     * pd1 before discovering pd2 was empty, so that sample is now gone. If
     * the two FIFOs ever drift out of step, this is where samples are lost.
     * The assertion records today's behaviour rather than endorsing it. */
    TEST_ASSERT_EQUAL_UINT8(0, fifo_pd1.count);
}

void test_null_arguments_are_rejected_without_crashing(void)
{
    push_pair(2048, 2048, 0);

    stream_from_fifo(NULL, &fifo_pd2, &data_msg);
    stream_from_fifo(&fifo_pd1, NULL, &data_msg);
    stream_from_fifo(&fifo_pd1, &fifo_pd2, NULL);

    /* Nothing was consumed and nothing was written. */
    TEST_ASSERT_EQUAL_INT(INTERNAL_MSG_NOT_SET, data_msg.status);
    TEST_ASSERT_EQUAL_UINT8(1, fifo_pd1.count);
}

/* --------------------------------------------------------------------- */
/* stream_from_fifo_ema_lpf - the filtered path                           */
/* --------------------------------------------------------------------- */

void test_ema_passes_a_steady_signal_through_unchanged(void)
{
    /* Once the filter has settled on a constant input its output must equal
     * that input - a low-pass filter has unity DC gain. */
    settle_ema_at(2048);

    push_pair(2048, 2048, 0);
    stream_from_fifo_ema_lpf(&fifo_pd1, &fifo_pd2, &data_msg);

    TEST_ASSERT_EQUAL_INT(SENSOR_OK, data_msg.status);
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 3.65126f, data_msg.distance_pd_1);
}

void test_ema_smooths_a_step_instead_of_following_it_immediately(void)
{
    /* The whole point of the filter: a sudden jump in the raw ADC value must
     * not appear at full size in the output. With SMOOTHING_FACTOR_EMA = 0.5
     * the first sample after a step lands roughly half way. */
    float settled_distance;
    float stepped_distance;
    float raw_step_distance;

    settle_ema_at(1000);

    push_pair(1000, 1000, 0);
    stream_from_fifo_ema_lpf(&fifo_pd1, &fifo_pd2, &data_msg);
    settled_distance = data_msg.distance_pd_1;

    /* What the step WOULD read with no filtering at all.
     * stream_from_fifo() does not touch the EMA cache, so this measurement
     * leaves the filter state alone. */
    push_pair(3000, 3000, 0);
    stream_from_fifo(&fifo_pd1, &fifo_pd2, &data_msg);
    raw_step_distance = data_msg.distance_pd_1;

    /* And what it actually reads through the filter, from the same state. */
    settle_ema_at(1000);
    push_pair(3000, 3000, 0);
    stream_from_fifo_ema_lpf(&fifo_pd1, &fifo_pd2, &data_msg);
    stepped_distance = data_msg.distance_pd_1;

    /* Higher ADC means shorter distance, so the filtered reading has to sit
     * strictly between "where we were" and "where the raw sample says". */
    TEST_ASSERT_TRUE(stepped_distance < settled_distance);
    TEST_ASSERT_TRUE(stepped_distance > raw_step_distance);
}

void test_ema_eventually_converges_on_a_new_level(void)
{
    settle_ema_at(500);
    settle_ema_at(3000);

    push_pair(3000, 3000, 0);
    stream_from_fifo_ema_lpf(&fifo_pd1, &fifo_pd2, &data_msg);

    /* Reference: sqrt(0.7 / ((3000/4095) * 3.3)) + 3.0 = 3.53809.
     * Tolerance is looser than in the unfiltered test because the filter
     * stores its state in a uint16_t, so the settled value can sit one LSB
     * below the input. */
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 3.53809f, data_msg.distance_pd_1);
}

void test_ema_path_reports_empty_fifos_too(void)
{
    stream_from_fifo_ema_lpf(&fifo_pd1, &fifo_pd2, &data_msg);

    TEST_ASSERT_EQUAL_INT(SENSOR_FIFO_EMPTY, data_msg.status);
}

/* --------------------------------------------------------------------- */

int main(void)
{
    UNITY_BEGIN();

    RUN_TEST(test_empty_fifos_report_sensor_fifo_empty);
    RUN_TEST(test_mid_range_sample_converts_to_the_expected_distance);
    RUN_TEST(test_distance_decreases_as_the_adc_reading_rises);
    RUN_TEST(test_reading_at_or_above_the_saturation_limit_flags_saturation);
    RUN_TEST(test_saturation_on_either_photodiode_is_enough);
    RUN_TEST(test_reading_at_or_below_the_out_of_range_limit_flags_out_of_range);
    RUN_TEST(test_readings_just_inside_both_limits_are_healthy);
    RUN_TEST(test_saturation_is_reported_in_preference_to_out_of_range);
    RUN_TEST(test_one_empty_fifo_reports_empty_even_if_the_other_has_data);
    RUN_TEST(test_null_arguments_are_rejected_without_crashing);

    RUN_TEST(test_ema_passes_a_steady_signal_through_unchanged);
    RUN_TEST(test_ema_smooths_a_step_instead_of_following_it_immediately);
    RUN_TEST(test_ema_eventually_converges_on_a_new_level);
    RUN_TEST(test_ema_path_reports_empty_fifos_too);

    return UNITY_END();
}
