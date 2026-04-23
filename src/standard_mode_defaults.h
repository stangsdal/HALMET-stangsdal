#ifndef HALMET_SRC_STANDARD_MODE_DEFAULTS_H_
#define HALMET_SRC_STANDARD_MODE_DEFAULTS_H_

#include <stdint.h>

namespace halmet {

// DS1603L tank sensor defaults
constexpr uint8_t kDefaultDS1603LTxPin = 16;
constexpr uint8_t kDefaultDS1603LRxPin = 17;
constexpr uint16_t kDefaultDS1603LTankHeightMm = 1000;
constexpr uint8_t kDefaultDS1603LFilterSize = 15;
constexpr unsigned long kDefaultDS1603LReadIntervalMs = 2000;

// Fuel tank defaults
constexpr float kDefaultFuelTankCapacityM3 = 0.2f;  // 200 liters
constexpr uint16_t kDefaultFuelTankCapacityLiters = 200;

// Temperature conversion defaults (A1/A2)
constexpr float kDefaultTempScaleKPerVolt = 20.0f;
constexpr float kDefaultTempOffsetK = 273.15f;

// Flow meter default (D3)
constexpr float kDefaultRawWaterFlowPulsesPerLiter = 450.0f;

}  // namespace halmet

#endif  // HALMET_SRC_STANDARD_MODE_DEFAULTS_H_
