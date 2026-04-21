#include "clipper_feature.h"

#include <N2kMessages.h>
#include <SPI.h>

#include <ESP32SPISlave.h>

#include "sensesp/signalk/signalk_output.h"
#include "sensesp/system/lambda_consumer.h"
#include "sensesp_base_app.h"

namespace halmet {
namespace {

#ifdef CLIPPER_DEBUG
#define CLIPPER_LOGD(...) debugD(__VA_ARGS__)
#else
#define CLIPPER_LOGD(...)
#endif

constexpr uint32_t kSpiBufferSize = 36;

class ClipperSpiDecoder {
 public:
  ClipperSpiDecoder(gpio_num_t htclk_pin, gpio_num_t htmiso_pin,
                    gpio_num_t htmosi_pin, gpio_num_t htcs_pin,
                    ClipperInputSignals* signals)
      : htclk_pin_(htclk_pin),
        htmiso_pin_(htmiso_pin),
        htmosi_pin_(htmosi_pin),
        htcs_pin_(htcs_pin),
        signals_(signals) {
    memset(spi_rx_buf_, 0, sizeof(spi_rx_buf_));
  }

  void begin() {
    slave_.setDataMode(SPI_MODE3);
    slave_.begin(HSPI, htclk_pin_, htmiso_pin_, htmosi_pin_, htcs_pin_);

    sensesp::event_loop()->onRepeat(10, [this]() { this->poll(); });

    // Decay stale values to NA if updates stop.
    sensesp::event_loop()->onRepeat(250, [this]() { this->invalidate_stale(); });

    // Publish decoder health metrics for integration diagnostics.
    sensesp::event_loop()->onRepeat(500, [this]() { this->publish_diagnostics(); });
  }

 private:
  bool frame_quality_ok() const {
    return consecutive_valid_frames_ >= CLIPPER_MIN_VALID_FRAMES;
  }

  void mark_invalid_frame() { consecutive_valid_frames_ = 0; }

  void mark_valid_frame() {
    if (consecutive_valid_frames_ < 255) {
      consecutive_valid_frames_++;
    }
  }

  static char SevenSegToChar(uint8_t bits) {
    switch (bits & 0x7f) {
      case 0x00:
        return ' ';
      case 0x3f:
        return '0';
      case 0x06:
        return '1';
      case 0x5b:
        return '2';
      case 0x4f:
        return '3';
      case 0x66:
        return '4';
      case 0x6d:
        return '5';
      case 0x7d:
        return '6';
      case 0x07:
        return '7';
      case 0x7f:
        return '8';
      case 0x6f:
        return '9';
      default:
        return '?';
    }
  }

  uint8_t segdata(uint8_t seg, uint8_t com) const {
    constexpr uint8_t shift_by = 9;  // 3 command bits + 6 address bits
    return ((spi_rx_buf_[(seg * 4 + com + shift_by) / 8] >>
             (7 - ((seg * 4 + com + shift_by) % 8))) &
            1);
  }

  uint8_t mkdigit0() const {
    return (segdata(13, 1) << 6) | (segdata(14, 0) << 5) |
           (segdata(14, 1) << 4) | (segdata(15, 1) << 3) |
           (segdata(12, 1) << 2) | (segdata(12, 0) << 1) | segdata(13, 0);
  }

  uint8_t mkdigit1() const {
    return (segdata(11, 1) << 6) | (segdata(15, 0) << 5) |
           (segdata(7, 1) << 4) | (segdata(7, 0) << 3) |
           (segdata(10, 1) << 2) | (segdata(10, 0) << 1) | segdata(11, 0);
  }

  uint8_t mkdigit2() const {
    return (segdata(8, 1) << 6) | (segdata(9, 0) << 5) |
           (segdata(9, 1) << 4) | (segdata(6, 1) << 3) |
           (segdata(16, 1) << 2) | (segdata(16, 0) << 1) | segdata(8, 0) |
           (segdata(6, 0) << 7);
  }

  uint8_t mkdigit3() const {
    return (segdata(26, 1) << 6) | (segdata(25, 0) << 5) |
           (segdata(25, 1) << 4) | (segdata(28, 1) << 3) |
           (segdata(27, 1) << 2) | (segdata(27, 0) << 1) | segdata(26, 0);
  }

  uint8_t mkdigit4() const {
    return (segdata(3, 1) << 6) | (segdata(4, 1) << 5) |
           (segdata(4, 0) << 4) | (segdata(3, 0) << 3) |
           (segdata(2, 0) << 2) | (segdata(2, 1) << 1) | segdata(5, 1);
  }

