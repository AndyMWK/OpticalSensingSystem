# Host-side unit tests

These tests compile the hardware-independent firmware modules with your PC's
compiler and run them as ordinary programs. No board, no debugger, no flashing
— the whole suite runs in about a third of a second.

```
ctest --test-dir build/host-tests --output-on-failure
```

---

## 1. Running them

The MSYS2 UCRT64 toolchain is not on the system `PATH`, so put it there for the
session first:

```powershell
$env:PATH = "C:\msys64\ucrt64\bin;$env:PATH"
```

Then, from the `ir_sensor_mock` directory:

```powershell
cmake -S tests -B build/host-tests -G Ninja   # configure (only after changing CMake files)
cmake --build build/host-tests                # compile
ctest --test-dir build/host-tests --output-on-failure
```

Day to day you only need the last two. Useful variations:

| Command | What it does |
| --- | --- |
| `ctest --test-dir build/host-tests -R fsm` | Run only suites matching "fsm" |
| `ctest --test-dir build/host-tests -V` | Show every assertion, not just failures |
| `.\build\host-tests\src\test_fsm.exe` | Run one suite directly — the fastest edit/run loop |

To make the toolchain permanent instead of per-session, add
`C:\msys64\ucrt64\bin` to your user `PATH` in Windows environment variables.

---

## 2. The four pieces, and why each exists

### Piece 1 — a second, separate CMake project

`../CMakeLists.txt` builds the firmware, and `../CMakePresets.json` pins it to
`cmake/gcc-arm-none-eabi.cmake`. Everything it produces is Cortex-M0 machine
code, which your PC cannot execute.

So `tests/CMakeLists.txt` is its own `project()` with **no toolchain file**.
CMake falls back to the native compiler, and the output is a normal `.exe`.
Same source files, two different compilers, two different build directories.
The firmware build is completely untouched by any of this.

```
ir_sensor_mock/
├── CMakeLists.txt          ── firmware  → arm-none-eabi-gcc → .elf
└── tests/
    └── CMakeLists.txt      ── tests     → gcc               → .exe
```

### Piece 2 — the HAL stub

This is the part that takes the most explaining, because it looks like a hack
until you see the constraint.

Every module header starts with `#include "main.h"`, and `Core/Inc/main.h`
starts with `#include "stm32f0xx_hal.h"`. That HAL header only compiles for ARM.

The obvious fix — put a different `main.h` earlier on the include path —
does not work. When `fsm.h` writes `#include "main.h"` with **quotes**, the
compiler searches `fsm.h`'s own directory first. `Core/Inc/main.h` always wins,
no matter what `-I` flags you pass.

So `fakes/hal_stub.h` does two things instead:

1. **`#define __MAIN_H`** — that is `main.h`'s own include guard. By the time
   the real `main.h` is reached, its `#ifndef __MAIN_H` is already false and the
   entire file, HAL include and all, evaluates to nothing.
2. **Supplies the handful of HAL type names** the module headers still mention
   (`UART_HandleTypeDef`, `ADC_HandleTypeDef`, …) as empty structs, since they
   are only ever used as pointers.

The stub is injected with GCC's `-include` flag (set in `tests/CMakeLists.txt`),
which processes a header before line 1 of every `.c` file. Nothing in `Core/`
is edited, and the ARM build never sees any of it.

**When you hit "unknown type name" after adding a module to the tests**, the
answer is almost always: add a minimal stand-in to `fakes/hal_stub.h`.

### Piece 3 — Unity, the test framework

