#pragma once

#include <array>
#include <cstdint>

struct AppConfig
{
    static const std::array<const uint8_t, 6> MacAddress;
    static const uint8_t BME280Address;
    // Serial1 is used for debug output
    static const int8_t Serial1RxPin;
    static const int8_t Serial1TxPin;
    // Serial2 is used for SPS30 communication
    static const int8_t Serial2RxPin;
    static const int8_t Serial2TxPin;
    static const int8_t SDA;
    static const int8_t SCL;
    static const int8_t StepUpPin;
    static const int8_t VoltagePin;
    static const float BatteryVoltageDivider;
};