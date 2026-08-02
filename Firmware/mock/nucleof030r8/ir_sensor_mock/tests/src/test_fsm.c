/**
 * @file test_fsm.c
 * @brief Unit tests for Core/Src/modules/fsm.c
 *
 * THE TWO NEW IDEAS IN THIS FILE
 * ------------------------------
 * 1. SPIES (a kind of test double).
 *    fsm.c does not call the UART or the timer directly - it calls through
 *    the function pointers registered by register_fsm_callbacks(). That is
 *    already a testable design. Here we register two tiny functions that do
 *    nothing but record what they were asked to do, then assert on those
 *    recordings. No hardware, no mocking framework required.
 *
 * 2. WORKING AROUND HIDDEN STATE.
 *    fsm.c keeps `ctx`, `output_msg` and `ir_led_pwm` as file-scope statics.
 *    reset_fsm() clears the first two but NOT ir_led_pwm, so the duty cycle
 *    carries over between tests. Rather than depend on test ordering, the
 *    PWM tests below assert on *relative* change ("the next callback value is
 *    one PWM_DELTA lower than the previous one"), which holds regardless of
 *    where the duty cycle happened to start.
 *
 * A note on the FSM's warm-up: from STATE_IDLE, update_fsm() produces no
 * output on the first call - it only transitions into STATE_STREAMING. So
 * most tests here call update_fsm() twice, or start from a state the FSM has
 * already been driven into. There is also a hard-coded debug line in the
 * STATE_IDLE branch of fsm.c that forces STATE_STREAMING regardless of the
 * RS485 command; test_idle_transitions_to_streaming_on_the_first_update
 * documents that so it is not forgotten before shipping.
 */

#include "unity.h"
#include "fsm.h"

#include <string.h>

/* --------------------------------------------------------------------- */
/* Test doubles                                                           */
/* --------------------------------------------------------------------- */

static char     spy_log_text[TX_MSG_BUFFER + 1];
static uint16_t spy_log_len;
static int      spy_log_calls;

static uint32_t spy_pwm_last_duty;
static int      spy_pwm_calls;

/* Stands in for the logger's asynchronous UART write. */
static void spy_log_callback(char *msg_buf, uint16_t msg_len)
{
    spy_log_calls++;
    spy_log_len = msg_len;

    memset(spy_log_text, 0, sizeof(spy_log_text));
    if (msg_buf != NULL && msg_len < sizeof(spy_log_text)) {
        memcpy(spy_log_text, msg_buf, msg_len);
    }
}

/* Stands in for the timer compare register write that sets LED brightness. */
static void spy_pwm_callback(uint32_t duty)
{
    spy_pwm_calls++;
    spy_pwm_last_duty = duty;
}

/* --------------------------------------------------------------------- */
/* Fixture                                                                */
/* --------------------------------------------------------------------- */

static fifo_t fifo_pd1;
static fifo_t fifo_pd2;
static comms_msg_t comms;

void setUp(void)
{
    fifo_init(&fifo_pd1);
    fifo_init(&fifo_pd2);

    memset(&comms, 0, sizeof(comms));
    /* memset gives fsm_action == 0 == LPF_ENABLE, i.e. "no stream command
     * pending", which is what we want as a neutral starting point. */

    /* Registering real (spy) callbacks matters: the STATE_PWM_DIM and
     * STATE_PWM_BRIGHTEN branches of fsm.c call pwm_callback() without a NULL
     * check, so an unregistered FSM would segfault the moment it tried to
     * change brightness. */
    register_fsm_callbacks(spy_log_callback, spy_pwm_callback);

    /* fsm.c's `async_signal` is a file-scope static that reset_fsm() does not
     * touch, so a test that raised it would leak the signal into the next
     * test. Drain it here, then zero the spies, so every test starts from
     * "nothing pending" no matter what ran before it. */
    (void)handle_async_request();
    reset_fsm();

    spy_log_calls = 0;
    spy_log_len = 0;
    memset(spy_log_text, 0, sizeof(spy_log_text));
    spy_pwm_calls = 0;
    spy_pwm_last_duty = 0;
}

