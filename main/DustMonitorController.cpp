#include "DustMonitorController.h"
#include "HardwareSensorControl.h"

#include "TimeFunctions.h"
#include "AppConfig.h"

#include <PacketUart.h>
#include <PersistentStorage.h>
#include "AnalogPin.h"

#include "esp32-esp-idf/GpioPinDefinition.h"

#include <Debug.h>

namespace
{
constexpr int sps30MeasurementDuration = 30; //seconds
constexpr int insufficientPowerThreshold = 50 * 60; //seconds

constexpr float rawToVolts = 3.3f/4095;
constexpr std::string_view controllerDataTag = "DMC";

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

enum class SensorFlags : uint32_t {
    BatteryFailure = 1 << 0,
};
} // namespace

bool DustMonitorController::setup(ResetReason resetReason)
{
    bool wakeUp = resetReason == ResetReason::DeepSleep;
    if (!wakeUp)
    {
        HardwareSensorControl::initStepUpControl(false);
        controllerData.insufficientPower = (resetReason == ResetReason::BrownOut);
        HardwareSensorControl::activateDeepSleepGpioHold();
        HardwareSensorControl::switchStepUpConversion(true);
    }
    else
    {
        if (auto data = storage.get<ControllerData>(controllerDataTag))
        {
            controllerData = *data;
        }
        HardwareSensorControl::initStepUpControl(SPS30Status::Measuring == controllerData.sps30Status);
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
            HardwareSensorControl::switchStepUpConversion(false);
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
    if (isTimeGood)
    {
        if (controllerData.firstSyncTime == 0)
        {
            controllerData.firstSyncTime = currentTime;
        }
        if (sensorPresent)
        {
            if (!controllerData.insufficientPower)
            {
                processSPS30Measurement();
            }
            else
            {
                HardwareSensorControl::switchStepUpConversion(false);
                if (currentTime - controllerData.firstSyncTime > insufficientPowerThreshold)
                {
                    controllerData.insufficientPower = false;
                }
            }
        }
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
                             float(controllerData.voltageRaw) * rawToVolts / AppConfig::batteryVoltageDivider,
                                     (controllerData.insufficientPower ? (uint32_t)SensorFlags::BatteryFailure : 0)});
    }
    switch (transport.getStatus())
    {
        case EspNowTransport::SendStatus::Failed:
            break;
        case EspNowTransport::SendStatus::Requested:
        case EspNowTransport::SendStatus::Awaiting:
            if (microsecondsNow() - transport.getLastPacketTimestamp() < 1000000)
            {
                return 1;
            }
            DEBUG_LOG("Internal unit didn't respond.")
            break;
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
    if (microsecondsTillNextMinute <= HardwareSensorControl::bootEstimationMicroseconds)
    {
        delayTime = microsecondsInMinute + microsecondsTillNextMinute - HardwareSensorControl::bootEstimationMicroseconds;
    }
    else
    {
        delayTime = microsecondsTillNextMinute - HardwareSensorControl::bootEstimationMicroseconds;
    }
    if (controllerData.sps30Status == SPS30Status::Measuring)
    {
        delayTime = std::min(delayTime, uint32_t(sps30MeasurementDuration * microsecondsInSecond));
    }
    return static_cast<uint32_t>(delayTime/1000);
}

void DustMonitorController::processSPS30Measurement()
{
    if (controllerData.sps30Status == SPS30Status::Measuring)
    {
        if (const auto timestamp = time(nullptr); timestamp >=
                                                  controllerData.lastPMMeasureStarted + sps30MeasurementDuration)
        {
            if (uint16_t p1, p25, p10; dustData.getMeasureData(p1, p25, p10))
            {
                controllerData.pm01 = static_cast<int16_t>(p1);
                controllerData.pm25 = static_cast<int16_t>(p25);
                controllerData.pm10 = static_cast<int16_t>(p10);
                dustData.stopMeasure();
            }
            else
            {
                DEBUG_LOG("Failed to obtain PMx data")
                controllerData.pm01 = -1;
                controllerData.pm10 = -1;
                controllerData.pm25 = -1;
            }
            dustData.sleep();
            controllerData.sps30Status = SPS30Status::Sleep;
            controllerData.voltageRaw = readVoltageRaw();
            HardwareSensorControl::switchStepUpConversion(false);
            DEBUG_LOG("PM Measurement finished")
        }
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
            HardwareSensorControl::switchStepUpConversion(true);
            dustData.startMeasure();
            controllerData.sps30Status = SPS30Status::Measuring;
            HardwareSensorControl::holdStepUpConversion();
            controllerData.voltageRaw = readVoltageRaw();
            controllerData.lastPMMeasureStarted = time(nullptr);
        }
    }
}

bool DustMonitorController::hibernate()
{
    dustData.hibernate();
    transport.hibernate();
    return storage.set(controllerDataTag, controllerData);
}
