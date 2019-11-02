# ESPHome custom component for linp-doorbell-g03

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

## Flashing the ESP32
In order to boot the ESP32 into flash mode (so that you can write ESPHome to it), you'll need to pull the two pins closest the bottom right corner of the ESP32 module to ground; I did this by soldering fine wires to the two pins and one of the ground pads (either from the test points listed above, or the second from the top on the "P3" test points to the left of the ESP32), but if you have an easier way to reliably short the two corner pads to ground temporarily, anything which is easy to undo should work fine.

1. Connect GND, TXD0, RXD0 to a serial-to-USB adapter (making sure to connect the ESP's RXD0 to your adapter's TX (or TXD) pin and the ESP's TXD0 to your adapter's RX (or RXD) pin)
2. Connect your serial-to-USB adapter to your computer, making note of the port which it shows up on (depending on your OS, this could be something along the lines of COM0 or /dev/ttyUSB0)
3. Power up the doorbell using the 5V and GND pads on the back of the board
4. Run `esphome linp-doorbell.yaml run` (replacing "linp-doorbell.yaml" with whatever your YAML file is called, if you've called it something else)
5. Once the compilation is complete, you'll be prompted to choose how to perform the update; choose your serial-to-USB adapter
6. Once the flashing is complete, unplug the doorbell from the 5V power supply, remove the short from the corner pads to GND, then power it back up to boot normally
7. The ESP32 should start up and connect to your WiFi network as configured in the yaml file; you can then add it to your Home Assistant and start integrating it into your home automation!
