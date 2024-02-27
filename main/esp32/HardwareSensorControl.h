#pragma once

#include "AppConfig.h"
#include "Delays.h"

#include <hal/gpio_types.h>
#include <driver/rtc_io.h>

namespace HardwareSensorControl
{

constexpr int bootEstimationMicroseconds = 700000;

inline void initStepUpControl(bool set)
{
    (void)set;// this is not necessary for ESP32
    const auto stepUpPin = (gpio_num_t)AppConfig::stepUpPin;
    rtc_gpio_init(stepUpPin); //initialize the RTC GPIO port
    rtc_gpio_set_direction(stepUpPin, RTC_GPIO_MODE_OUTPUT_ONLY); //set the port to output only mode
    rtc_gpio_hold_dis(stepUpPin); //disable hold before setting the level
}

inline void switchStepUpConversion(bool enable)
{
    const auto stepUpPin = (gpio_num_t)AppConfig::stepUpPin;
    rtc_gpio_set_level(stepUpPin, enable ? 1 : 0);
    if (enable)
    {
        embedded::delay(20);
    }
}

inline void holdStepUpConversion()
{
    const auto stepUpPin = (gpio_num_t)AppConfig::stepUpPin;
    rtc_gpio_hold_en(stepUpPin);
}

inline void activateDeepSleepGpioHold()
{
}

}