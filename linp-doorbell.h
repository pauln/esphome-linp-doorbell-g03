#include "esphome.h"
#include "Queue.h"


#define get_linp_doorbell(constructor) static_cast<LinpDoorbell *> \
  (const_cast<custom_component::CustomComponentConstructor *>(&constructor)->get_component(0))

class LinpDoorbell : public Component {
 DataQueue<String> commandQueue;
 DataQueue<String> requests;
 DataQueue<int> buttonPresses;
 bool isChiming = false;

 public:
  Sensor *volume_sensor = new Sensor("Volume");
  Sensor *playing_sensor = new Sensor("Chime Playing");
  BinarySensor *button1_sensor = new BinarySensor("Button 1");
  BinarySensor *button2_sensor = new BinarySensor("Button 2");
  BinarySensor *button3_sensor = new BinarySensor("Button 3");
  BinarySensor *button4_sensor = new BinarySensor("Button 4");
  BinarySensor *button5_sensor = new BinarySensor("Button 5");
  BinarySensor *button6_sensor = new BinarySensor("Button 6");
  BinarySensor *button7_sensor = new BinarySensor("Button 7");
  BinarySensor *button8_sensor = new BinarySensor("Button 8");
  BinarySensor *button9_sensor = new BinarySensor("Button 9");
  BinarySensor *button10_sensor = new BinarySensor("Button 10");
  BinarySensor *button_sensors[10] = {
    button1_sensor,
    button2_sensor,
    button3_sensor,
    button4_sensor,
    button5_sensor,
    button6_sensor,
    button7_sensor,
    button8_sensor,
    button9_sensor,
    button10_sensor
  };
  Sensor *chime1_sensor = new Sensor("Button 1 Tune");
  Sensor *chime2_sensor = new Sensor("Button 2 Tune");
  Sensor *chime3_sensor = new Sensor("Button 3 Tune");
  Sensor *chime4_sensor = new Sensor("Button 4 Tune");
  Sensor *chime5_sensor = new Sensor("Button 5 Tune");
  Sensor *chime6_sensor = new Sensor("Button 6 Tune");
  Sensor *chime7_sensor = new Sensor("Button 7 Tune");
  Sensor *chime8_sensor = new Sensor("Button 8 Tune");
  Sensor *chime9_sensor = new Sensor("Button 9 Tune");
  Sensor *chime10_sensor = new Sensor("Button 10 Tune");
  Sensor *chime_sensors[10] = {
    chime1_sensor,
    chime2_sensor,
    chime3_sensor,
    chime4_sensor,
    chime5_sensor,
    chime6_sensor,
    chime7_sensor,
    chime8_sensor,
    chime9_sensor,
    chime10_sensor
  };

  LinpDoorbell() : commandQueue(255), requests(255), buttonPresses(255) { }

  float get_setup_priority() const { return setup_priority::HARDWARE; }

  void setup() override {
    Serial2.begin(115200);
    commandQueue.enqueue("down get_volume");
    commandQueue.enqueue("down get_switch_list");
  }

  void loop() override {
    if (Serial2.available() > 0) {
      String received = Serial2.readStringUntil('\r');
      ESP_LOGV("linp-doorbell", "RX: %s", received.c_str());
      String response = handleMessage(received);
      if (response.length() > 0) {
        ESP_LOGV("linp-doorbell", "TX: %s", response.c_str());
        Serial2.print(response + "\r");
      }
    }
  }

