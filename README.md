# Smart Sensor & Power Monitor

Prototype of a universal embedded module for monitoring the power and
state of a small electronic system. Built on **ESP32** with **ESP-IDF and
FreeRTOS**, it measures voltage, current and temperature, calculates power
and accumulated energy, raises a warning when thresholds are exceeded,
keeps an event journal in Flash and streams telemetry to a host PC and LCD display.

## About the project

This is **Case 1.5** from Engineering Project Sprint 2026, developed by a
team of five. My part was the firmware.

The goal of the case was a working prototype (a board or a breadboard
stand) with a microcontroller, sensors, a communication interface and
baseline firmware, together with a schematic, a BOM, a PCB concept and a
plan for moving to a production revision. The brief suggested an
STM32F1xx-based design as an example; we implemented it on ESP32

## Why ESP32

The brief suggested STM32F1xx as an example platform. We chose ESP32 for
these reasons:

- **Familiar toolchain.** I already had hands-on experience with ESP-IDF
  and FreeRTOS, which mattered with only two days available: no time was
  spent learning a new framework or RTOS.
- **Wi-Fi on board.** We planned to stream telemetry to a phone over
  Wi-Fi. It was dropped due to the time limit, but the hardware already
  supports it, so it remains a natural next step.
- **Enough resources for the task.** Dual-core MCU with built-in Flash
  that covers the event journal requirement (>= 100 records) without any
  external memory, which also simplified the PCB.

<p align="center">
  <img src="docs/prototype.jpg" alt="Smart Sensor & Power Monitor prototype on a perfboard: ESP32, INA219 module, SH1106 OLED, status LEDs" width="600">
</p>
<p align="center"><em>Prototype on a perfboard: ESP32 DevKit, INA219, SH1106 OLED, two DS18B20 and status LEDs.</em></p>
  
