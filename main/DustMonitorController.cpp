#include "DustMonitorController.h"

#include "TimeFunctions.h"
#include "AppConfig.h"

#include <PacketUart.h>
#include <PersistentStorage.h>
#include "AnalogPin.h"

#include "esp32-esp-idf/GpioPinDefinition.h"
#include "Delays.h"

#include <driver/rtc_io.h>

#include <Debug.h>

namespace
{
constexpr int bootEstimationMicroseconds = 700000;
constexpr int sps30MeasurementDuration = 30; //seconds

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
    if (enable)
    {
        embedded::delay(20);
    }
}

void holdStepUpConversion()
{
    const auto stepUpPin = (gpio_num_t)AppConfig::stepUpPin;
    rtc_gpio_hold_en(stepUpPin);
}

bool isTimeSyncronized(time_t time)
{
    return time > 1692025000;
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
    DEBUG_LOG("VoltageRaw is " << voltage)
    return voltage;
}

void correctTime(const int64_t correction)
{
    if (std::abs(correction) > 10000)
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
    sensorPresent = dustData.setup(wakeUp);
    if (!wakeUp)
    {
        controllerData.voltageRaw = readVoltageRaw();
        if (sensorPresent)
        {
            const auto serial = dustData.getSpsSerial();
            memcpy(controllerData.sps30Serial, serial.begin(), serial.size());
            dustData.sleep();
            switchStepUpConversion(false);
        }
    }
    auto meteoResul = meteoData.setup(wakeUp);
    auto viewResult = transport.setup(controllerData.sps30Serial, wakeUp);
    return (meteoResul | sensorPresent)  && viewResult ;
}

uint32_t DustMonitorController::process()
{
    const auto currentTime = time(nullptr);
    const bool isTimeGood = isTimeSyncronized(currentTime);
    if (isTimeGood && sensorPresent)
    {
        processSPS30Measurement();
    }

    if (transport.getStatus() == EspNowTransport::SendStatus::Idle) // no send attempts after boot
    {
        needSend = !isTimeGood || getLocalTime(currentTime).tm_sec == 59;
    }
    if (needSend)
    {
        needSend = false;
        if (meteoData.activate() && meteoData.doMeasure())
        {
            meteoData.hibernate();
        }
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
                break;
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
            correctTime(transport.getCorrection());
            break;
        default:
            break;
    }

    auto nowMicroseconds = microsecondsNow();
    const int64_t wholeMinutePast = (nowMicroseconds / microsecondsInMinute) * microsecondsInMinute;
    const auto microsecondsTillNextMinute = wholeMinutePast + microsecondsInMinute - nowMicroseconds;
    uint32_t delayTime;
    if (microsecondsTillNextMinute <= bootEstimationMicroseconds)
    {
        delayTime = microsecondsInMinute + microsecondsTillNextMinute - bootEstimationMicroseconds;
    }
    else
    {
        delayTime = microsecondsTillNextMinute - bootEstimationMicroseconds;
    }
    if (controllerData.sps30Status == SPS30Status::Measuring)
    {
        delayTime = std::min(delayTime, uint32_t(sps30MeasurementDuration * microsecondsInSecond));
    }
    return static_cast<uint32_t>(delayTime/1000);
}

void DustMonitorController::processSPS30Measurement()
{
    if (controllerData.sps30Status == SPS30Status::Measuring
        && time(nullptr) > controllerData.lastPMMeasureStarted + sps30MeasurementDuration)
    {
        uint16_t p1, p25, p10;
        if (dustData.getMeasureData(p1, p25, p10))
        {
            controllerData.pm01 = p1;
            controllerData.pm25 = p25;
            controllerData.pm10 = p10;
        }
        else
        {
            DEBUG_LOG("Failed to obtain PMx data")
            controllerData.pm01 = -1;
            controllerData.pm10 = -1;
            controllerData.pm25 = -1;
        }
        dustData.stopMeasure();
        dustData.sleep();
        controllerData.sps30Status = SPS30Status::Sleep;
        controllerData.voltageRaw = readVoltageRaw();
        switchStepUpConversion(false);
        DEBUG_LOG("PM Measurement finished")
    }
    else
    {
        const auto currentTime = time(nullptr);
        const bool shallStartMeasurement = (controllerData.lastPMMeasureStarted == 0) ||
                (currentTime - controllerData.lastPMMeasureStarted > 10*60
                    && getLocalTime(currentTime).tm_min == 59);
        if (shallStartMeasurement)
        {
            DEBUG_LOG("Starting PM measurement")
            switchStepUpConversion(true);
            dustData.startMeasure();
            controllerData.sps30Status = SPS30Status::Measuring;
            holdStepUpConversion();
            controllerData.voltageRaw = readVoltageRaw();
            controllerData.lastPMMeasureStarted = time(nullptr);
        }
    }
}

bool DustMonitorController::hibernate()
{
    dustData.hibernate();
    return storage.set(controllerDataTag, controllerData);
}
