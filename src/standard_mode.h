#ifndef HALMET_SRC_STANDARD_MODE_H_
#define HALMET_SRC_STANDARD_MODE_H_

#include <Adafruit_ADS1X15.h>
#include <Adafruit_SSD1306.h>
#include <NMEA2000.h>

#include "standard_mode_defaults.h"

namespace halmet {

struct StandardModeConfig {
  tNMEA2000* nmea2000 = nullptr;
  Adafruit_ADS1115* ads1115 = nullptr;
  Adafruit_SSD1306* display = nullptr;
  bool display_present = false;

  bool* alarm_states = nullptr;             // expected size >= 4
  bool* ds1603_connected_display = nullptr;

  uint8_t ds1603_tx_pin = kDefaultDS1603LTxPin;
  uint8_t ds1603_rx_pin = kDefaultDS1603LRxPin;
  uint16_t ds1603_tank_height_mm = kDefaultDS1603LTankHeightMm;
  uint8_t ds1603_filter_size = kDefaultDS1603LFilterSize;
  unsigned long ds1603_read_interval_ms = kDefaultDS1603LReadIntervalMs;

  float fuel_tank_capacity_m3 = kDefaultFuelTankCapacityM3;
  uint16_t fuel_tank_capacity_liters = kDefaultFuelTankCapacityLiters;

  float temp_scale_k_per_volt = kDefaultTempScaleKPerVolt;
  float temp_offset_k = kDefaultTempOffsetK;
  float raw_water_flow_pulses_per_liter =
      kDefaultRawWaterFlowPulsesPerLiter;
};

// Configure all non-clipper inputs/outputs (Signal K, NMEA2000, display).
void SetupStandardMode(const StandardModeConfig& cfg);

}  // namespace halmet

#endif  // HALMET_SRC_STANDARD_MODE_H_
