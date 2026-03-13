# Live Trail Uploader Architecture

## Overview

The firmware is split into focused modules with clear ownership:

1. Main orchestration
   - File: Live-Trail-Conditions-CPP-Uploader.c
   - Boot, initialize modules, run upload cycle, then sleep.

2. Configuration
   - File: app_config.h
   - Central place for SIM/API settings, pin map, calibration constants, and sleep interval.

3. Modem
   - Files: modem.h, modem.c
   - UART lifecycle, PWRKEY handling, AT command transport, network attach, HTTP POST, low-power modem state.

4. Sensors
   - Files: sensors.h, sensors.c
   - ADC setup, moisture sampling, battery measurement and normalization.

5. Sleep control
   - Files: power_sleep.h, power_sleep.c
   - RTC alarm setup and low-power wait loop.

6. Upload application logic
   - Files: upload_cycle.h, upload_cycle.c
   - Builds JSON payload and coordinates sensor + modem operations.

7. Generic utilities
   - Files: utils.h, utils.c
   - Shared math helpers used by sensors.

## Design Goals

1. Separation of concerns: each module has one clear responsibility.
2. Single source of truth: all constants live in app_config.h.
3. Reusability: modem/sensor/sleep logic can evolve independently.
4. Testability: upload flow is isolated in upload_cycle.c.

## Build Integration

CMakeLists.txt now compiles all module source files into one firmware target.

## Runtime Flow

1. Boot and initialize sensors + modem.
2. Read moisture and battery.
3. Attach modem to network.
4. POST JSON payload to API.
5. Put modem in low-power state.
6. Sleep using RTC alarm.
7. Repeat forever.
