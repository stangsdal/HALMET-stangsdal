// Signal K application template file.
//
// This application demonstrates core SensESP concepts in a very
// concise manner. You can build and upload the application as is
// and observe the value changes on the serial port monitor.
//
// You can use this source file as a basis for your own projects.
// Remove the parts that are not relevant to you, and add your own code
// for external hardware libraries.

#include <Adafruit_ADS1X15.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <N2kMessages.h>
#include <NMEA2000_esp32.h>
#include <memory>

#include <HALMET_ClipperDuet.h>
#include "sensesp/net/discovery.h"
#include "sensesp/system/lambda_consumer.h"
#include "sensesp/system/system_status_led.h"
#include "sensesp_app_builder.h"
#define BUILDER_CLASS SensESPAppBuilder

#include "halmet_const.h"
#include "halmet_display.h"
#include "halmet_serial.h"
#include "standard_mode_defaults.h"
#include "standard_mode.h"
#include "sensesp/net/http_server.h"
#include "sensesp/net/networking.h"

using namespace sensesp;
using namespace halmet;

#ifndef PROJECT_VERSION
#define PROJECT_VERSION "0.9.0"
#endif

/////////////////////////////////////////////////////////////////////
// Declare some global variables required for the firmware operation.

tNMEA2000* nmea2000;
elapsedMillis n2k_time_since_rx = 0;
elapsedMillis n2k_time_since_tx = 0;

TwoWire* i2c;
Adafruit_SSD1306* display;

// Store alarm states in an array for local display output
bool alarm_states[4] = {false, false, false, false};
float clipper_depth_display = NAN;
float clipper_speed_display = NAN;
bool ds1603_connected_display = false;

#ifdef ENABLE_CLIPPER_INPUT
hamlet::clipperduet::Decoder clipper_decoder;
uint8_t clipper_sid = 0;
#if defined(HAMLET_CLIPPERDUET_HAS_SPI_CAPTURE)
hamlet::clipperduet::Esp32SpiCapture clipper_capture({
  static_cast<int8_t>(kClipperHTClkPin),
  static_cast<int8_t>(kClipperHTDataOutPin),
  static_cast<int8_t>(kClipperHTDataPin),
  static_cast<int8_t>(kClipperHTCSPin),
});
#endif
#endif

// Set the ADS1115 GAIN to adjust the analog input voltage range.
// On HALMET, this refers to the voltage range of the ADS1115 input
// AFTER the 33.3/3.3 voltage divider.

// GAIN_TWOTHIRDS: 2/3x gain +/- 6.144V  1 bit = 3mV      0.1875mV (default)
// GAIN_ONE:       1x gain   +/- 4.096V  1 bit = 2mV      0.125mV
// GAIN_TWO:       2x gain   +/- 2.048V  1 bit = 1mV      0.0625mV
// GAIN_FOUR:      4x gain   +/- 1.024V  1 bit = 0.5mV    0.03125mV
// GAIN_EIGHT:     8x gain   +/- 0.512V  1 bit = 0.25mV   0.015625mV
// GAIN_SIXTEEN:   16x gain  +/- 0.256V  1 bit = 0.125mV  0.0078125mV

const adsGain_t kADS1115Gain = GAIN_ONE;

/////////////////////////////////////////////////////////////////////
// Test output pin configuration. If ENABLE_TEST_OUTPUT_PIN is defined,
// GPIO 33 will output a pulse wave at 380 Hz with a 50% duty cycle.
// If this output and GND are connected to one of the digital inputs, it can
// be used to test that the frequency counter functionality is working.
#define ENABLE_TEST_OUTPUT_PIN
#if defined(ENABLE_TEST_OUTPUT_PIN) && !defined(ENABLE_CLIPPER_INPUT)
const int kTestOutputPin = GPIO_NUM_33;
// With the default pulse rate of 100 pulses per revolution (configured in
// halmet_digital.cpp), this frequency corresponds to 3.8 r/s or about 228 rpm.
const int kTestOutputFrequency = 380;
#endif

