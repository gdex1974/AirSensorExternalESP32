#include "DustMonitorController.h"

#include "TimeFunctions.h"
#include "AppConfig.h"

#include <Delays.h>
#include <PacketUart.h>
#include <PersistentStorage.h>
#include "AnalogPin.h"
#include "esp32-arduino/GpioPinDefinition.h"

#include <driver/rtc_io.h>

#include <Debug.h>

namespace
{
constexpr int millisecondsInMinute = 60000;
constexpr int bootEstimationMicroseconds = 700000;

constexpr float rawToVolts = 3.3f/4095;
constexpr std::string_view controllerDataTag = "DMC";

void initStepUpControl()
{
    const auto stepUpPin = (gpio_num_t)AppConfig::stepUpPin;
    rtc_gpio_init(stepUpPin); //initialize the RTC GPIO port
    rtc_gpio_set_direction(stepUpPin, RTC_GPIO_MODE_OUTPUT_ONLY); //set the port to output only mode
    rtc_gpio_hold_dis(stepUpPin); //disable hold before setting the level
}

void switchStepUpConversion(bool enable)
{
    const auto stepUpPin = (gpio_num_t)AppConfig::stepUpPin;
    rtc_gpio_set_level(stepUpPin, enable? 1 : 0);
}

void holdStepUpConversion()
{
    const auto stepUpPin = (gpio_num_t)AppConfig::stepUpPin;
    rtc_gpio_hold_en(stepUpPin);
}

bool isTimeSyncronized()
{
    return time(nullptr) > 1692025000;
}

tm getLocalTime(time_t time)
{
    tm info {};
    localtime_r(&time, &info);
    return info;
}

int readVoltageRaw()
{
    embedded::GpioPinDefinition voltagePin { AppConfig::voltagePin };
    const auto voltage = embedded::AnalogPin(voltagePin).singleRead();
    DEBUG_LOG("VoltageRaw is " << voltage);
    return voltage;
}
} // namespace

bool DustMonitorController::setup(bool wakeUp)
{
    initStepUpControl();
    if (!wakeUp)
    {
        switchStepUpConversion(true);
    }
    else
    {
        if (auto data = storage.get<ControllerData>(controllerDataTag))
        {
            controllerData = *data;
        }
    }
    auto pmResult = dustData.setup(wakeUp);
    if (!wakeUp)
    {
        if (pmResult)
        {
            const auto serial = dustData.getSpsSerial();
            memcpy(controllerData.sps30Serial, serial.begin(), serial.size());
        }
    }
    auto meteoResul = meteoData.setup(wakeUp);
    auto viewResult = transport.setup(controllerData.sps30Serial, wakeUp);
    return (meteoResul | pmResult)  && viewResult ;
}

uint32_t DustMonitorController::process()
{
    if (!isMeasured)
    {
        DEBUG_LOG("Measuring")
        if (meteoData.activate() && meteoData.doMeasure())
        {
            meteoData.hibernate();
        }

        if (controllerData.sps30Serial[0] != 0)
        {
            processSPS30Measurement();
        }
        isMeasured = true;
        needSend = true;
    }
    if (needSend)
    {
        needSend = false;
        transport.updateView({meteoData.getHumidity(), meteoData.getTemperature(), meteoData.getPressure(),
                             controllerData.pm01, controllerData.pm25, controllerData.pm10,
                             float(controllerData.voltageRaw) * rawToVolts / AppConfig::batteryVoltageDivider});
    }
    switch (transport.getStatus())
    {
        case EspNowTransport::SendStatus::Failed:
            if (++attemptsCounter > 10)
            {
                DEBUG_LOG("Too manu attempts, resetting")
                attemptsCounter = 0;
                return millisecondsInMinute;
            }
            else{
                DEBUG_LOG("Failed to send data, retry " << attemptsCounter)
                needSend = true;
            }
            return 100;
        case EspNowTransport::SendStatus::Requested:
        case EspNowTransport::SendStatus::Awaiting:
            return 1;
        case EspNowTransport::SendStatus::Completed:
            if (const auto correction = transport.getCorrection(); std::abs(correction) > 10000)
            {
                auto nowMicroseconds = microsecondsNow();
                nowMicroseconds += correction;
                const auto nowSeconds = static_cast<decltype(timeval::tv_sec)>(nowMicroseconds / microsecondsInSecond);
                timeval correctedTimeval {
                    .tv_sec = nowSeconds,
                    .tv_usec = static_cast<decltype(timeval::tv_usec)>(nowMicroseconds - nowSeconds * microsecondsInSecond)
                };
                settimeofday(&correctedTimeval, nullptr);
                DEBUG_LOG("Time is corrected by " << correction << " us")
            }
            else
            {
                DEBUG_LOG("Small correction factor " << correction << " us is skipped")
            }
            break;
        default:
            break;
    }

    auto nowMicroseconds = microsecondsNow();
    const int64_t wholeMinutePast = (nowMicroseconds/microsecondsInMinute)*microsecondsInMinute;
    const auto microsecondsTillNextMinute = wholeMinutePast + microsecondsInMinute - nowMicroseconds;
    const auto minimumDelay = (controllerData.sps30Status != SPS30Status::Measuring) ?
            microsecondsInSecond : 30 * microsecondsInSecond;
    uint32_t delayTime = 0;
    if (microsecondsTillNextMinute <= minimumDelay)
    {
        delayTime = microsecondsInMinute + microsecondsTillNextMinute - bootEstimationMicroseconds;
    }
    else
    {
        delayTime = microsecondsTillNextMinute - bootEstimationMicroseconds;
    }
    return static_cast<uint32_t>(delayTime/1000);
}

void DustMonitorController::processSPS30Measurement()
{
    bool shallStartMeasurement = (controllerData.wakeupCounter++) % 60 == 0;
    if (isTimeSyncronized() && controllerData.sps30Status != SPS30Status::Measuring)
    {
        shallStartMeasurement = getLocalTime(time(nullptr)).tm_min == 59;
    }
    if (shallStartMeasurement)
    {
        switchStepUpConversion(true);
        if (controllerData.sps30Status != SPS30Status::Measuring)
        {
            if (controllerData.sps30Status == SPS30Status::Sleep)
            {
                dustData.wakeUp();
            }
            dustData.startMeasure();
            controllerData.sps30Status = SPS30Status::Measuring;
            holdStepUpConversion();
        }
        controllerData.voltageRaw = readVoltageRaw();
    }
    else if (controllerData.sps30Status == SPS30Status::Measuring)
    {
        uint16_t p1, p25, p10;
        if (dustData.getMeasureData(p1, p25, p10))
        {
            controllerData.pm01 = p1;
            controllerData.pm25 = p25;
            controllerData.pm10 = p10;
        }
        dustData.stopMeasure();
        dustData.sleep();
        controllerData.sps30Status = SPS30Status::Sleep;
        controllerData.voltageRaw = readVoltageRaw();
        switchStepUpConversion(false);
    }
}

bool DustMonitorController::hibernate()
{
    dustData.hibernate();
    return storage.set(controllerDataTag, controllerData);
}
