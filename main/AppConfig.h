#pragma once

#include <array>
#include <cstdint>

struct AppConfig
{
    static const std::array<const uint8_t, 6> macAddress;
    static const uint8_t bme280Address;
    // Serial1 is used for debug output
    static const int8_t serial1RxPin;
    static const int8_t serial1TxPin;
    // Serial2 is used for SPS30 communication
    static const int8_t serial2RxPin;
    static const int8_t serial2TxPin;
    static const int8_t SDA;
    static const int8_t SCL;
    static const int8_t stepUpPin;
    static const int8_t voltagePin;
    static const float batteryVoltageDivider;
};