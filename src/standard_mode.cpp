#include "standard_mode.h"

#include <memory>
#include <WiFi.h>

#include "halmet_analog.h"
#include "halmet_const.h"
#include "halmet_digital.h"
#include "halmet_display.h"
#include "n2k_senders.h"
#include "sensesp/sensors/digital_input.h"
#include "sensesp/signalk/signalk_output.h"
#include "sensesp/system/lambda_consumer.h"
#include "sensesp/transforms/frequency.h"
#include "sensesp/transforms/lambda_transform.h"
#include "sensesp/transforms/linear.h"
#include "sensesp/ui/config_item.h"
#include "sensesp_base_app.h"

#if __has_include("DS1603LSensor.h")
#include "DS1603LSensor.h"
#elif __has_include("../components/sensesp_ds1603l_sensor/include/DS1603LSensor.h")
#include "../components/sensesp_ds1603l_sensor/include/DS1603LSensor.h"
#else
#error "DS1603LSensor.h not found. Verify components/sensesp_ds1603l_sensor is available."
#endif

namespace halmet {
namespace {

N2kEngineParameterDynamicSender* CreateEngineDynamicSender(tNMEA2000* nmea2000) {
  auto* engine_dynamic_sender =
      new N2kEngineParameterDynamicSender("/NMEA 2000/Engine 1 Dynamic", 0,
                                          nmea2000);

  ConfigItem(engine_dynamic_sender)
      ->set_title("Engine 1 Dynamic")
      ->set_description("NMEA 2000 dynamic engine parameters for engine 1")
      ->set_sort_order(3010);

  return engine_dynamic_sender;
}

void SetupAnalogTemperatureInputs(const StandardModeConfig& cfg,
                                  N2kEngineParameterDynamicSender* dynamic_sender) {
  auto a1_voltage =
      new ADS1115VoltageInput(cfg.ads1115, 0, "/Sensors/A1/Voltage");
  ConfigItem(a1_voltage)
      ->set_title("A1 Raw Water Sensor Voltage")
      ->set_description("Voltage input for raw water temperature sensor")
      ->set_sort_order(3000);

  auto a1_raw_water_temp = a1_voltage->connect_to(new Linear(
      cfg.temp_scale_k_per_volt, cfg.temp_offset_k,
      "/Sensors/A1/Raw Water Temp Calibration"));
  ConfigItem(a1_raw_water_temp)
      ->set_title("A1 Raw Water Temperature Calibration")
      ->set_description("Convert A1 volts to raw water temperature in Kelvin")
      ->set_sort_order(3001);

  a1_raw_water_temp->connect_to(new SKOutputFloat(
      "propulsion.main.rawWaterTemperature",
      new SKMetadata("K", "Raw Water Temp", "Raw water temperature")));

  auto* raw_water_temp_sender =
      new N2kTemperatureSender("/NMEA 2000/Raw Water Temperature", 0,
                               N2kts_SeaTemperature, cfg.nmea2000);
  ConfigItem(raw_water_temp_sender)
      ->set_title("Raw Water Temperature NMEA 2000")
      ->set_description("NMEA 2000 temperature sender for raw water")
      ->set_sort_order(3004);
  a1_raw_water_temp->connect_to(&(raw_water_temp_sender->temperature_));

  auto a2_voltage =
      new ADS1115VoltageInput(cfg.ads1115, 1, "/Sensors/A2/Voltage");
  ConfigItem(a2_voltage)
      ->set_title("A2 Coolant Sensor Voltage")
      ->set_description("Voltage input for coolant temperature sensor")
      ->set_sort_order(3002);

  auto a2_coolant_temp = a2_voltage->connect_to(new Linear(
      cfg.temp_scale_k_per_volt, cfg.temp_offset_k,
      "/Sensors/A2/Coolant Temp Calibration"));
  ConfigItem(a2_coolant_temp)
      ->set_title("A2 Coolant Temperature Calibration")
      ->set_description("Convert A2 volts to coolant temperature in Kelvin")
      ->set_sort_order(3003);

  a2_coolant_temp->connect_to(new SKOutputFloat(
      "propulsion.main.coolantTemperature",
      new SKMetadata("K", "Coolant Temp", "Engine coolant temperature")));

  auto* coolant_temp_sender =
      new N2kTemperatureSender("/NMEA 2000/Coolant Temperature", 1,
                               N2kts_EngineRoomTemperature, cfg.nmea2000);
  ConfigItem(coolant_temp_sender)
      ->set_title("Coolant Temperature NMEA 2000")
      ->set_description("NMEA 2000 temperature sender for coolant")
      ->set_sort_order(3006);
  a2_coolant_temp->connect_to(&(coolant_temp_sender->temperature_));

  a2_coolant_temp->connect_to(dynamic_sender->temperature_);
}

sensesp::FloatProducer* SetupRawWaterFlow(const StandardModeConfig& cfg) {
  auto* raw_water_flow_input =
      new DigitalInputCounter(kDigitalInputPin3, INPUT, RISING, 500,
                              "/Flow D3/Raw Water Pulse Input");
  ConfigItem(raw_water_flow_input)
      ->set_title("D3 Raw Water Flow Input")
      ->set_description("Pulse input for raw water flow meter")
      ->set_sort_order(3020);

  auto* raw_water_flow_hz =
      new Frequency(1.0, "/Flow D3/Pulse Frequency Multiplier");
  ConfigItem(raw_water_flow_hz)
      ->set_title("D3 Raw Water Frequency")
      ->set_description("Pulse frequency from raw water flow meter")
      ->set_sort_order(3021);
  raw_water_flow_input->connect_to(raw_water_flow_hz);

  auto* raw_water_flow_m3s = raw_water_flow_hz->connect_to(
      new Linear(1.0 / (cfg.raw_water_flow_pulses_per_liter * 1000.0), 0.0,
                 "/Flow D3/Flow Calibration m3s"));
  ConfigItem(raw_water_flow_m3s)
      ->set_title("D3 Raw Water Flow Calibration")
      ->set_description("Convert flow pulses to m3/s")
      ->set_sort_order(3022);

  raw_water_flow_m3s->connect_to(new SKOutputFloat(
      "propulsion.main.rawWaterFlow",
      new SKMetadata("m3/s", "Raw Water Flow", "Raw water flow rate")));

  return raw_water_flow_m3s;
}

void SetupOilAlarmInput(const StandardModeConfig& cfg,
                        N2kEngineParameterDynamicSender* dynamic_sender) {
  auto* alarm_d2_input = ConnectAlarmSender(kDigitalInputPin2, "D2");
  alarm_d2_input->connect_to(new SKOutputBool(
      "propulsion.main.oilPressureAlarm",
      "/Alarm D2/Oil Pressure Alarm SK Path"));
  alarm_d2_input->connect_to(new LambdaConsumer<bool>([cfg](bool value) {
    if (cfg.alarm_states != nullptr) {
      cfg.alarm_states[1] = value;
    }
  }));
  alarm_d2_input->connect_to(dynamic_sender->low_oil_pressure_);
}

sensesp::FloatProducer* SetupTachoInput(const StandardModeConfig& cfg) {
  auto* tacho_d1_frequency = ConnectTachoSender(kDigitalInputPin1, "main");

  auto* engine_rapid_sender =
      new N2kEngineParameterRapidSender("/NMEA 2000/Engine 1 Rapid Update", 0,
                                        cfg.nmea2000);

  ConfigItem(engine_rapid_sender)
      ->set_title("Engine 1 Rapid Update")
      ->set_description("NMEA 2000 rapid update engine parameters for engine 1")
      ->set_sort_order(3015);

  tacho_d1_frequency->connect_to(&(engine_rapid_sender->engine_speed_));
  return tacho_d1_frequency;
}

void SetupFuelTank(const StandardModeConfig& cfg) {
  auto ds1603l_config = std::make_shared<sensesp::DS1603LConfig>();
  ds1603l_config->tx_pin = cfg.ds1603_tx_pin;
  ds1603l_config->rx_pin = cfg.ds1603_rx_pin;
  ds1603l_config->read_timeout_ms = 500;
  ds1603l_config->tank_height_mm = cfg.ds1603_tank_height_mm;
  ds1603l_config->filter_size = cfg.ds1603_filter_size;

  auto* ds1603l_sensor =
      new sensesp::DS1603LSensor(ds1603l_config, cfg.ds1603_read_interval_ms);

  auto* fuel_level_ratio = ds1603l_sensor->connect_to(
      new LambdaTransform<float, float>([cfg](float distance_m) {
        const float tank_height_m = cfg.ds1603_tank_height_mm / 1000.0f;
        if (tank_height_m <= 0.0f) {
          return 0.0f;
        }
        float level = 1.0f - (distance_m / tank_height_m);
        if (level < 0.0f) {
          return 0.0f;
        }
        if (level > 1.0f) {
          return 1.0f;
        }
        return level;
      }));

  fuel_level_ratio->connect_to(new SKOutputFloat(
      "tanks.fuel.0.currentLevel",
      new SKMetadata("ratio", "Fuel Level", "Current fuel tank level")));

  auto* fuel_volume_m3 = fuel_level_ratio->connect_to(
      new Linear(cfg.fuel_tank_capacity_m3, 0.0,
                 "/Tanks/Fuel/Volume Calibration"));
  fuel_volume_m3->connect_to(new SKOutputFloat(
      "tanks.fuel.0.currentVolume",
      new SKMetadata("m3", "Fuel Volume", "Current fuel volume")));

  auto* fuel_tank_sender =
      new N2kFluidLevelSender("/Tanks/Fuel/NMEA 2000", 0, N2kft_Fuel,
                              cfg.fuel_tank_capacity_liters, cfg.nmea2000);
  ConfigItem(fuel_tank_sender)
      ->set_title("Fuel Tank NMEA 2000")
      ->set_description("NMEA 2000 fluid level sender for fuel tank")
      ->set_sort_order(3030);
  fuel_level_ratio->connect_to(&(fuel_tank_sender->tank_level_));

  event_loop()->onRepeat(1000, [cfg, ds1603l_sensor]() {
    if (cfg.ds1603_connected_display != nullptr) {
      *cfg.ds1603_connected_display = ds1603l_sensor->is_connected();
    }
  });
}

void SetupDisplay(const StandardModeConfig& cfg,
                  sensesp::FloatProducer* tacho_d1_frequency,
                  sensesp::FloatProducer* raw_water_flow_m3s) {
  if (!cfg.display_present || cfg.display == nullptr) {
    return;
  }

  tacho_d1_frequency->connect_to(new LambdaConsumer<float>([cfg](float value) {
    PrintValue(cfg.display, 3, "RPM D1", 60 * value);
  }));
  raw_water_flow_m3s->connect_to(new LambdaConsumer<float>([cfg](float value) {
    PrintValue(cfg.display, 2, "Flow L/m", value * 60000.0f);
  }));

  event_loop()->onRepeat(1000, [cfg]() {
    PrintValue(cfg.display, 1, "IP:", WiFi.localIP().toString());
  });

  event_loop()->onRepeat(1000, [cfg]() {
    char state_string[5] = {};
    if (cfg.alarm_states != nullptr && cfg.ds1603_connected_display != nullptr) {
      cfg.alarm_states[0] = *cfg.ds1603_connected_display;
      for (int i = 0; i < 4; i++) {
        state_string[i] = cfg.alarm_states[i] ? '*' : '_';
      }
    }
    PrintValue(cfg.display, 4, "Alarm", state_string);
  });
}

}  // namespace

void SetupStandardMode(const StandardModeConfig& cfg) {
  if (cfg.nmea2000 == nullptr || cfg.ads1115 == nullptr) {
    return;
  }

  auto* engine_dynamic_sender = CreateEngineDynamicSender(cfg.nmea2000);
  SetupAnalogTemperatureInputs(cfg, engine_dynamic_sender);
  SetupOilAlarmInput(cfg, engine_dynamic_sender);

  auto* raw_water_flow_m3s = SetupRawWaterFlow(cfg);
  auto* tacho_d1_frequency = SetupTachoInput(cfg);

  SetupFuelTank(cfg);
  SetupDisplay(cfg, tacho_d1_frequency, raw_water_flow_m3s);
}

}  // namespace halmet