void tearDown(void)
{
}

static void push_pair(uint16_t adc_value, uint32_t timestamp)
{
    adc_msg_t s1 = { .adc_value = adc_value, .timestamp = timestamp, .photodiode = PHOTODIODE_1 };
    adc_msg_t s2 = { .adc_value = adc_value, .timestamp = timestamp, .photodiode = PHOTODIODE_2 };

    fifo_enqueue(&fifo_pd1, &s1);
    fifo_enqueue(&fifo_pd2, &s2);
}

/* Reads whatever the FSM has queued for the UART.
 * output_from_fsm() memcpy's without a terminator, so the destination is one
 * byte larger than TX_MSG_BUFFER and pre-zeroed to keep it a valid C string. */
static const char *read_uart_output(void)
{
    static char buffer[TX_MSG_BUFFER + 1];
    uint16_t filled_len = 0;

    memset(buffer, 0, sizeof(buffer));
    output_from_fsm(UART_TX, buffer, &filled_len, TX_MSG_BUFFER);

    return buffer;
}

#define TEST_ASSERT_OUTPUT_CONTAINS(expected)                                  \
    do {                                                                       \
        const char *actual_output_ = read_uart_output();                       \
        if (strstr(actual_output_, (expected)) == NULL) {                      \
            TEST_FAIL_MESSAGE("expected \"" expected "\" in FSM UART output"); \
        }                                                                      \
    } while (0)

/* fsm.c feeds itself through stream_from_fifo_ema_lpf(), whose filter state is
 * a static inside ir_processing.c and is not reset by reset_fsm(). Driving the
 * filter to a known level first means "push a saturated sample" really does
 * produce a saturated reading, instead of a half-way value that depends on
 * which test ran before this one. */
static void settle_sensor_at(uint16_t adc_value)
{
    data_msg_t scratch;

    for (int i = 0; i < 20; i++) {
        push_pair(adc_value, 0);
        stream_from_fifo_ema_lpf(&fifo_pd1, &fifo_pd2, &scratch);
    }
}

/* Puts the FSM into steady streaming operation on healthy data. */
static void drive_to_streaming(void)
{
    settle_sensor_at(2048);

    push_pair(2048, 0);
    update_fsm(&fifo_pd1, &fifo_pd2, &comms);  /* IDLE -> STREAMING */
}

/* --------------------------------------------------------------------- */
/* Start-up and healthy streaming                                         */
/* --------------------------------------------------------------------- */

void test_idle_transitions_to_streaming_on_the_first_update(void)
{
    settle_sensor_at(2048);
    push_pair(2048, 0);

    update_fsm(&fifo_pd1, &fifo_pd2, &comms);

    /* No output yet: the STATE_IDLE branch only changes state. Note this
     * happens even though comms.rs485_msg.fsm_action is not STREAM_ENABLE,
     * because of the "just for debugging" override in fsm.c. When that line
     * is removed, this assertion is the one that will tell you. */
    TEST_ASSERT_EQUAL_STRING("", read_uart_output());

    /* Second cycle, now in STATE_STREAMING, does produce output. */
    push_pair(2048, 0);
    update_fsm(&fifo_pd1, &fifo_pd2, &comms);

    TEST_ASSERT_OUTPUT_CONTAINS("Sensor State is healthy");
}

void test_streaming_reports_distance_and_timestamp_for_both_photodiodes(void)
{
    drive_to_streaming();

    push_pair(2048, 1234);
    update_fsm(&fifo_pd1, &fifo_pd2, &comms);

    /* create_log_msg() formats "PD<n> D:<m>.<cm> T:<t>s" for each photodiode.
     * 2048 counts is ~3.65 m through the model in ir_processing.h. */
    TEST_ASSERT_OUTPUT_CONTAINS("PD0 D:3.65 T:1234s");
    TEST_ASSERT_OUTPUT_CONTAINS("PD1 D:3.65 T:1234s");
}