  uint8_t mkdigit5() const {
    return (segdata(0, 1) << 6) | (segdata(1, 1) << 5) |
           (segdata(1, 0) << 4) | (segdata(0, 0) << 3) |
           (segdata(17, 0) << 2) | (segdata(17, 1) << 1) | segdata(23, 1) |
           (segdata(23, 0) << 7);
  }

  uint8_t mkdigit6() const {
    return (segdata(18, 1) << 6) | (segdata(22, 1) << 5) |
           (segdata(22, 0) << 4) | (segdata(18, 0) << 3) |
           (segdata(19, 0) << 2) | (segdata(19, 1) << 1) | segdata(21, 1);
  }

  uint8_t mkinfo1() const {
    return (segdata(28, 0) << 7) | (segdata(29, 0) << 6) |
           (segdata(31, 1) << 5) | (segdata(30, 0) << 4) |
           (segdata(30, 1) << 3) | (segdata(31, 0) << 2) |
           (segdata(29, 1) << 1) | segdata(24, 1);
  }

  uint8_t mkinfo2() const {
    return (segdata(6, 0) << 5) | (segdata(23, 0) << 4) |
           (segdata(5, 0) << 3) | (segdata(24, 0) << 2) |
           (segdata(20, 1) << 1) | segdata(20, 0);
  }

  uint32_t digits2int(char d0, char d1, char d2, char d3, bool dot) const {
    int x = N2kUInt32NA;

    if (d0 >= '0' && d0 <= '9') {
      x = d0 - '0';
      x *= 10;
    }

    if (d1 == ' ') {
      x = 0;
    } else if (d1 >= '0' && d1 <= '9') {
      if (x == N2kUInt32NA) {
        x = d1 - '0';
      } else {
        x += d1 - '0';
      }
      x *= 10;
    } else {
      return N2kUInt32NA;
    }

    if (d2 >= '0' && d2 <= '9') {
      x += d2 - '0';
      x *= 10;
    } else {
      return N2kUInt32NA;
    }

    if (d3 >= '0' && d3 <= '9') {
      x += d3 - '0';
    } else {
      return N2kUInt32NA;
    }

    return dot ? x : x * 10;
  }

  double rowa2double(char d0, char d1, char d2, char d3, uint8_t info2) const {
    const uint32_t val = digits2int(d0, d1, d2, d3, ((info2 & (1 << 5)) != 0));
    return (val != N2kUInt32NA) ? static_cast<double>(val) / 10.0 : N2kDoubleNA;
  }

  double rowb2double(char d4, char d5, char d6, uint8_t info2) const {
    const uint32_t val =
        digits2int(' ', d4, d5, d6, ((info2 & (1 << 4)) != 0));
    return (val != N2kUInt32NA) ? static_cast<double>(val) / 10.0 : N2kDoubleNA;
  }

