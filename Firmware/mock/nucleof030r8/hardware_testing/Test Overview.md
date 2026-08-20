# Hardware Test Overview

## Setup

- **DUT:** ST Nucleo-F030R8 running the IR sensor firmware (FSM + PWM control loop).
- **Stimulus/measurement instrument:** Digilent Analog Discovery 2 (AD2), connected via flying leads to the Nucleo's Arduino header (photodiode ADC inputs, PWM output, ground reference).

The AD2's power supplies / AWG channels are used to source static DC voltages into the two photodiode input channels (PD1/PD2), standing in for the analog signal the IR photodiode front-end would normally produce at a given target distance. The AD2's scope channels are used to monitor the resulting PWM output from the Nucleo.

## What's being tested

The firmware's FSM (`fsm.c`) classifies the photodiode signal on every sample against two thresholds:

- **Saturation** — ADC reading above `SATURATION_LIMIT` (~3.2 V), meaning the target is too close and the sensor front-end is clipping.
- **Too far** — ADC reading below `OUT_OF_RANGE_LIMIT` (~40 mV), meaning the target is too far and the return signal is too weak to trust.

When either condition is detected, the FSM drives the IR LED PWM duty cycle toward a compensation target instead of leaving it fixed:

- `STATE_SATURATION` → `STATE_PWM_DIM`, ramping duty cycle down toward `PWM_DIM_TARGET` (35%) in `PWM_DELTA` (1%) steps.
- `STATE_TOO_FAR` → `STATE_PWM_BRIGHTEN`, ramping duty cycle up toward `PWM_BRIGHTEN_TARGET` (65%) in the same 1% steps.

This is the same idea as active/automatic gain compensation (AGC): rather than a fixed emitter drive, the loop continuously trims LED brightness to keep the received signal in a usable range regardless of target distance.

## Procedure

1. Flash the firmware to the Nucleo and connect the AD2 leads to the PD1/PD2 analog inputs and the PWM output pin.
2. Using the AD2 (WaveForms), apply a static DC voltage above the saturation threshold to a photodiode input and confirm:
   - FSM transitions into `STATE_SATURATION` then `STATE_PWM_DIM`.
   - PWM duty cycle steps downward, 1% per FSM tick, converging on 35%.
3. Sweep/step the DC input down through the out-of-range threshold and confirm:
   - FSM transitions into `STATE_TOO_FAR` then `STATE_PWM_BRIGHTEN`.
   - PWM duty cycle steps upward, 1% per FSM tick, converging on 65%.
4. Apply a DC voltage between the two thresholds and confirm the FSM holds `STATE_STREAMING` with no PWM adjustment (baseline/no-compensation-needed case).
5. Capture the PWM duty cycle transition on the AD2 scope channel to verify the ramp rate and settling behavior visually.

## Video Footage
[![Video Link](https://youtube.com/shorts/1NGU2nRBifU)](https://youtube.com/shorts/1NGU2nRBifU)