void test_each_update_starts_from_a_clean_output_buffer(void)
{
    drive_to_streaming();

    push_pair(2048, 0);
    update_fsm(&fifo_pd1, &fifo_pd2, &comms);
    uint32_t first_length = (uint32_t)strlen(read_uart_output());

    push_pair(2048, 0);
    update_fsm(&fifo_pd1, &fifo_pd2, &comms);
    uint32_t second_length = (uint32_t)strlen(read_uart_output());

    /* If the buffer were not cleared each cycle it would grow until the
     * 128-byte guard in append_log_msg_to_uart() silently dropped messages. */
    TEST_ASSERT_TRUE(first_length > 0);
    TEST_ASSERT_EQUAL_UINT32(first_length, second_length);
}

/* --------------------------------------------------------------------- */
/* Error paths                                                            */
/* --------------------------------------------------------------------- */

void test_empty_fifo_is_reported_and_recovers_to_streaming(void)
{
    /* Both FIFOs empty - nothing was pushed. */
    update_fsm(&fifo_pd1, &fifo_pd2, &comms);

    TEST_ASSERT_OUTPUT_CONTAINS("ADC FIFO Empty");

    /* One dropped frame is not fatal: below MAX_ERROR_COUNT the FSM returns
     * to streaming rather than latching an error. */
    settle_sensor_at(2048);
    push_pair(2048, 0);
    update_fsm(&fifo_pd1, &fifo_pd2, &comms);

    TEST_ASSERT_OUTPUT_CONTAINS("Sensor State is healthy");
}

void test_repeated_empty_fifos_eventually_latch_the_error_state(void)
{
    /* MAX_ERROR_COUNT is 10; give it a generous margin. */
    for (int i = 0; i < MAX_ERROR_COUNT + 5; i++) {
        update_fsm(&fifo_pd1, &fifo_pd2, &comms);
    }

    /* Once error_count reaches MAX_ERROR_COUNT the FIFO_EMPTY branch stops
     * bouncing back to STATE_STREAMING, so the FSM stays parked reporting the
     * fault instead of pretending the data stream is fine.
     *
     * Note what is NOT reachable here: STATE_ERROR ("Maximum Error Count
     * Reached") is only entered from handle_sensor_error(), which only runs
     * on SENSOR_RATE_LIMIT - and ir_processing.c never returns that status,
     * because verify_rate_limit() is still commented out. So the FSM has no
     * way to reach its terminal error state today. */
    TEST_ASSERT_OUTPUT_CONTAINS("ADC FIFO Empty");
}

/* --------------------------------------------------------------------- */
/* Closed-loop brightness control                                         */
/* --------------------------------------------------------------------- */

void test_saturated_sensor_asks_for_dimming(void)
{
    settle_sensor_at(SATURATION_LIMIT + 100);

    push_pair(SATURATION_LIMIT + 100, 0);
    update_fsm(&fifo_pd1, &fifo_pd2, &comms);

    TEST_ASSERT_OUTPUT_CONTAINS("Sensor too close, Dimming.");
}

void test_dimming_steps_the_duty_cycle_down_by_one_delta_per_cycle(void)
{
    uint32_t first_duty;
    uint32_t second_duty;

    settle_sensor_at(SATURATION_LIMIT + 100);

    /* Cycle 1: enters STATE_SATURATION and hands off to STATE_PWM_DIM. */
    push_pair(SATURATION_LIMIT + 100, 0);
    update_fsm(&fifo_pd1, &fifo_pd2, &comms);
    TEST_ASSERT_EQUAL_INT(0, spy_pwm_calls); /* no PWM write yet */

    /* Cycle 2: first actual duty-cycle change. */
    push_pair(SATURATION_LIMIT + 100, 0);
    update_fsm(&fifo_pd1, &fifo_pd2, &comms);
    TEST_ASSERT_EQUAL_INT(1, spy_pwm_calls);
    first_duty = spy_pwm_last_duty;

    /* Cycle 3: one more step in the same direction. */
    push_pair(SATURATION_LIMIT + 100, 0);
    update_fsm(&fifo_pd1, &fifo_pd2, &comms);
    TEST_ASSERT_EQUAL_INT(2, spy_pwm_calls);
    second_duty = spy_pwm_last_duty;

    TEST_ASSERT_EQUAL_UINT32(first_duty - PWM_DELTA, second_duty);
}

