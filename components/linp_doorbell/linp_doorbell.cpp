#include "linp_doorbell.h"
#include "esphome/core/log.h"
#include "esphome/core/application.h"

namespace esphome {
namespace linp_doorbell {

static const char *const TAG = "linp_doorbell";

float LinpDoorbellComponent::get_setup_priority() const { return setup_priority::HARDWARE; }

void LinpDoorbellComponent::setup() {
  Serial2.begin(115200);
  hasSetVolume = false;
  commandQueue.enqueue("down none");
  commandQueue.enqueue("down none");
  commandQueue.enqueue("down none");
  commandQueue.enqueue("down get_volume");
  commandQueue.enqueue("down get_switch_list");

  if (this->use_old_service_names_) {
    register_service(&LinpDoorbellComponent::setVolume, "linp_set_volume", {"volume"});
    register_service(&LinpDoorbellComponent::playTune, "linp_play_tune", {"tune"});
    register_service(&LinpDoorbellComponent::stopTune, "linp_stop_tune");
    register_service(&LinpDoorbellComponent::learnButton, "linp_learn_button", {"tune"});
    register_service(&LinpDoorbellComponent::setTune, "linp_set_tune", {"button", "tune"});
    register_service(&LinpDoorbellComponent::forgetButton, "linp_forget_button", {"button"});
    register_service(&LinpDoorbellComponent::sendRawCommand, "linp_send_raw_command", {"command"});
  } else {
    register_service(&LinpDoorbellComponent::setVolume, "set_volume", {"volume"});
    register_service(&LinpDoorbellComponent::playTune, "play_tune", {"tune"});
    register_service(&LinpDoorbellComponent::stopTune, "stop_tune");
    register_service(&LinpDoorbellComponent::learnButton, "learn_button", {"tune"});
    register_service(&LinpDoorbellComponent::setTune, "set_tune", {"button", "tune"});
    register_service(&LinpDoorbellComponent::forgetButton, "forget_button", {"button"});
    register_service(&LinpDoorbellComponent::sendRawCommand, "send_raw_command", {"command"});
  }
}

void LinpDoorbellComponent::dump_config() {
  ESP_LOGCONFIG(TAG, "Doorbell Config:");

  LOG_SENSOR("  ", "Volume", this->volume_sensor_);
  LOG_SENSOR("  ", "Chime Playing", this->chime_playing_sensor_);
}

void LinpDoorbellComponent::loop() {
  if (Serial2.available() > 0) {
    String received = Serial2.readStringUntil('\r');
    ESP_LOGV(TAG, "RX: %s", received.c_str());
    String response = handleMessage(received);
    if (response.length() > 0) {
      ESP_LOGV(TAG, "TX: %s", response.c_str());
      Serial2.print(response + "\r");
    }
  }
}

String LinpDoorbellComponent::handleMessage(String received) {
  if (isChiming) {
    isChiming = false;
    if (this->chime_playing_sensor_ != nullptr)
      this->chime_playing_sensor_->publish_state(0.0);
  }

  if (received.equals("net")) {
    return String("local");
  } else if (received.startsWith("result ")) {
    String value = String(received);
    value.remove(0, 7);
    if (!value.equals("\"ok\"")) {
      if (requests.isEmpty()) {
        ESP_LOGD(TAG, "Unexpected property received: %s", value.c_str());
      } else {
        String param = requests.dequeue();
        handleParam(param, value);
      }
    }
  } else if (received.startsWith("props ")) {
    String param = String(received);
    param.remove(0, 6);
    String value = String(param);
    int spacePos = param.indexOf(" ");
    param.remove(spacePos, param.length() - spacePos);
    value.remove(0, spacePos + 1);
    ESP_LOGD(TAG, "Property received: %s = %s", param.c_str(), value.c_str());
  } else if (received.startsWith("event ")) {
    received.remove(0, 6);
    handleEvent(received);
    return String("ok");
  } else if (received.equals("get_down")) {
    if (commandQueue.isEmpty()) {
      return String("down none");
    } else {
      String response = commandQueue.dequeue();
      ESP_LOGD(TAG, "Sending command: %s", response.c_str());
      if (response.startsWith("down get_")) {
        // Strip the "down get_" prefix off.
        String request = String(response);
        request.remove(0, 9);
        requests.enqueue(request);
      } else if (response.startsWith("down set_music")) {
        requests.enqueue(String("music"));
      }
      return response;
    }
  }

  return "";
}

void LinpDoorbellComponent::handleEvent(String event) {
  ESP_LOGI(TAG, "Event received: %s", event.c_str());
  
  if (event.startsWith("switch_pressed_")) {
    event.remove(0, 15);
    int buttonIndex = atoi(event.c_str());

    char buttonStr[2];
    itoa(buttonIndex+1, buttonStr, 10); // Increment to get a 1-based number
    fire_homeassistant_event("esphome.linp_doorbell_button_pressed", {
      {"button", buttonStr},
      {"device", App.get_name()},
    });
  } else if(event.startsWith("bell_ring")) {
    event.remove(0, 10);

    if (this->chime_playing_sensor_ != nullptr)
      this->chime_playing_sensor_->publish_state(parse_number<float>(event.c_str()).value());
    isChiming = true;

    fire_homeassistant_event("esphome.linp_doorbell_tune_played", {
      {"tune", event.c_str()},
      {"device", App.get_name()},
    });
  } else if(event.startsWith("learn_success")) {
    // Button learning succeded; request a fresh list (event supplies a list, but it's space-separated).
    commandQueue.enqueue("down get_switch_list");
  }
}

void LinpDoorbellComponent::handleParam(String param, String value) {
  ESP_LOGD(TAG, "Param received: %s = %s", param.c_str(), value.c_str());
  if (param.equals("volume")) {
    if (!hasSetVolume) {
      // Volume needs to be reset on boot to put it back to the last set value.
      // This also jogs the doorbell to life so that it'll actually chime on first button press.
      // If we don't do this, it won't chime on first press but will on subsequent presses.
      int initialVolume = atoi(value.c_str());
      ESP_LOGI(TAG, "Setting initial volume: %i", initialVolume);
      hasSetVolume = true;
      setVolume(initialVolume);
    }
    if (this->volume_sensor_ != nullptr)
      this->volume_sensor_->publish_state(parse_number<float>(value.c_str()).value());
  } else if (param.equals("switch_list")) {
    // Comma-separated list of button tunes.
    int offset = 0;
    for(int i=0; i<10; i++) {
      int commaPos = value.indexOf(',', offset);
      if (commaPos == -1 && i < 9) {
        // Comma not found.  Value might be malformed; we won't be able to find any more tunes, so stop here.
        break;
      }
      String tune = value.substring(offset, commaPos);
      auto tuneFloat = parse_number<float>(tune.c_str());
      if (tune.equals("255")) {
        tuneFloat = -1;
      }
      //chime_sensors[i]->publish_state(tuneFloat.value());
      offset = commaPos + 1;
    }
  }
}

void LinpDoorbellComponent::setVolume(int volume) {
  if (volume < 0 || volume > 4) {
    ESP_LOGI(TAG, "Ignoring invalid volume request: %i", volume);
    return;
  }

  ESP_LOGI(TAG, "Setting volume to: %i", volume);
  String command = String("down set_volume ");
  command.concat(volume);
  commandQueue.enqueue(command);
  commandQueue.enqueue("down get_volume");
}

void LinpDoorbellComponent::playTune(int tune) {
  if (tune < 1 || tune > 36) {
    ESP_LOGI(TAG, "Ignoring invalid tune request: %i", tune);
    return;
  }

  ESP_LOGI(TAG, "Playing tune: %i", tune);
  String command = String("down play_specified_music ");
  command.concat(tune);
  commandQueue.enqueue(command);
}

void LinpDoorbellComponent::stopTune() {
  ESP_LOGI(TAG, "Stopping tune");
  commandQueue.enqueue(String("down stop_play"));
}

void LinpDoorbellComponent::learnButton(int tune) {
  if (tune < 1 || tune > 36) {
    ESP_LOGI(TAG, "Ignoring learn request - invalid tune: %i", tune);
    return;
  }

  ESP_LOGI(TAG, "Entering learn mode with tune: %i", tune);
  String command = String("down enter_specified_learn_mode ");
  command.concat(tune);
  commandQueue.enqueue(command);
}

void LinpDoorbellComponent::setTune(int button, int tune) {
  if (button < 1 || button > 10) {
    ESP_LOGI(TAG, "Ignoring set tune request - invalid button: %i", button);
    return;
  }
  if (tune < 1 || tune > 36) {
    ESP_LOGI(TAG, "Ignoring set tune request - invalid tune: %i", tune);
    return;
  }

  ESP_LOGI(TAG, "Setting button %i to tune: %i", button, tune);
  String command = String("down set_music_for_switch ");
  command.concat(button-1);
  command.concat(",");
  command.concat(tune);
  commandQueue.enqueue(command);
}

void LinpDoorbellComponent::forgetButton(int button) {
  if (button < 1 || button > 10) {
    ESP_LOGI(TAG, "Ignoring forget button request - invalid button: %i", button);
    return;
  }
  ESP_LOGI(TAG, "Forgetting button %i", button);

  String command = String("down delete_specified_switch ");
  command.concat(button-1);
  commandQueue.enqueue(command);

  // Doorbell sends a "switch list" param after forgetting the button.
  requests.enqueue(String("switch_list"));
}

void LinpDoorbellComponent::sendRawCommand(std::string command) {
  ESP_LOGI(TAG, "Sending raw command: %s", command.c_str());
  commandQueue.enqueue(String(command.c_str()));
}

}  // namespace linp_doorbell
}  // namespace esphome