  void decode_and_publish() {
    const char d0 = SevenSegToChar(mkdigit0());
    const char d1 = SevenSegToChar(mkdigit1());
    const char d2 = SevenSegToChar(mkdigit2());
    const char d3 = SevenSegToChar(mkdigit3());
    const char d4 = SevenSegToChar(mkdigit4());
    const char d5 = SevenSegToChar(mkdigit5());
    const char d6 = SevenSegToChar(mkdigit6());
    const uint8_t info1 = mkinfo1();
    const uint8_t info2 = mkinfo2();

    // Parse only the regular data display mode.
    if ((info2 & (1 << 2)) == 0) {
      mark_invalid_frame();
      return;
    }

    saw_valid_data_ = true;
    last_valid_data_ms_ = millis();
    mark_valid_frame();

    if (!frame_quality_ok()) {
      CLIPPER_LOGD("Clipper warmup: valid frames %u/%u",
                   consecutive_valid_frames_, CLIPPER_MIN_VALID_FRAMES);
      return;
    }

    double depth = rowb2double(d4, d5, d6, info2);
    if (depth != N2kDoubleNA) {
      if ((info2 & 0x01) == 0x01) {
        signals_->depth_m.set(static_cast<float>(depth));
        last_depth_ms_ = millis();
        CLIPPER_LOGD("Clipper depth(m): %.2f", depth);
      } else if ((info2 & 0x02) == 0x02) {
        signals_->depth_m.set(static_cast<float>(depth / 3.281));
        last_depth_ms_ = millis();
        CLIPPER_LOGD("Clipper depth(ft->m): %.2f", depth / 3.281);
      } else {
        signals_->depth_m.set(N2kDoubleNA);
      }
    }

    if (info1 < (1 << 6)) {
      double speed = rowa2double(d0, d1, d2, d3, info2);
      if (speed == N2kDoubleNA) {
        signals_->speed_mps.set(N2kDoubleNA);
        return;
      }

      if ((info1 & 0b00100000) == 0b00100000) {
        signals_->speed_mps.set(static_cast<float>(speed * 0.514444444444));
        last_speed_ms_ = millis();
        CLIPPER_LOGD("Clipper speed(kn->m/s): %.2f", speed * 0.514444444444);
      } else if ((info1 & 0b00001100) == 0b00001100) {
        signals_->speed_mps.set(static_cast<float>(speed / 3.6));
        last_speed_ms_ = millis();
        CLIPPER_LOGD("Clipper speed(km/h->m/s): %.2f", speed / 3.6);
      } else if ((info1 & 0b00010000) == 0b00010000) {
        signals_->speed_mps.set(static_cast<float>(speed / 2.237));
        last_speed_ms_ = millis();
        CLIPPER_LOGD("Clipper speed(mph->m/s): %.2f", speed / 2.237);
      }
    } else {
      double distance = rowa2double(d0, d1, d2, d3, info2);
      if (distance == N2kDoubleNA) {
        return;
      }

      if ((info1 & 0b00000011) == 0b00000011) {
        distance *= 1852.0;
      } else if ((info1 & 0b00001000) == 0b00001000) {
        distance *= 1000.0;
      } else if ((info1 & 0b00000001) == 0b00000001) {
        distance *= 1609.0;
      } else {
        return;
      }

      if ((info1 & 0b10000000) == 0b10000000) {
        signals_->trip_m.set(static_cast<float>(distance));
        last_trip_ms_ = millis();
        CLIPPER_LOGD("Clipper trip(m): %.1f", distance);
      } else if ((info1 & 0b01000000) == 0b01000000) {
        signals_->total_m.set(static_cast<float>(distance));
        last_total_ms_ = millis();
        CLIPPER_LOGD("Clipper total(m): %.1f", distance);
      }
    }
  }

  void invalidate_stale() {
    const unsigned long now = millis();

    if (!saw_valid_data_) {
      return;
    }

    // If the SPI decode stream is stale, clear all values.
    if ((now - last_valid_data_ms_) > CLIPPER_DATA_TIMEOUT_MS) {
      signals_->depth_m.set(N2kDoubleNA);
      signals_->speed_mps.set(N2kDoubleNA);
      signals_->trip_m.set(N2kDoubleNA);
      signals_->total_m.set(N2kDoubleNA);
      saw_valid_data_ = false;
      consecutive_valid_frames_ = 0;
      CLIPPER_LOGD("Clipper data timeout: clearing all values");
      return;
    }

    if ((now - last_depth_ms_) > CLIPPER_DATA_TIMEOUT_MS) {
      signals_->depth_m.set(N2kDoubleNA);
    }
    if ((now - last_speed_ms_) > CLIPPER_DATA_TIMEOUT_MS) {
      signals_->speed_mps.set(N2kDoubleNA);
    }
    if ((now - last_trip_ms_) > CLIPPER_DATA_TIMEOUT_MS) {
      signals_->trip_m.set(N2kDoubleNA);
    }
    if ((now - last_total_ms_) > CLIPPER_DATA_TIMEOUT_MS) {
      signals_->total_m.set(N2kDoubleNA);
    }
  }

  void publish_diagnostics() {
    signals_->debug_lock_state.set(frame_quality_ok() ? 1.0f : 0.0f);
    signals_->debug_consecutive_valid_frames.set(
        static_cast<float>(consecutive_valid_frames_));

    if (saw_valid_data_) {
      signals_->debug_last_valid_age_s.set(
          static_cast<float>(millis() - last_valid_data_ms_) / 1000.0f);
    } else {
      signals_->debug_last_valid_age_s.set(NAN);
    }
  }

  void poll() {
    // Queue a new SPI slave transaction if not already queued
    static bool transaction_queued = false;
    if (!transaction_queued) {
      slave_.queue(nullptr, spi_rx_buf_, kSpiBufferSize);
      transaction_queued = true;
    }

    // Check if transaction completed
    size_t num_bytes = slave_.numBytesReceived();
    if (num_bytes > 0) {
      transaction_queued = false;  // Reset for next transaction

      if (num_bytes > 2 && num_bytes < kSpiBufferSize &&
          ((spi_rx_buf_[0] & 0b11100000) == 0b10100000)) {
        decode_and_publish();
      } else {
        mark_invalid_frame();
      }
    }
  }

  gpio_num_t htclk_pin_;
  gpio_num_t htmiso_pin_;
  gpio_num_t htmosi_pin_;
  gpio_num_t htcs_pin_;
  ClipperInputSignals* signals_;