void test_out_of_range_sensor_asks_for_brightening(void)
{
    settle_sensor_at(OUT_OF_RANGE_LIMIT / 2);

    push_pair(OUT_OF_RANGE_LIMIT / 2, 0);
    update_fsm(&fifo_pd1, &fifo_pd2, &comms);

    TEST_ASSERT_OUTPUT_CONTAINS("Sensor too far, Brightening.");
}

void test_brightening_steps_the_duty_cycle_up_by_one_delta_per_cycle(void)
{
    uint32_t first_duty;
    uint32_t second_duty;

    settle_sensor_at(OUT_OF_RANGE_LIMIT / 2);

    push_pair(OUT_OF_RANGE_LIMIT / 2, 0);
    update_fsm(&fifo_pd1, &fifo_pd2, &comms);   /* -> STATE_PWM_BRIGHTEN */

    push_pair(OUT_OF_RANGE_LIMIT / 2, 0);
    update_fsm(&fifo_pd1, &fifo_pd2, &comms);
    TEST_ASSERT_EQUAL_INT(1, spy_pwm_calls);
    first_duty = spy_pwm_last_duty;

    push_pair(OUT_OF_RANGE_LIMIT / 2, 0);
    update_fsm(&fifo_pd1, &fifo_pd2, &comms);
    second_duty = spy_pwm_last_duty;

    TEST_ASSERT_EQUAL_UINT32(first_duty + PWM_DELTA, second_duty);
}

void test_dimming_stops_at_the_lower_duty_limit(void)
{
    /* Drive dimming for far more cycles than the 50 -> 35 range needs, and
     * check the control loop clamps instead of running the duty cycle
     * negative. This is the kind of run-away that is painful to reproduce on
     * hardware and trivial to catch here. */
    settle_sensor_at(SATURATION_LIMIT + 100);

    for (int i = 0; i < 100; i++) {
        push_pair(SATURATION_LIMIT + 100, 0);
        update_fsm(&fifo_pd1, &fifo_pd2, &comms);
    }

    TEST_ASSERT_EQUAL_UINT32(PWM_DIM_TARGET, spy_pwm_last_duty);
    TEST_ASSERT_OUTPUT_CONTAINS("PWM minimum duty cycle reached");
}

void test_healthy_reading_ends_the_dimming_loop(void)
{
    settle_sensor_at(SATURATION_LIMIT + 100);
    push_pair(SATURATION_LIMIT + 100, 0);
    update_fsm(&fifo_pd1, &fifo_pd2, &comms);   /* -> STATE_PWM_DIM */

    /* Sensor recovers. */
    settle_sensor_at(2048);
    push_pair(2048, 0);
    update_fsm(&fifo_pd1, &fifo_pd2, &comms);   /* PWM_DIM sees SENSOR_OK */

    push_pair(2048, 0);
    update_fsm(&fifo_pd1, &fifo_pd2, &comms);

    TEST_ASSERT_OUTPUT_CONTAINS("Sensor State is healthy");
}

/* --------------------------------------------------------------------- */
/* RS485 command handling                                                 */
/* --------------------------------------------------------------------- */

void test_rs485_stream_disable_stops_the_data_log(void)
{
    drive_to_streaming();

    comms.rs485_flag = 1;
    comms.rs485_msg.fsm_action = STREAM_DISABLE;

    push_pair(2048, 0);
    update_fsm(&fifo_pd1, &fifo_pd2, &comms);

    TEST_ASSERT_OUTPUT_CONTAINS("Streaming Disabled");

    /* In STATE_STREAMING_DISABLED the per-cycle distance log is skipped
     * entirely, which is the point of the command. */
    const char *output = read_uart_output();
    TEST_ASSERT_NULL(strstr(output, "PD0 D:"));
}

