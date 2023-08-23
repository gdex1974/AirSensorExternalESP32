# Air Quality Sensor - external unit

## Description

This is a part of the air quality sensor project. The external unit is placed outside and measures the air quality. It's powered by 3xAA batteries and sends the data to the internal unit via Esp-Now transport protocol.
This unit is based on Lolin32-lite board, with the firmware based on ESP-IDF framework.

## Hardware

- [Lolin32-lite](https://templates.blakadder.com/wemos_LOLIN32_Lite_v1.html) main microcontroller board
- [SPS30](https://sensirion.com/products/catalog/SPS30/) particulate matter sensor
- [BME280](https://www.bosch-sensortec.com/bst/products/all_products/bme280) temperature, humidity and pressure sensor
- [Pololu 2810](https://www.pololu.com/product/2810) low voltage module switch
- [HW-105 5V step-up module](https://www.aliexpress.com/item/33025722716.html) 5V step-up module
- [3xAA battery holder](https://www.aliexpress.com/item/4001008150456.html)

## Software

- [esp-idf](https://github.com/espressif/esp-idf/releases/tag/v4.4.5) main framework

Currently, ESP-IDF 4.4.5 build system overrides C/C++ standard using `-std=gnu99` and `-std=gnu++11` flags.
It happens into estp-idf/tools/cmake/build.cmake file. To avoid this, the following lines should be commented out:

`list(APPEND c_compile_options   "-std=gnu99")`

`list(APPEND cpp_compile_options "-std=gnu++11")`

The patch file `fix_esp_idf_build.patch` is provided in the repository.

## Setup

All parts of the unit are fixed on frame and covered by external cover. The frame is designed to fit the Lolin32-lite board, switch, step-up converter, BME280 and SPS30 sensors. It's attached to battery case and the whole module is placed into the external cover. The fame and cover parts are 3D printed.
Connection schema:
- SPS30 sensor is connected to the UART2 port of the Lolin32-lite board.
  - `VCC` pin of the sensor is connected to the +5V output of the step-up converter
  - `RX` pin of the sensor is connected to the `IO27` pin of the board
  - `TX` pin of the sensor is connected to the `IO26` pin of the board
  - `SEL` pin of the sensor is not connected
  - `GND` pin of the sensor is connected to the `GND` pin of the board
- BME280 module is connected to the I2C port of the Lolin32-lite board
  - `SDA` pin of the sensor is connected to the `IO33` pin of the board
  - `SCL` pin of the sensor is connected to the `IO32` pin of the board
  - `GND` pin of the sensor is connected to the `GND` pin of the board
  - `VCC` pin of the sensor is connected to the `3V3` pin of the board
- Pololu 2810 switch is mounted _under_ the main board and handled by pass-through pins:
  - `GND` connected to the GND pin of the board
  - `Enable` pin connected to the IO12 pin of the Lolin32-lite board (next to the GND pin)
  - `VIN` pin connected to the battery+ of the Lolin32-lite board
  - `VOUT` pin connected to the + input of the step-up converter.  Addtionally, it's connected to voltage divider (R1=220k, R2=330k) and the output of the divider is connected to the `IO2` pin of the board. This pin is used to measure the battery voltage.
- step-up converter's is fixed into separated place on the frame
  - `+` input is connected to the VBAT pin of the Lolin32-lite board
  - `-` input is connected to the GND pin of the Lolin32-lite board
  - `+` output is connected to the VCC pin of the SPS30 sensor.

## Firmware

The firmware is based on ESP-IDF framework (tested version is 4.4.5) and is written in C++ language. The main task is to read the data from sensors and send them to the internal unit via Esp-Now protocol. The firmware is divided into several modules:

- components - contains the code of the external unit
  - general-support library - contains the code for general support functions, obtained from this repository.
  - SimpleDrivers - contains the code for the SPS30 and BME280 drivers, obtained from this repository.
- main - contains the main code of the external unit's firmware
  - AppConfig - contains the code for the application's configuration
  - ArduinoStubs - contains setup() and loop() functions, required by the arduino framework
  - EspNowTransport - contains the code for the communication with the main unit based on Esp-Now protocol
  - DustMonitorController - contains the code for the controller class handling the main logic of the firmware
  - PTHProvider - contains the code for the class providing the data from BME280 sensor
  - SPS30DataProvider - contains the code for the class providing the data from SPS30 sensor
- CMakeLists.txt - main CMake file for the firmware
- sdkconfig - default configuration file for the ESP-IDF framework.
