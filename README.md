# IR Sensor Project

An IR proximity/distance sensing system built around dual photodiodes, covering both the electrical hardware and the firmware that drives it.

[![cpp-linter](https://github.com/cpp-linter/cpp-linter-action/actions/workflows/cpp-linter.yml/badge.svg)](https://github.com/cpp-linter/cpp-linter-action/actions/workflows/cpp-linter.yml)

## Electrical

Designed in Altium ([Electrical/board design](Electrical/board%20design)), the board integrates an IR LED emitter with a dual-photodiode receiver front end, an analog TIA (transimpedance amplifier) stage, USB and RS485 communication interfaces, and power regulation. Analog behavior is validated with SPICE simulations under `Electrical/analog spice simulations`, and datasheets/design docs are kept under `Electrical/Datasheets` and `Electrical/Design Docs`.

## Firmware

The firmware ([Firmware/](Firmware/)) runs on STM32 microcontrollers (mock/dev target: STM32F030R8, production target: STM32G030) and handles ADC acquisition via DMA, EMA low-pass filtering, distance conversion, a finite state machine for sensor status/error handling, and RS485/UART communication. See [Firmware/Doc.md](Firmware/Doc.md) for a detailed architecture overview.

The build tooling is just **STM32CubeMX** for peripheral/HAL code generation, with custom application-level APIs layered on top — no additional framework or RTOS. A Python GUI ([Firmware/gui](Firmware/gui)) is included for interacting with and visualizing sensor data over the host machine.

### Platform support

Currently Windows-only. Linux and macOS support is not implemented yet.