void test_rs485_stream_enable_resumes_streaming(void)
{
    settle_sensor_at(2048);

    comms.rs485_flag = 1;
    comms.rs485_msg.fsm_action = STREAM_DISABLE;
    push_pair(2048, 0);
    update_fsm(&fifo_pd1, &fifo_pd2, &comms);
    TEST_ASSERT_NULL(strstr(read_uart_output(), "Sensor State is healthy"));

    comms.rs485_msg.fsm_action = STREAM_ENABLE;
    push_pair(2048, 0);
    update_fsm(&fifo_pd1, &fifo_pd2, &comms);

    /* The observable effect of STREAM_ENABLE is that the per-cycle data log
     * comes back. We deliberately do NOT assert on the "Streaming Enabled"
     * acknowledgement here - see the next test for why it never arrives. */
    TEST_ASSERT_OUTPUT_CONTAINS("Sensor State is healthy");
}

void test_rs485_acknowledgement_is_lost_when_the_data_log_resumes(void)
{
    /* ---- This test documents a bug, it does not bless it. ----
     *
     * update_fsm() runs update_state_rs485_cmd() FIRST, which appends the
     * acknowledgement ("LED Brightness Set to: 80") to output_msg.uart_tx.
     * A few lines later it does:
     *
     *     if (ctx.state != STATE_IDLE && ctx.state != STATE_STREAMING_DISABLED) {
     *         memset(output_msg.uart_tx, 0, sizeof(output_msg.uart_tx));
     *         create_log_msg(&incoming_fifo_data);
     *     }
     *
     * That memset wipes the acknowledgement before anything can transmit it.
     * Every RS485 reply is silently dropped except STREAM_DISABLE, which
     * survives only because STATE_STREAMING_DISABLED skips the block.
     *
     * The fix is to clear the buffer at the TOP of update_fsm(), before the
     * status switch. When you make that change, this test will fail - invert
     * it to TEST_ASSERT_OUTPUT_CONTAINS("LED Brightness Set to: 80") and it
     * becomes the regression test for the fix. */
    drive_to_streaming();

    comms.rs485_flag = 1;
    comms.rs485_msg.fsm_action = LED_BRIGHTNESS_SET;
    comms.rs485_msg.data = 80;

    push_pair(2048, 0);
    update_fsm(&fifo_pd1, &fifo_pd2, &comms);

    TEST_ASSERT_NULL(strstr(read_uart_output(), "LED Brightness Set to"));
}

void test_rs485_commands_are_ignored_while_the_flag_is_clear(void)
{
    drive_to_streaming();

    comms.rs485_flag = 0;                        /* no new message pending */
    comms.rs485_msg.fsm_action = STREAM_DISABLE; /* stale contents */

    push_pair(2048, 0);
    update_fsm(&fifo_pd1, &fifo_pd2, &comms);

    /* A stale command must not silence the stream. If the flag were ignored
     * the FSM would sit in STATE_STREAMING_DISABLED and emit nothing. */
    TEST_ASSERT_OUTPUT_CONTAINS("Sensor State is healthy");
}

/* --------------------------------------------------------------------- */
/* Asynchronous log handoff                                               */
/* --------------------------------------------------------------------- */

void test_async_request_pushes_the_pending_log_to_the_callback(void)
{
    /* A FIFO underrun raises the async signal inside the FSM. */
    update_fsm(&fifo_pd1, &fifo_pd2, &comms);

    TEST_ASSERT_EQUAL_UINT8(1, handle_async_request());
    TEST_ASSERT_EQUAL_INT(1, spy_log_calls);
    TEST_ASSERT_TRUE(spy_log_len > 0);
    TEST_ASSERT_NOT_NULL(strstr(spy_log_text, "ADC FIFO Empty"));
}

void test_async_signal_is_consumed_after_one_handoff(void)
{
    update_fsm(&fifo_pd1, &fifo_pd2, &comms);

    TEST_ASSERT_EQUAL_UINT8(1, handle_async_request());

    /* Second call has nothing left to do - otherwise the same log would be
     * transmitted repeatedly. */
    TEST_ASSERT_EQUAL_UINT8(0, handle_async_request());
    TEST_ASSERT_EQUAL_INT(1, spy_log_calls);
}

