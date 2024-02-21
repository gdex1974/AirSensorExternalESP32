#pragma once
#include "../HardwareSensorControl.h"
#include "AppConfig.h"

#include "Delays.h"
#include "GpioDigitalPin.h"
#include "esp32-esp-idf/GpioPinDefinition.h"
#include <hal/gpio_types.h>
#include <driver/gpio.h>

namespace HardwareSensorControl
{

constexpr int bootEstimationMicroseconds = 500000;

inline void initStepUpControl(bool set)
{
    const auto stepUpPin = (gpio_num_t)AppConfig::stepUpPin;
    embedded::GpioPinDefinition stepUpPinDefinition { static_cast<uint8_t>(stepUpPin) };
    embedded::GpioDigitalPin(stepUpPinDefinition).init(embedded::GpioDigitalPin::Direction::Output);
    embedded::GpioDigitalPin(stepUpPinDefinition).set(set);
    gpio_hold_dis(stepUpPin);
}

inline void switchStepUpConversion(bool enable)
{
    embedded::GpioPinDefinition stepUpPinDefinition { static_cast<uint8_t>(AppConfig::stepUpPin) };
    if (enable)
    {
        gpio_deep_sleep_hold_en();
    }
    embedded::GpioDigitalPin(stepUpPinDefinition).set(enable);
    if (enable)
    {
        embedded::delay(20);
    }
}

inline void holdStepUpConversion()
{
    const auto stepUpPin = (gpio_num_t)AppConfig::stepUpPin;
    gpio_hold_en(stepUpPin);
}

inline void activateDeepSleepGpioHold()
{
    gpio_deep_sleep_hold_en();
}

}