  ESP32SPISlave slave_;
  uint8_t spi_rx_buf_[kSpiBufferSize]{};

  bool saw_valid_data_ = false;
  uint8_t consecutive_valid_frames_ = 0;
  unsigned long last_valid_data_ms_ = 0;
  unsigned long last_depth_ms_ = 0;
  unsigned long last_speed_ms_ = 0;
  unsigned long last_trip_ms_ = 0;
  unsigned long last_total_ms_ = 0;
};

class ClipperN2kPublisher {
 public:
  explicit ClipperN2kPublisher(tNMEA2000* nmea2000) : nmea2000_(nmea2000) {
    depth_m_ = N2kDoubleNA;
    speed_mps_ = N2kDoubleNA;
    trip_m_ = N2kDoubleNA;
    total_m_ = N2kDoubleNA;

    sensesp::event_loop()->onRepeat(1000, [this]() {
      tN2kMsg n2k_msg;
      SetN2kWaterDepth(n2k_msg, sid_++, depth_m_, 0.0, N2kDoubleNA);
      nmea2000_->SendMsg(n2k_msg);

      SetN2kBoatSpeed(n2k_msg, sid_++, speed_mps_, N2kDoubleNA,
                      N2kSWRT_Paddle_wheel);
      nmea2000_->SendMsg(n2k_msg);
    });

    sensesp::event_loop()->onRepeat(5000, [this]() {
      tN2kMsg n2k_msg;
      SetN2kDistanceLog(n2k_msg, 0, 0.0, total_m_, trip_m_);
      nmea2000_->SendMsg(n2k_msg);
    });
  }

  void set_depth(float value) { depth_m_ = value; }
  void set_speed(float value) { speed_mps_ = value; }
  void set_trip(float value) { trip_m_ = value; }
  void set_total(float value) { total_m_ = value; }

 private:
  tNMEA2000* nmea2000_;
  uint8_t sid_ = 0;
  double depth_m_;
  double speed_mps_;
  double trip_m_;
  double total_m_;
};

}  // namespace

ClipperInputSignals* SetupClipperFeature(tNMEA2000* nmea2000,
                                         bool enable_signalk_output,
                                         bool enable_nmea2000_output,
                                         gpio_num_t htclk_pin,
                                         gpio_num_t htmiso_pin,
                                         gpio_num_t htmosi_pin,
                                         gpio_num_t htcs_pin) {
  auto* signals = new ClipperInputSignals();

  if (enable_signalk_output) {
    signals->depth_m.connect_to(new sensesp::SKOutputFloat(
        "environment.depth.belowKeel", "/sensors/depth/sk"));
    signals->speed_mps.connect_to(new sensesp::SKOutputFloat(
        "navigation.speedThroughWater", "/sensors/speed/sk"));
    signals->trip_m.connect_to(
        new sensesp::SKOutputFloat("navigation.trip.log", "/sensors/trip/sk"));
    signals->total_m.connect_to(
        new sensesp::SKOutputFloat("navigation.log", "/sensors/total/sk"));

    signals->debug_lock_state.connect_to(new sensesp::SKOutputFloat(
        "sensors.clipper.debug.locked", "/sensors/clipper/debug/locked"));
    signals->debug_consecutive_valid_frames.connect_to(new sensesp::SKOutputFloat(
        "sensors.clipper.debug.consecutiveValidFrames",
        "/sensors/clipper/debug/consecutive_valid_frames"));
    signals->debug_last_valid_age_s.connect_to(new sensesp::SKOutputFloat(
        "sensors.clipper.debug.lastValidAge", "/sensors/clipper/debug/last_valid_age"));
  }

  if (enable_nmea2000_output) {
    auto* publisher = new ClipperN2kPublisher(nmea2000);

    signals->depth_m.connect_to(new sensesp::LambdaConsumer<float>(
        [publisher](float value) { publisher->set_depth(value); }));
    signals->speed_mps.connect_to(new sensesp::LambdaConsumer<float>(
        [publisher](float value) { publisher->set_speed(value); }));
    signals->trip_m.connect_to(new sensesp::LambdaConsumer<float>(
        [publisher](float value) { publisher->set_trip(value); }));
    signals->total_m.connect_to(new sensesp::LambdaConsumer<float>(
        [publisher](float value) { publisher->set_total(value); }));
  }

  auto* decoder =
      new ClipperSpiDecoder(htclk_pin, htmiso_pin, htmosi_pin, htcs_pin, signals);
  decoder->begin();

  return signals;
}

}  // namespace halmet
