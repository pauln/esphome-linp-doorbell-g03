# ESPHome custom component for linp-doorbell-g03

## WARNING - new incompatible board revision
This custom component (including the `external_components` branch) was built for a version of the doorbell which runs the doorbell functionality on a separate microcontroller (see below) which communicates with the ESP32 over serial.  However, a new revision appears to have removed this secondary microcontroller (presumably the ESP32 is now handling this, interfacing directly with a CMT2217B OOK RF receiver), thereby rendering this custom component incompatible with newer hardware.

The device still carries the same model number externally, so unfortunately, there may not be an easy way to tell which version you have before you crack the case open.  **Before you attempt to flash this firmware to your doorbell**, you should check whether your board matches the [photos in the issue discussing the new revision](https://github.com/pauln/esphome-linp-doorbell-g03/issues/23#issuecomment-984352857), as this component won't do anything of use on the new revision.  The board revision details can be found under a sticker beside the ESP32 if you'd like confirmation of which you have - see the aforementioned issue for more details.

## Background
The linp-doorbell-g03 is a wifi doorbell with a self-powered button.  It's part of the Mijia (Xiaomi smart home) ecosystem, but it unfortunately doesn't seem to announce events (such as the button being pressed) on the local network.  As such, this project aims to provide replacement firmware to enable full local control via [ESPHome](https://esphome.io/) - which provides for simple integration with [Home Assistant](https://www.home-assistant.io/).

## Hardware
The doorbell's base unit is pressed/clipped together - there are no screws or other fasteners holding it together.  The seam between the two halves can be found running around the perimeter of the device.

**With the device unplugged**, carefully pry / pull the two halves apart.

Once you've got it open, you'll see an ESP32 module (which handles the wifi/network/cloud control, but not the basic doorbell functionality) with a row of test points beside it, marked (from top to bottom):
- GND
- TXD0
- RXD0
- 3.3V
- GND
- TXD1
- RXD1

The first three pins (GND, TXD0, RXD0) can be used to communicate directly with the ESP32, whilst the last three (GND, TXD1, RXD1) are used for communication between the ESP32 and the STM8S005K6 microcontroller which runs the basic doorbell functionality.

If you lift the main circuit board out of the plastic case, you'll find "5V" and "GND" pads near the top on the back/bottom, which can be used to power the entire unit instead of the mains.  I highly recommend that you do this while working on the doorbell, as it's much safer to use a USB power supply (or other isolated 5V DC supply) than to try powering it from the mains while reflashing the ESP32.  I desoldered the mains input wires from the circuit board, which allows the circuit board to be completely removed from the plastic case (if you unplug the speaker).

## ESP32 quirks
The ESP32 module used in this device is a single-core variant, which means that although you can compile code with the standard Arduino and/or [PlatformIO](https://platformio.org/) ESP32 libraries, it likely won't run on this ESP32.

Additionally, the MAC address CRC written to the EFUSE at the factory seems to be incorrect, so a modification to the core libraries is required to disable the reset on MAC CRC check failure.  The original firmware includes this modification.

This is all handled by the use of the `solo-no-mac-crc/1.0.4` branch of [my fork of the arduino-esp32 repository](https://github.com/pauln/arduino-esp32), which can also be used in other PlatformIO 4.0+ projects by overriding the `framework-arduinoespressif32` package with this branch in the `platform_packages` configuration.  A separate branch (`solo/1.0.4`) is also provided for use with other single-core ESP32 modules which do have correct MAC address CRCs.

I chose the PlatformIO board configuration `esp32doit-devkit-v1`, as the pinout seems to match this model.

## Requirements for flashing
- ESPHome
- PlatformIO 4.0 or higher, so that the `platform_packages` configuration option is available
- A serial-to-USB adapter

## Configuration
- Set your own WiFi credentials in the `wifi:` section of the YAML file, so that it knows how to connect to your network
- In the `sensor:` and `binary_sensor:` sections, you can reduce the number of `Button X Tune` and `Button X` entries if you only want to have states for the buttons which you actually have
- If your button-triggered automation will be solely handled by Home Assistant (i.e. you're not intending to use ESPHome's on-node automation), you can remove the optional parts of the yaml - the entire `binary_sensor:` section and the "Tune Playing" `sensor`.
- Technically, all of the sensors are optional (as of v0.4) - but it probably makes sense to keep at least the `Volume` sensor, and perhaps the `Button X Tune` sensor(s) for as many buttons as you have, so that the doorbell's current configuration is reported to Home Assistant.  The remaining sensors may not be much use unless you plan to use on-node ESPHome automations.

## Flashing the ESP32
In order to boot the ESP32 into flash mode (so that you can write ESPHome to it), you'll need to pull the two pins closest the bottom right corner of the ESP32 module to ground; I did this by soldering fine wires to the two pins and one of the ground pads (either from the test points listed above, or the second from the top on the "P3" test points to the left of the ESP32), but if you have an easier way to reliably short the two corner pads to ground temporarily, anything which is easy to undo should work fine.

1. Connect GND, TXD0, RXD0 to a serial-to-USB adapter (making sure to connect the ESP's RXD0 to your adapter's TX (or TXD) pin and the ESP's TXD0 to your adapter's RX (or RXD) pin)
2. Connect your serial-to-USB adapter to your computer, making note of the port which it shows up on (depending on your OS, this could be something along the lines of COM0 or /dev/ttyUSB0)
3. Power up the doorbell using the 5V and GND pads on the back of the board
4. Run `esphome linp-doorbell.yaml run` (replacing "linp-doorbell.yaml" with whatever your YAML file is called, if you've called it something else)
5. Once the compilation is complete, you'll be prompted to choose how to perform the update; choose your serial-to-USB adapter
6. Once the flashing is complete, unplug the doorbell from the 5V power supply, remove the short from the corner pads to GND, then power it back up to boot normally
7. The ESP32 should start up and connect to your WiFi network as configured in the yaml file; you can then add it to your Home Assistant and start integrating it into your home automation!

## Home Assistant
Once you have your doorbell flashed with ESPHome and connected to Home Assistant, you should see the following services appear:

| Service name  | Description | Parameter 1 | Parameter 2 |
| ------------- | ----------- | ----------- | ----------- |
| `esphome.linp_doorbell_linp_set_volume` | Set the volume (`0` to mute) | `volume` \[int, 0-4]  |  |
| `esphome.linp_doorbell_linp_play_tune` | Play a tune/chime | `tune` \[int, 1-36]  |  |
| `esphome.linp_doorbell_linp_learn_button` | Register a new button, setting its tune/chime to the specified value | `tune` \[int, 1-36]  |  |
| `esphome.linp_doorbell_linp_set_tune` | Set the tune/chime for an already-registered button | `button` \[int, 1-10]  | `tune` \[int, 1-36] |
| `esphome.linp_doorbell_linp_forget_button` | Unregister a button from this unit | `button` \[int, 1-10]  |  |
| `esphome.linp_doorbell_linp_send_raw_command` | Enqueue a raw command to be sent to the STM8S005K6 | `command` \[string]  |  |

Note that the `linp_doorbell` prefix on all service names is the name of your ESPHome node, as defined in the `esphome:` block of your yaml file.

If these services don't appear in Home Assistant, try power cycling the doorbell so that it reconnects to Home Assistant.  I've found that the services don't seem to appear on first connect, but do on the second.
