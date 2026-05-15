# HALMET Mapping – SignalK & NMEA 2000

## Overview

Mapping af HALMET inputs til:

* SignalK datamodel
* NMEA 2000 standard PGN’er
* Anbefalet anvendelse (SignalK vs N2K)

Dokumentet dækker begge firmware-profiler:

* `stangsdal` (standard HALMET mode)
* `stangsdal_clipper` (Clipper Duet HT1621 sniff mode)

---

## Inputs

Sektionen nedenfor gælder standard HALMET mode (`stangsdal`).

### d1 – Tachometer

* **Title:** RPM
* **Description:** Motorens aktuelle omdrejningstal
* **SignalK:** `propulsion.main.revolutions`
* **NMEA 2000:** PGN 127488 – Engine Speed
* **Recommendation:** Send til både SignalK og NMEA 2000

---

### d2 – Olietrykskontakt

* **Title:** Oil Alarm
* **Description:** Olietrykskontakt, OK/alarm
* **SignalK:** `propulsion.main.oilPressureAlarm`
* **NMEA 2000:** PGN 127489 – Oil pressure related alarm/status
* **Recommendation:** Send til begge (hvis HALMET understøtter mapping til alarm/status)

---

### d3 – Kølevands flowmåler (saltvand)

* **Title:** Raw Water Flow
* **Description:** Flow af saltvand til køling
* **SignalK:** `propulsion.main.rawWaterFlow`
* **NMEA 2000:** Ikke standardiseret (typisk custom/proprietary)
* **Recommendation:** Primært SignalK

---

### a1 – Kølevand (saltvand)

* **Title:** Raw Water Temp
* **Description:** Temperatur på saltvand i kølekreds
* **SignalK:** `propulsion.main.rawWaterTemperature`
* **NMEA 2000:** PGN 130312 – Temperature
* **Recommendation:** Primært SignalK, evt. også NMEA 2000

---

### a2 – Kølevand (ferskvand)

* **Title:** Coolant Temp
* **Description:** Temperatur på motorens ferskvandskredsløb
* **SignalK:** `propulsion.main.coolantTemperature`
* **NMEA 2000:** PGN 127489 – Engine Temperature
* **Recommendation:** Send til både SignalK og NMEA 2000

---

### c1 – Tankmåler

* **Title:** Fuel Level
* **Description:** Brændstofniveau i procent og volumen

**SignalK:**

* `tanks.fuel.0.currentLevel`
* `tanks.fuel.0.currentVolume`

**NMEA 2000:**

* PGN 127505 – Fluid Level

**Recommendation:** Send til både SignalK og NMEA 2000

---

## Dashboard Labels (OpenPlotter)

Anbefalede visningsnavne:

* RPM
* Oil Alarm
* Raw Water Flow
* Raw Water Temp
* Coolant Temp
* Fuel Level
* Fuel Left

---

## Unit Notes (SignalK)

SignalK bruger SI-enheder:

| Parameter  | Enhed                       |
| ---------- | --------------------------- |
| RPM        | Hz (revolutions per second) |
| Temperatur | Kelvin (K)                  |
| Volume     | m³                          |
| Tank level | 0–1                         |

**Konvertering nødvendig fra:**

* RPM → Hz (divider med 60)
* Liter → m³ (divider med 1000)
* °C → K (+273.15)

---

## Recommended Strategy

### Send til både SignalK og NMEA 2000

* d1 RPM
* d2 Oil Alarm
* a2 Coolant Temp
* c1 Fuel Level

### Primært SignalK

* d3 Raw Water Flow
* a1 Raw Water Temp

---

## Clipper Duet Mode (`stangsdal_clipper`)

I Clipper mode læses værdier fra HT1621-displaybussen og publiceres direkte til
SignalK og NMEA 2000.

### SignalK Mapping (Clipper)

* `environment.depth.belowKeel`
* `navigation.speedThroughWater`
* `navigation.trip.log`
* `navigation.log`

### NMEA 2000 Mapping (Clipper)

* Water Depth
* Boat Speed
* Distance Log (trip/total)

### Clipper Debug Telemetri (SignalK)

* `sensors.clipper.debug.locked`
* `sensors.clipper.debug.consecutiveValidFrames`
* `sensors.clipper.debug.lastValidAge`

### HT1621 Pin Mapping (Clipper)

Pins defineret i `src/halmet_const.h`:

* HT DATA (MOSI): GPIO 13
* HT DATA OUT (MISO): GPIO 14
* HT CLK: GPIO 32
* HT CS: GPIO 33

CAN forbliver:

* RX: GPIO 18
* TX: GPIO 19

### Dependency Note

Clipper mode bygger på HALMET-ClipperDuet biblioteket, pinned i
`platformio.ini` til `v0.1.1`.

---

## Notes

* Olietrykskontakt (d2) er en digital status – ikke et analogt tryk
* Raw water flow er ikke en klassisk NMEA 2000 engine parameter
* SignalK bør være din **“source of truth”** i OpenPlotter setup

---
