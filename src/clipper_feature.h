#ifndef HALMET_SRC_CLIPPER_FEATURE_H_
#define HALMET_SRC_CLIPPER_FEATURE_H_

#include <NMEA2000.h>

#include "sensesp/sensors/sensor.h"

namespace halmet {

// If no valid frames are decoded for this duration, publish NA values.
#ifndef CLIPPER_DATA_TIMEOUT_MS
#define CLIPPER_DATA_TIMEOUT_MS 5000
#endif

// Number of consecutive valid frames required before publishing recovered
// values after startup or timeout.
#ifndef CLIPPER_MIN_VALID_FRAMES
#define CLIPPER_MIN_VALID_FRAMES 3
#endif

// Optional verbose decode logs.
// #define CLIPPER_DEBUG

struct ClipperInputSignals {
  sensesp::ObservableValue<float> depth_m;
  sensesp::ObservableValue<float> speed_mps;
  sensesp::ObservableValue<float> trip_m;
  sensesp::ObservableValue<float> total_m;

  // Debug/health telemetry for integration and sea trials.
  sensesp::ObservableValue<float> debug_lock_state;
  sensesp::ObservableValue<float> debug_consecutive_valid_frames;
  sensesp::ObservableValue<float> debug_last_valid_age_s;
};

// Initializes a Signal K and NMEA 2000 publishing pipeline for Clipper-derived
// depth, speed, trip and total values.
ClipperInputSignals* SetupClipperFeature(tNMEA2000* nmea2000,
                                         bool enable_signalk_output,
                                         bool enable_nmea2000_output,
                                         gpio_num_t htclk_pin,
                                         gpio_num_t htmiso_pin,
                                         gpio_num_t htmosi_pin,
                                         gpio_num_t htcs_pin);

}  // namespace halmet

#endif  // HALMET_SRC_CLIPPER_FEATURE_H_
