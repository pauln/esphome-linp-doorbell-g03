#pragma once

#include "esphome/core/component.h"
#include "esphome/components/api/custom_api_device.h"
#include "esphome/components/sensor/sensor.h"
#include "ArduinoQueue.h"

namespace esphome {
namespace linp_doorbell {

class LinpDoorbellComponent : public Component, api::CustomAPIDevice {
 public:
  LinpDoorbellComponent() : commandQueue(255), requests(255) { }

  void set_volume_sensor(sensor::Sensor *volume_sensor) { volume_sensor_ = volume_sensor; }
  void set_chime_playing_sensor(sensor::Sensor *chime_playing_sensor) { chime_playing_sensor_ = chime_playing_sensor; }
  void set_use_old_service_names(bool use_old_service_names) { use_old_service_names_ = use_old_service_names; }

  void setup() override;
  void dump_config() override;
  float get_setup_priority() const override;
  void loop() override;

 protected:
  String handleMessage(String received);
  void handleEvent(String event);
  void handleParam(String param, String value);
  void setVolume(int volume);
  void playTune(int tune);
  void stopTune();
  void learnButton(int tune);
  void setTune(int button, int tune);
  void forgetButton(int button);
  void sendRawCommand(std::string command);

  sensor::Sensor *volume_sensor_;
  sensor::Sensor *chime_playing_sensor_;

 private:
  ArduinoQueue<String> commandQueue;
  ArduinoQueue<String> requests;
  bool isChiming = false;
  bool hasSetVolume = false;
  bool use_old_service_names_{false};
};

}  // namespace linp_doorbell
}  // namespace esphome