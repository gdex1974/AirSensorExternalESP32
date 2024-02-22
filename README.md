# Air Quality Sensor - external unit

## Description

The Air Quality Sensor project contains two parts:
- [Internal unit](https://github.com/gdex1974/AirSensorInternalESP32) - the main unit placed inside the building
- External unit - the unit placed outside the building
This repository contains the hardware and software definitions for the external unit.
In the current state it's based on ESP32C3 module from DFRobot and communicate with the internal unit by ESP-NOW protocol.
The temperature, humdity and pressure are measured each minute. The PMx values are measured each hour.

## Hardware

- [Beetle ESP32-C3](https://wiki.dfrobot.com/SKU_DFR0868_Beetle_ESP32_C3) main microcontroller board
- [SPS30](https://botland.store/air-quality-sensors/15062-dust-air-sensor-sps30-pm10-pm25-pm4--5904422366094.html) sensor for particles matter measurement
- [BME280](https://botland.store/pressure-sensors/11803-bme280-humidity-temperature-and-pressure-5904422366179.html) sensor for temperature, humidity and pressure measurement
- [Pololu Mini MOSFET Switch](https://www.pololu.com/product/2810) low voltage module switch
- [Pololu U1V10F5](https://www.pololu.com/product/2564) 5V step-up module
- [Module with microUSB socket](https://botland.store/protoboard-connector-board-accessories/7668-module-with-microusb-socket-5904422300678.html) 5V power module
- [Li-Pol Akyga 1950mAh 1S 3,7V](https://www.akyga.com/products/344-lithium-polymer-battery-3-7v-1950mah-pcm-wires-l-150mm.html) Li-Pol accumulator
- [Prototype board](https://botland.store/circuit-boards/2715-universal-double-sided-board-30x70mm-5904422303280.html) used to fit most of the components

Alternalively, the ESP32-based compact boards like Lolin32-Lite could be used, the possible parts and schematics are described [here](docs/Lolin32-lite/Schematics.md)
## Software

- [esp-idf](https://github.com/espressif/esp-idf/releases/tag/v4.4.5) main framework
- [general-support-library](https://github.com/gdex1974/embedded-general-support-library) common embedded library
- [SimpleDrivers](https://github.com/gdex1974/embedded-simple-drivers) simple embedded drivers for SPS30 and BME280 sensors

## Setup

The microcontroller, switch, step-up converter and BME-280 sensor are placed on the prototype board.
It has to be cut to fit the size of the external cover. The SPS30 sensor and micro-USB socket module are placed aside and connected to the main board by the wires.
The frame and cover parts are 3D printed.

### Connection schema:

[Beetle C3 based schema](docs/BeetleC3/Schematics.md)

[Lolin32-lite based schema](docs/Lolin32-lite/Schematics.md)

## Firmware

The firmware is based on ESP-IDF framework (tested version is 4.4.6) and is written in C++17 language. The main task is to read the data from sensors and send them to the internal unit via Esp-Now protocol. The firmware is divided into several modules:

- components - contains the code of the external unit
  - general-support library - contains the code for general support functions, obtained from this repository.
  - SimpleDrivers - contains the code for the SPS30 and BME280 drivers, obtained from this repository.
- main - contains the main code of the external unit's firmware
  - AppConfig - contains the code for the application's configuration
  - AppMain - contains the app_main() function and hosts the controller object.
  - EspNowTransport - contains the code for the communication with the main unit based on Esp-Now protocol
  - DustMonitorController - contains the code for the controller class handling the main logic of the firmware
  - PTHProvider - contains the code for the class providing the data from BME280 sensor
  - SPS30DataProvider - contains the code for the class providing the data from SPS30 sensor
- CMakeLists.txt - main CMake file for the firmware
- sdkconfig - default configuration file for the ESP-IDF framework.

## Build

The main CMakeLists.txt file could be opened directly in IDE.
To build manually, do the following steps:

```shell
mkdir build
cd build
cmake -DIDF_TARGET=esp32c3 -G Ninja  ..
cd ..
idf.py build
```