  String handleMessage(String received) {
    if (!buttonPresses.isEmpty()) {
      // Revert button state to `false`.
      int buttonIndex = buttonPresses.dequeue();
      button_sensors[buttonIndex]->publish_state(false);
    }
    if (isChiming) {
      isChiming = false;
      playing_sensor->publish_state(0.0);
    }

    if (received.equals("net")) {
      return String("local");
    } else if (received.startsWith("result ")) {
      String value = String(received);
      value.remove(0, 7);
      if (!value.equals("\"ok\"")) {
	if (requests.isEmpty()) {
          ESP_LOGI("linp-doorbell", "Unexpected property received: %s", value);
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
      ESP_LOGI("linp-doorbell", "Property received: %s = %s", param.c_str(), value.c_str());
    } else if (received.startsWith("event ")) {
      received.remove(0, 6);
      handleEvent(received);
      return String("ok");
    } else if (received.equals("get_down")) {
      if (commandQueue.isEmpty()) {
        return String("down none");
      } else {
        String response = commandQueue.dequeue();
	ESP_LOGI("linp-doorbell", "Sending command: %s", response.c_str());
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

  void handleEvent(String event) {
    ESP_LOGI("linp-doorbell", "Event received: %s", event.c_str());
    
    if (event.startsWith("switch_pressed_")) {
      event.remove(0, 15);
      int buttonIndex = atoi(event.c_str());
      buttonPresses.enqueue(buttonIndex);
      button_sensors[buttonIndex]->publish_state(true);
    } else if(event.startsWith("bell_ring")) {
      event.remove(0, 10);
      playing_sensor->publish_state(parse_float(event.c_str()).value());
      isChiming = true;
    } else if(event.startsWith("learn_success")) {
      // Button learning succeded; request a fresh list (event supplies a list, but it's space-separated).
      commandQueue.enqueue("down get_switch_list");
    }
  }

  void handleParam(String param, String value) {
    ESP_LOGI("linp-doorbell", "Param received: %s = %s", param.c_str(), value.c_str());
    if (param.equals("volume")) {
      volume_sensor->publish_state(parse_float(value.c_str()).value());
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
        auto tuneFloat = parse_float(tune.c_str());
	if (tune.equals("255")) {
          tuneFloat = -1;
        }
        chime_sensors[i]->publish_state(tuneFloat.value());
        offset = commaPos + 1;
      }
    }
  }

  void setVolume(int volume) {
    if (volume < 0 || volume > 4) {
      ESP_LOGI("linp-doorbell", "Ignoring invalid volume request: %i", volume);
      return;
    }

    ESP_LOGI("linp-doorbell", "Setting volume to: %i", volume);
    String command = String("down set_volume ");
    command.concat(volume);
    commandQueue.enqueue(command);
    commandQueue.enqueue("down get_volume");
  }

  void playTune(int tune) {
    if (tune < 1 || tune > 36) {
      ESP_LOGI("linp-doorbell", "Ignoring invalid tune request: %i", tune);
      return;
    }

    ESP_LOGI("linp-doorbell", "Playing tune: %i", tune);
    String command = String("down play_specified_music ");
    command.concat(tune);
    commandQueue.enqueue(command);
  }

  void learnButton(int tune) {
    if (tune < 1 || tune > 36) {
      ESP_LOGI("linp-doorbell", "Ignoring learn request - invalid tune: %i", tune);
      return;
    }

    ESP_LOGI("linp-doorbell", "Entering learn mode with tune: %i", tune);
    String command = String("down enter_specified_learn_mode ");
    command.concat(tune);
    commandQueue.enqueue(command);
  }

  void setTune(int button, int tune) {
    if (button < 1 || button > 10) {
      ESP_LOGI("linp-doorbell", "Ignoring set tune request - invalid button: %i", button);
      return;
    }
    if (tune < 1 || tune > 36) {
      ESP_LOGI("linp-doorbell", "Ignoring set tune request - invalid tune: %i", tune);
      return;
    }

    ESP_LOGI("linp-doorbell", "Setting button %i to tune: %i", button, tune);
    String command = String("down set_music_for_switch ");
    command.concat(button-1);
    command.concat(",");
    command.concat(tune);
    commandQueue.enqueue(command);
  }

  void forgetButton(int button) {
    if (button < 1 || button > 10) {
      ESP_LOGI("linp-doorbell", "Ignoring forget button request - invalid button: %i", button);
      return;
    }
    ESP_LOGI("linp-doorbell", "Forgetting button %i", button);

    String command = String("down delete_specified_switch ");
    command.concat(button-1);
    commandQueue.enqueue(command);

    // Doorbell sends a "switch list" param after forgetting the button.
    requests.enqueue(String("switch_list"));
  }

  void sendRawCommand(String command) {
    ESP_LOGI("linp-doorbell", "Sending raw command: %s", command.c_str());
    commandQueue.enqueue(command);
  }
};