/////////////////////////////////////////////////////////////////////
// The setup function performs one-time application initialization.
void setup() {
  SetupLogging(ESP_LOG_DEBUG);

  // These calls can be used for fine-grained control over the logging level.
  // esp_log_level_set("*", esp_log_level_t::ESP_LOG_DEBUG);

  Serial.begin(115200);

  /////////////////////////////////////////////////////////////////////
  // Initialize the application framework

  // Construct the global SensESPApp() object
  BUILDER_CLASS builder;
  sensesp_app = (&builder)
                    // EDIT: Set a custom hostname for the app.
                    ->set_hostname("BoatMeter")
                    // EDIT: Optionally, hard-code the WiFi and Signal K server
                    // settings. This is normally not needed.
                    ->set_wifi_client("Stangsdal", "CarpeDiem")
                    //->set_sk_server("192.168.10.3", 80)
                    // EDIT: Enable OTA updates with a password.
                    ->enable_ota("red_deer")
                    ->get_app();

  // initialize the I2C bus
  i2c = new TwoWire(0);
  i2c->begin(kSDAPin, kSCLPin);

  // Initialize ADS1115
  auto ads1115 = new Adafruit_ADS1115();

  ads1115->setGain(kADS1115Gain);
  bool ads_initialized = ads1115->begin(kADS1115Address, i2c);
  debugD("ADS1115 initialized: %d", ads_initialized);

#if defined(ENABLE_TEST_OUTPUT_PIN) && !defined(ENABLE_CLIPPER_INPUT)
  pinMode(kTestOutputPin, OUTPUT);
  // Set the LEDC peripheral to a 13-bit resolution
  ledcAttach(kTestOutputPin, kTestOutputFrequency, 13);
  // Set the duty cycle to 50%
  // Duty cycle value is calculated based on the resolution
  // For 13-bit resolution, max value is 8191, so 50% is 4096
  ledcWrite(0, 4096);
#endif

  /////////////////////////////////////////////////////////////////////
  // Initialize NMEA 2000 functionality

  nmea2000 = new tNMEA2000_esp32(kCANTxPin, kCANRxPin);

  // Reserve enough buffer for sending all messages.
  nmea2000->SetN2kCANSendFrameBufSize(250);
  nmea2000->SetN2kCANReceiveFrameBufSize(250);

  // Set Product information
  // EDIT: Change the values below to match your device.
  nmea2000->SetProductInformation(
      "20231229",  // Manufacturer's Model serial code (max 32 chars)
      104,         // Manufacturer's product code
      "HALMET",    // Manufacturer's Model ID (max 33 chars)
      PROJECT_VERSION,  // Manufacturer's Software version code (max 40 chars)
      PROJECT_VERSION   // Manufacturer's Model version (max 24 chars)
  );

  // For device class/function information, see:
  // http://www.nmea.org/Assets/20120726%20nmea%202000%20class%20&%20function%20codes%20v%202.00.pdf

  // For mfg registration list, see:
  // https://actisense.com/nmea-certified-product-providers/
  // The format is inconvenient, but the manufacturer code below should be
  // one not already on the list.

  // EDIT: Change the class and function values below to match your device.
  nmea2000->SetDeviceInformation(
      GetBoardSerialNumber(),  // Unique number. Use e.g. Serial number.
      140,                     // Device function: Engine
      50,                      // Device class: Propulsion
      2046);                   // Manufacturer code

  nmea2000->SetMode(tNMEA2000::N2km_NodeOnly,
                    71  // Default N2k node address
  );
  nmea2000->EnableForward(false);
  nmea2000->Open();

  // No need to parse the messages at every single loop iteration; 1 ms will do
  event_loop()->onRepeat(1, []() { nmea2000->ParseMessages(); });

  // Initialize the OLED display
  bool display_present = InitializeSSD1306(sensesp_app->get(), &display, i2c);

  auto ws_client = sensesp_app->get()->get_ws_client();
  if (display_present && ws_client != nullptr) {
    event_loop()->onRepeat(1000, [ws_client]() {
      PrintStatusLine(display, ws_client->is_connected(),
                      ds1603_connected_display);
    });
  }

#ifdef ENABLE_CLIPPER_INPUT
  ///////////////////////////////////////////////////////////////////
  // Clipper input feature path

#if defined(HAMLET_CLIPPERDUET_HAS_SPI_CAPTURE)
  clipper_capture.Begin();

  event_loop()->onRepeat(5, []() {
    uint8_t frame[hamlet::clipperduet::kFrameBufferSize] = {0};
    size_t frame_size = 0;

    if (!clipper_capture.ReadFrame(frame, &frame_size)) {
      return;
    }

    const hamlet::clipperduet::Event event =
        clipper_decoder.ProcessFrame(frame, frame_size, millis());
    if (!event.valid_frame || !event.operational_mode) {
      return;
    }

    const auto& data = clipper_decoder.data();
    clipper_depth_display =
        (data.depth_m == N2kDoubleNA) ? NAN : static_cast<float>(data.depth_m);
    clipper_speed_display =
        (data.speed_mps == N2kDoubleNA) ? NAN : static_cast<float>(data.speed_mps);

    tN2kMsg msg;
    clipper_sid = static_cast<uint8_t>((clipper_sid + 1) & 0xFF);

    if (event.depth_ready) {
      clipper_decoder.BuildWaterDepthMessage(msg, clipper_sid);
      nmea2000->SendMsg(msg);
    }

    if (event.speed_ready) {
      clipper_decoder.BuildBoatSpeedMessage(msg, clipper_sid);
      nmea2000->SendMsg(msg);
    }

    if (event.distance_log_ready) {
      // These timestamps can be wired to a real clock source later.
      const double days_since_1970 = 0;
      const double seconds_since_midnight = 0;
      clipper_decoder.BuildDistanceLogMessage(msg, days_since_1970,
                                              seconds_since_midnight);
      nmea2000->SendMsg(msg);
    }
  });
#else
  debugE("HALMET_ClipperDuet SPI capture helper unavailable: missing ESP32SPISlave");
#endif

  if (display_present) {
    event_loop()->onRepeat(1000, []() {
      PrintValue(display, 1, "IP:", WiFi.localIP().toString());
    });
    event_loop()->onRepeat(1000, []() {
      PrintValue(display, 2, "Depth", clipper_depth_display);
      PrintValue(display, 3, "Speed", clipper_speed_display);
      PrintValue(display, 4, "Mode", "CLIPPER");
    });
  }

#else
  StandardModeConfig standard_mode_cfg;
  standard_mode_cfg.nmea2000 = nmea2000;
  standard_mode_cfg.ads1115 = ads1115;
  standard_mode_cfg.display = display;
  standard_mode_cfg.display_present = display_present;
  standard_mode_cfg.alarm_states = alarm_states;
  standard_mode_cfg.ds1603_connected_display = &ds1603_connected_display;
    standard_mode_cfg.ds1603_tx_pin = kDefaultDS1603LTxPin;
    standard_mode_cfg.ds1603_rx_pin = kDefaultDS1603LRxPin;
    standard_mode_cfg.ds1603_tank_height_mm = kDefaultDS1603LTankHeightMm;
    standard_mode_cfg.ds1603_filter_size = kDefaultDS1603LFilterSize;
    standard_mode_cfg.ds1603_read_interval_ms = kDefaultDS1603LReadIntervalMs;
    standard_mode_cfg.fuel_tank_capacity_m3 = kDefaultFuelTankCapacityM3;
    standard_mode_cfg.fuel_tank_capacity_liters = kDefaultFuelTankCapacityLiters;
    standard_mode_cfg.temp_scale_k_per_volt = kDefaultTempScaleKPerVolt;
    standard_mode_cfg.temp_offset_k = kDefaultTempOffsetK;
  standard_mode_cfg.raw_water_flow_pulses_per_liter =
      kDefaultRawWaterFlowPulsesPerLiter;

  SetupStandardMode(standard_mode_cfg);

#endif  // ENABLE_CLIPPER_INPUT

  // To avoid garbage collecting all shared pointers created in setup(),
  // loop from here.
  while (true) {
    loop();
  }
}

void loop() { event_loop()->tick(); }