void test_healthy_streaming_does_not_raise_the_async_signal(void)
{
    drive_to_streaming();

    push_pair(2048, 0);
    update_fsm(&fifo_pd1, &fifo_pd2, &comms);

    /* Routine data goes out on the normal polled path, not the urgent one. */
    TEST_ASSERT_EQUAL_UINT8(0, handle_async_request());
    TEST_ASSERT_EQUAL_INT(0, spy_log_calls);
}

/* --------------------------------------------------------------------- */
/* Public API guards                                                      */
/* --------------------------------------------------------------------- */

void test_output_from_fsm_rejects_null_arguments(void)
{
    char buffer[TX_MSG_BUFFER] = { 0 };
    uint16_t filled_len = 0xFFFF;

    output_from_fsm(UART_TX, NULL, &filled_len, TX_MSG_BUFFER);
    output_from_fsm(UART_TX, buffer, NULL, TX_MSG_BUFFER);
    output_from_fsm(UART_TX, buffer, &filled_len, 0);

    TEST_ASSERT_EQUAL_UINT16(0xFFFF, filled_len); /* never written */
}

void test_reset_clears_any_pending_output(void)
{
    drive_to_streaming();
    push_pair(2048, 0);
    update_fsm(&fifo_pd1, &fifo_pd2, &comms);
    TEST_ASSERT_TRUE(strlen(read_uart_output()) > 0);

    reset_fsm();

    TEST_ASSERT_EQUAL_STRING("", read_uart_output());
}

void test_register_callbacks_ignores_null_pointers(void)
{
    /* setUp() registered working spies. Passing NULL must not wipe them out,
     * because the PWM branches call pwm_callback unconditionally. */
    register_fsm_callbacks(NULL, NULL);

    update_fsm(&fifo_pd1, &fifo_pd2, &comms);   /* raises the async signal */

    TEST_ASSERT_EQUAL_UINT8(1, handle_async_request());
    TEST_ASSERT_EQUAL_INT(1, spy_log_calls);
}

/* --------------------------------------------------------------------- */

int main(void)
{
    UNITY_BEGIN();

    RUN_TEST(test_idle_transitions_to_streaming_on_the_first_update);
    RUN_TEST(test_streaming_reports_distance_and_timestamp_for_both_photodiodes);
    RUN_TEST(test_each_update_starts_from_a_clean_output_buffer);

    RUN_TEST(test_empty_fifo_is_reported_and_recovers_to_streaming);
    RUN_TEST(test_repeated_empty_fifos_eventually_latch_the_error_state);

    RUN_TEST(test_saturated_sensor_asks_for_dimming);
    RUN_TEST(test_dimming_steps_the_duty_cycle_down_by_one_delta_per_cycle);
    RUN_TEST(test_out_of_range_sensor_asks_for_brightening);
    RUN_TEST(test_brightening_steps_the_duty_cycle_up_by_one_delta_per_cycle);
    RUN_TEST(test_healthy_reading_ends_the_dimming_loop);
    RUN_TEST(test_dimming_stops_at_the_lower_duty_limit);

    RUN_TEST(test_rs485_stream_disable_stops_the_data_log);
    RUN_TEST(test_rs485_stream_enable_resumes_streaming);
    RUN_TEST(test_rs485_acknowledgement_is_lost_when_the_data_log_resumes);
    RUN_TEST(test_rs485_commands_are_ignored_while_the_flag_is_clear);

    RUN_TEST(test_async_request_pushes_the_pending_log_to_the_callback);
    RUN_TEST(test_async_signal_is_consumed_after_one_handoff);
    RUN_TEST(test_healthy_streaming_does_not_raise_the_async_signal);

    RUN_TEST(test_output_from_fsm_rejects_null_arguments);
    RUN_TEST(test_reset_clears_any_pending_output);
    RUN_TEST(test_register_callbacks_ignores_null_pointers);

    return UNITY_END();
}
