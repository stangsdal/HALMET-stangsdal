# HALMET-stangsdal Firmware

This repository provides adapted firmware for [HALMET: Hat Labs Marine Engine & Tank interface](https://shop.hatlabs.fi/products/halmet), extended with support for:

- DS1603L ultrasonic tank measurement
- Signal K output via SensESP
- NMEA 2000 output over CAN
- Optional Clipper Duet HT1621 display sniffing mode (Signal K + NMEA 2000 only)

To get started with the firmware, follow the generic SensESP [Getting Started](https://signalk.org/SensESP/pages/getting_started/) instructions but use this repository instead of the SensESP Project Template.

## Default HALMET Mode

Default mode is built with environment `stangsdal` and configures the following mappings:

- d1 (GPIO 23): Engine RPM
- d2 (GPIO 25): Oil pressure alarm
- d3 (GPIO 27): Raw water flow pulse input
- a1 (ADS1115 ch0): Raw water temperature
- a2 (ADS1115 ch1): Coolant temperature
- c1 (DS1603L): Fuel level + volume

Implementation entry point for this mode is in `src/standard_mode.cpp`.

## Pin Mapping

### HALMET Inputs

- D1: GPIO 23 (tachometer)
- D2: GPIO 25 (oil pressure alarm)
- D3: GPIO 27 (raw water flow)
- A1: ADS1115 channel 0 (raw water temperature)
- A2: ADS1115 channel 1 (coolant temperature)

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

### Original Project Description (Clipper)

The Clipper project goal is to make legacy Clipper instrument data available on
modern boat data networks without changing the original Clipper display system.

The implementation reads HT1621 display bus traffic from the Clipper instrument,
decodes the visible values, and republishes them to:

- Signal K (for dashboards, logging, and integrations)
- NMEA 2000 (for chartplotters and marine devices on CAN)

This approach is non-invasive from a functional perspective: Clipper remains the
source instrument, while HALMET acts as a protocol bridge and data publisher.

Scope of Clipper mode in this repository:

- Decode depth, speed-through-water, trip, and total log
- Publish decoded values in SI units
- Add diagnostics for data quality and stale-data detection
- Support commissioning and sea-trial verification through debug telemetry

Out of scope in Clipper mode:

- NMEA 0183 transport
- Writing back to or controlling Clipper instrument behavior

### Clipper Architecture Overview

At runtime, the Clipper pipeline consists of these layers:

1. HT1621 frame capture (ESP32 SPI slave sniffing)
2. Frame decoding and unit conversion
3. Signal quality gating (minimum valid frame streak)
4. Stale-value invalidation (timeout to NA)
5. Publication to Signal K and NMEA 2000

External module used by HALMET:

- `https://github.com/stangsdal/HALMET-ClipperDuet`

### Hardware Integration Notes

For stable decoding, keep wiring short and consistent ground reference between
HALMET and the tapped Clipper signal lines.

Recommended installation workflow:

1. Connect GND first
2. Connect HT CLK and HT CS
3. Connect HT DATA and HT DATA OUT
4. Boot in Clipper environment and verify debug lock telemetry
5. Validate live value updates before sea trial

### Commissioning Checklist (Clipper)

1. Build environment `stangsdal_clipper`
2. Confirm Signal K keys update:
	- `environment.depth.belowKeel`
	- `navigation.speedThroughWater`
	- `navigation.trip.log`
	- `navigation.log`
3. Confirm NMEA 2000 recipients see depth/speed/log values
4. Confirm diagnostics:
	- `sensors.clipper.debug.locked` transitions to `1`
	- `sensors.clipper.debug.consecutiveValidFrames` increases
	- `sensors.clipper.debug.lastValidAge` stays low during active traffic
5. Simulate disconnect or idle and verify stale values decay to NA

### Acceptance Criteria

Clipper project can be considered complete for deployment when:

- Decoded values match Clipper display readings within expected tolerance
- Signal K and NMEA 2000 outputs remain stable during normal operation
- Timeout handling correctly clears stale data
- Startup/recovery does not publish unstable garbage frames
- Sea-trial logs show reliable continuity for depth/speed/log channels

### Clipper HT1621 Display Interface

Clipper HT1621 sniff pins are defined in `src/halmet_const.h`:

- HT DATA (MOSI): GPIO 13
- HT DATA OUT (MISO placeholder for SPI slave): GPIO 14
- HT CLK: GPIO 32
- HT CS: GPIO 33

CAN remains on HALMET CAN pins (RX 18, TX 19).

## Signal K Outputs

### Standard Mode (`stangsdal`)

- `propulsion.main.revolutions` (d1)
- `propulsion.main.oilPressureAlarm` (d2)
- `propulsion.main.rawWaterFlow` (d3)
- `propulsion.main.rawWaterTemperature` (a1)
- `propulsion.main.coolantTemperature` (a2)
- `tanks.fuel.0.currentLevel` (c1)
- `tanks.fuel.0.currentVolume` (c1)

### Clipper Mode (`stangsdal_clipper`)

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

### Standard Mode (`stangsdal`)

- PGN 127488 Engine Parameters, Rapid Update (engine speed from d1)
- PGN 127489 Engine Parameters, Dynamic:
	- low oil pressure status from d2
	- engine temperature from a2
- PGN 130312 Temperature:
	- raw water temperature from a1
	- coolant temperature from a2
- PGN 127505 Fluid Level (fuel level from c1)

### Clipper Mode (`stangsdal_clipper`)

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

Decoder and capture behavior are provided by HALMET-ClipperDuet. Optional build
flags for Clipper mode are documented in `platformio.ini` under
`env:stangsdal_clipper`.

## Build Notes

Use your PlatformIO environment as usual, for example:

- Default HALMET mode: `platformio run -e stangsdal`
- Clipper mode: `platformio run -e stangsdal_clipper`

## Customization

For custom wiring and feature setup:

- Common app/bootstrap setup: `src/main.cpp`
- Standard mode sensor mapping: `src/standard_mode.cpp`
- Clipper mode wiring and publish path:
	`src/main.cpp` (uses HALMET-ClipperDuet decoder/capture)

The HALMET-ClipperDuet dependency is pinned in `platformio.ini` to `v0.1.1`.

Some customizable parts are marked with `EDIT:` comments.

## Standard Mode Calibration Defaults

Defined in `src/standard_mode_defaults.h` and applied by `src/standard_mode.cpp`:

- Temperature conversion (A1/A2):
	- `kDefaultTempScaleKPerVolt = 20.0`
	- `kDefaultTempOffsetK = 273.15`
- Raw water flow conversion:
	- `kDefaultRawWaterFlowPulsesPerLiter = 450.0`
- Fuel tank capacity:
	- `kDefaultFuelTankCapacityM3 = 0.2` (200 L)
