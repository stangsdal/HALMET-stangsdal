# HALMET-stangsdal Firmware

This repository provides adapted firmware for [HALMET: Hat Labs Marine Engine & Tank interface](https://shop.hatlabs.fi/products/halmet), extended with support for:

- DS1603L ultrasonic tank measurement
- Signal K output via SensESP
- NMEA 2000 output over CAN
- Optional Clipper Duet HT1621 display sniffing mode (Signal K + NMEA 2000 only)

To get started with the firmware, follow the generic SensESP [Getting Started](https://signalk.org/SensESP/pages/getting_started/) instructions but use this repository instead of the SensESP Project Template.

## Default HALMET Mode

Default behavior is unchanged:

- Engine RPM from D1
- Fuel sender from A1
- DS1603L tank sensor
- Alarm on D2

This mode is built with environment `stangsdal`.

## Pin Mapping

### DS1603L Ultrasonic Tank Sensor

DS1603L connections to ESP32:

- VCC: 3V3
- GND: GND
- txPin: GPIO 16
- rxPin: GPIO 17

## Clipper Input Mode

A compile-time Clipper feature path is implemented and guarded by `ENABLE_CLIPPER_INPUT`.

In Clipper mode, firmware:

- Decodes values from HT1621 LCD traffic using ESP32 SPI slave capture
- Publishes to Signal K
- Publishes to NMEA 2000
- Does not use NMEA 0183

The dedicated PlatformIO environment is:

- `stangsdal_clipper`

It enables:

- `-D ENABLE_CLIPPER_INPUT`

### Clipper HT1621 Display Interface

Clipper HT1621 sniff pins are defined in `src/halmet_const.h`:

- HT DATA (MOSI): GPIO 12
- HT DATA OUT (MISO placeholder for SPI slave): GPIO 13
- HT CLK: GPIO 14
- HT CS: GPIO 15

CAN remains on HALMET CAN pins (RX 18, TX 19).

## Signal K Outputs

Clipper mode publishes:

- `environment.depth.belowKeel`
- `navigation.speedThroughWater`
- `navigation.trip.log`
- `navigation.log`

### Clipper Debug / Health Telemetry

Clipper mode also publishes integration telemetry to Signal K:

- `sensors.clipper.debug.locked` (0 or 1)
- `sensors.clipper.debug.consecutiveValidFrames`
- `sensors.clipper.debug.lastValidAge` (seconds)

These are intended for setup verification and sea-trial diagnostics.

## NMEA 2000 Outputs

Clipper mode transmits:

- Water Depth
- Boat Speed
- Distance Log (trip/total)

No NMEA 0183 transport is included in HALMET Clipper mode.

## Clipper Robustness Features

Implemented protections in the Clipper decoder path:

- Timeout invalidation of stale values
- Per-signal stale decay to NA
- Frame-quality gate requiring consecutive valid frames before publishing after startup/recovery

Tunable compile-time parameters in `src/clipper_feature.h`:

- `CLIPPER_DATA_TIMEOUT_MS` (default: 5000)
- `CLIPPER_MIN_VALID_FRAMES` (default: 3)
- `CLIPPER_DEBUG` (optional verbose logs)

Optional overrides are documented in `platformio.ini` under `env:stangsdal_clipper`.

## Build Notes

Use your PlatformIO environment as usual, for example:

- Default HALMET mode: `platformio run -e stangsdal`
- Clipper mode: `platformio run -e stangsdal_clipper`

## Customization

For custom wiring and feature setup, edit `src/main.cpp`.
Parts intended for customization are marked with `EDIT:` comments.
