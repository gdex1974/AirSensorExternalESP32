#include "DustMonitorController.h"

#include "TimeFunctions.h"
#include "AppConfig.h"

#include <PacketUart.h>
#include <Debug.h>
#include <Arduino.h>
#include <driver/rtc_io.h>
#include <esp32-hal-gpio.h>

namespace
{

constexpr int millisecondsInMinute = 60000;
constexpr int bootEstimationMilliseconds = 500;

enum class SPS30Status
{
    Startup,
    Sleep,
    Measuring,
};

RTC_DATA_ATTR SPS30Status sps30Status = SPS30Status::Startup;
RTC_DATA_ATTR int pm01{};
RTC_DATA_ATTR int pm25{};
RTC_DATA_ATTR int pm10{};
RTC_DATA_ATTR uint16_t voltageRaw = 0;
RTC_DATA_ATTR char sps30Serial[32];

const auto &voltagePin = AppConfig::VoltagePin;
constexpr float rawToVolts = 3.3f/4095;

int attemptsCounter = 0;

void initStepUpControl()
{
    const auto stepUpPin = (gpio_num_t)AppConfig::StepUpPin;
    rtc_gpio_init(stepUpPin); //initialize the RTC GPIO port
    rtc_gpio_set_direction(stepUpPin, RTC_GPIO_MODE_OUTPUT_ONLY); //set the port to output only mode
    rtc_gpio_hold_dis(stepUpPin); //disable hold before setting the level
}

void switchStepUpConversion(bool enable)
{
    const auto stepUpPin = (gpio_num_t)AppConfig::StepUpPin;
    rtc_gpio_set_level(stepUpPin, enable ? HIGH : LOW);
}

void holdStepUpConversion()
{
    const auto stepUpPin = (gpio_num_t)AppConfig::StepUpPin;
    rtc_gpio_hold_en(stepUpPin);
}

} // namespace

RTC_DATA_ATTR bool DustMonitorController::needPMMeasure = false;
RTC_DATA_ATTR uint32_t DustMonitorController::wakeupCounter = 0;

bool DustMonitorController::setup(bool wakeUp)
{
    initStepUpControl();
    if (!wakeUp)
    {
        switchStepUpConversion(true);
    }
    auto pmResult = dustData.setup(wakeUp);
    if (!wakeUp)
    {
        const auto serial = dustData.getSpsSerial();
        memset(sps30Serial, 0, sizeof(sps30Serial));
        memcpy(sps30Serial, serial.begin(), serial.size());
    }
    auto meteoResul = meteoData.setup(wakeUp);
    auto viewResult = view.setup(sps30Serial, wakeUp);
    return meteoResul | pmResult  |viewResult ;
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

        if (!needPMMeasure)
        {
            bool shallStartMeasurement = (wakeupCounter++) % 60 == 0;
            if (view.getCorrection() > 0 && sps30Status != SPS30Status::Measuring)
            {
                tm time {};
                if(getLocalTime(&time, 0) )
                {
                    shallStartMeasurement = time.tm_min == 59;
                }
            }
            if (shallStartMeasurement)
            {
                switchStepUpConversion(true);
                if (sps30Status != SPS30Status::Measuring)
                {
                    if (sps30Status == SPS30Status::Sleep)
                    {
                        dustData.wakeUp();
                    }
                    dustData.startMeasure();
                    sps30Status = SPS30Status::Measuring;
                    holdStepUpConversion();
                }
                voltageRaw = analogRead(voltagePin);
                needPMMeasure = true;
            }
        }
        else if (sps30Status == SPS30Status::Measuring)
        {
            int p1, p25, p10;
            if (dustData.getMeasureData(p1, p25, p10))
            {
                pm01 = p1;
                pm25 = p25;
                pm10 = p10;
            }
            dustData.hibernate();
            sps30Status = SPS30Status::Sleep;
            needPMMeasure = false;
            voltageRaw = analogRead(voltagePin);
            switchStepUpConversion(false);
        }
        isMeasured = true;
        needSend = true;
    }
    if (needSend)
    {
        needSend = false;
        view.updateView(meteoData.getHumidity(), meteoData.getTemperature(), meteoData.getPressure(), pm01, pm25,
                        pm10, float(voltageRaw)*rawToVolts/AppConfig::BatteryVoltageDivider);
    }
    switch (view.getStatus())
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
            if (const auto correction = view.getCorrection(); std::abs(correction) > 10000)
            {
                auto nowMicroseconds = microsecondsNow();
                nowMicroseconds += correction;
                const auto nowSeconds = static_cast<decltype(timeval::tv_sec)>(nowMicroseconds / microsecondsInSecond);
                timeval correctedTimeval {
                    .tv_sec = nowSeconds,
                    .tv_usec = static_cast<decltype(timeval::tv_usec)>(nowMicroseconds - nowSeconds * microsecondsInSecond)
                };
                timezone utcTimezone {0, 0};
                settimeofday(&correctedTimeval, &utcTimezone);
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
    const int64_t nextWholeMinute = wholeMinutePast + microsecondsInMinute;
    const auto microsecondsTillNextMinute = nextWholeMinute - nowMicroseconds;
    const auto minimumDelay = (sps30Status != SPS30Status::Measuring) ? microsecondsInSecond : 30 * microsecondsInSecond;
    auto delay = (microsecondsTillNextMinute / 1000 - bootEstimationMilliseconds);
    if (microsecondsTillNextMinute <= minimumDelay)
    {
        delay += millisecondsInMinute;
    }
    return static_cast<uint32_t>(delay);
}