[Unity](https://github.com/ThrowTheSwitch/Unity) is a small C assertion library
built for embedded work — pure C99, no C++, no dynamic allocation, small enough
to also run on the target later if you want to.

You do not need to install it. `FetchContent` in `tests/CMakeLists.txt`
downloads a pinned release at configure time and caches it under
`build/host-tests/_deps`.

A Unity test file has a fixed shape:

```c
#include "unity.h"

void setUp(void)    { /* runs before EVERY test */ }
void tearDown(void) { /* runs after  EVERY test */ }

void test_something_specific(void)
{
    TEST_ASSERT_EQUAL_UINT16(1234, actual);
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_something_specific);
    return UNITY_END();     // returns the failure count
}
```

`setUp` and `tearDown` are mandatory — Unity calls them by name and the link
fails if they are missing. There is no automatic test discovery in C, so each
test must be listed in `RUN_TEST`. Forgetting that line is the single most
common way to end up with a test that silently never runs.

Assertions used in this suite:

| Macro | Use |
| --- | --- |
| `TEST_ASSERT_EQUAL_UINT8/16/32` | Integers, with the width spelled out |
| `TEST_ASSERT_EQUAL_INT` | Enums (`sensor_status_t`, `fsm_state_t`) |
| `TEST_ASSERT_FLOAT_WITHIN(tol, expected, actual)` | Floats — never compare with `==` |
| `TEST_ASSERT_EQUAL_STRING` | Whole-string match |
| `TEST_ASSERT_TRUE` / `_NULL` / `_NOT_NULL` | Everything else |
| `TEST_FAIL_MESSAGE("...")` | Custom failure text |

### Piece 4 — CTest, the runner

`add_test()` in `src/CMakeLists.txt` registers each executable with CTest.
The contract is just the process exit code: `0` means pass. `UNITY_END()`
returns the number of failures, so this wires up with no glue code.

One executable per test file is deliberate. `fsm.c` and `ir_processing.c` both
hold state in file-scope `static` variables, and a separate process per file
guarantees that state cannot leak between files.

---

## 3. Adding a test

**A new case in an existing suite** — write the function, then add its
`RUN_TEST(...)` line in `main()`. Nothing else.

**A new suite** — drop `test_<module>.c` in `src/`, and add its filename to
`TEST_SOURCES` in `src/CMakeLists.txt`. Re-run the `cmake -S tests -B ...`
configure step so CMake picks up the new file.

**A new module under test** — add its `.c` to `fw_under_test` in
`tests/CMakeLists.txt`. If it calls HAL functions directly (`logger.c`,
`rs485.c` and `ir_adc.c` all do), you first need to give the linker a
definition for each one. Write a `fakes/hal_fake.c` with a recording
implementation — the same spy pattern `test_fsm.c` already uses for its
callbacks:

```c
/* in fakes/hal_fake.c */
int hal_uart_transmit_calls;

HAL_StatusTypeDef HAL_UART_Transmit(UART_HandleTypeDef *h, uint8_t *d,
                                    uint16_t size, uint32_t timeout)
{
    (void)h; (void)d; (void)size; (void)timeout;
    hal_uart_transmit_calls++;
    return 0;
}
```

...then add that file to the `fw_under_test` sources and the matching type
declarations to `hal_stub.h`.

---

## 4. Notes on what is actually tested

**44 tests across 3 suites.**

`test_fifo.c` (9) — pure logic, no hidden state, the easiest module to test and
the best one to read first. Covers ordering, capacity, the overwrite-oldest
overflow policy, index wraparound and NULL guards.

`test_ir_processor.c` (14) — the ADC-to-distance model, the saturation and
out-of-range thresholds (including the exact boundary values), and the EMA
filter's step response.

`test_fsm.c` (21) — state transitions, log output, the closed-loop PWM
brightness control, RS485 command handling and the async log handoff.

### Two testability problems these tests had to work around

Both are worth knowing about, because they are the kind of thing that only
becomes visible once you try to write the first test.

**1. `ir_processing.c`'s EMA cache cannot be reset.** `static ema_lpf_cache_t
ema_mem` persists for the life of the process and has no public reset. A test
that assumed the filter started at zero would pass or fail depending on which
test ran before it. The workaround is `settle_ema_at()` — drive the filter to a
known level first. The proper fix is a small `void ir_processing_reset(void);`
in the module, called from `setUp()`.

**2. `reset_fsm()` does not reset everything.** It clears `ctx` and the UART
buffer, but not `ir_led_pwm` (so the duty cycle carries between tests) and not
`async_signal`. The tests handle this by draining the async signal in `setUp()`
and by asserting on *relative* duty-cycle change rather than absolute values.

### Two firmware issues the tests surfaced

**`update_fsm()` erases RS485 acknowledgements before they can be sent.**
`update_state_rs485_cmd()` appends the reply to `output_msg.uart_tx`, then a few
lines later `update_fsm()` runs `memset(output_msg.uart_tx, 0, ...)` before
building the data log. Every acknowledgement is lost except `STREAM_DISABLE`,
which survives only because `STATE_STREAMING_DISABLED` skips that block.
`test_rs485_acknowledgement_is_lost_when_the_data_log_resumes` pins the current
behaviour and explains how to invert it once the `memset` is moved to the top
of `update_fsm()`.

**`STATE_ERROR` is unreachable.** It is only entered from
`handle_sensor_error()`, which only runs on `SENSOR_RATE_LIMIT` — and
`ir_processing.c` never returns that status, because `verify_rate_limit()` is
still commented out. So `MAX_ERROR_COUNT` currently has no terminal state to
escalate to. Noted in
`test_repeated_empty_fifos_eventually_latch_the_error_state`.

There is also a `/* just for debugging */` line in the `STATE_IDLE` branch of
`fsm.c` that forces `STATE_STREAMING` regardless of the RS485 command.
`test_idle_transitions_to_streaming_on_the_first_update` documents it so it is
not shipped by accident.
