#include "SPS30DataProvider.h"
#include "Debug.h"

#include <esp_attr.h>

namespace
{
    RTC_DATA_ATTR uint32_t measurementsCounter;
}

using embedded::Sps30Error;

bool SPS30DataProvider::setup(bool wakeUp)
{
    if (wakeUp)
    {
        return true;
    }
    measurementsCounter = 0;
    DEBUG_LOG("Probing SPS30")
    auto spsInitResult = sps30.probe();
    if (spsInitResult == Sps30Error::Success)
    {
        spsInitResult = sps30.getSerial(serialNumber);
        DEBUG_LOG("Serial number: " << serialNumber.serial)
        sps30.resetSensor();
        auto versionResult = sps30.getVersion();
        if (std::holds_alternative<embedded::Sps30VersionInformation>(versionResult))
        {
            const auto& sps30Version = std::get<embedded::Sps30VersionInformation>(versionResult);
            DEBUG_LOG("Firmware revision: " << (int) sps30Version.firmware_major << "." << (int) sps30Version.firmware_minor)
            if (sps30Version.shdlc)
            {
                DEBUG_LOG("Hardware revision: " << (int)sps30Version.shdlc->hardware_revision)
                DEBUG_LOG("SHDLC revision: " << (int)sps30Version.shdlc->shdlc_major << "." << (int)sps30Version.shdlc->shdlc_minor)
            }
        } else
        {
            DEBUG_LOG("Firmware revision reading failed with code: " << (int) spsInitResult)
        }
        auto cleaningInterval = sps30.getFanAutoCleaningInterval();
        if (std::holds_alternative<uint32_t>(cleaningInterval))
        {
            DEBUG_LOG("Cleaning interval: " << (int) std::get<uint32_t>(cleaningInterval))
        }
        else
        {
            DEBUG_LOG("Fan cleaning interval reading error: " << (int) std::get<Sps30Error>(cleaningInterval))
        }
    }
    else
    {
        DEBUG_LOG("Probe is failed with code: " << (int)spsInitResult)
    }
    return spsInitResult == Sps30Error::Success;
}

bool SPS30DataProvider::startMeasure()
{
    auto result =  sps30.startMeasurement(true) == Sps30Error::Success;
    if (result && (measurementsCounter++ % 168 == 0))
    {
        if (auto cleaningResult = sps30.startManualFanCleaning(); cleaningResult == Sps30Error::Success)
        {
            DEBUG_LOG("Manual cleaning started")
        }
        else
        {
            DEBUG_LOG("Manual cleaning start failed with the code " << (int)cleaningResult)
        }
    }
    return result;
}

bool SPS30DataProvider::wakeUp()
{
    return sps30.wakeUp() == Sps30Error::Success;
}

bool SPS30DataProvider::hibernate()
{
    sps30.stopMeasurement();
    return sps30.sleep() == Sps30Error::Success;
}

bool SPS30DataProvider::getMeasureData(int &pm1, int &pm25, int &pm10)
{
    const auto result = sps30.readMeasurement();
    if (std::holds_alternative<embedded::Sps30MeasurementData>(result))
    {
        const auto& measurementData = std::get<embedded::Sps30MeasurementData>(result);
        if (measurementData.measureInFloat)
        {
            pm1 = (int)measurementData.floatData.mc_1p0;
            pm25 = (int)measurementData.floatData.mc_2p5;
            pm10 = (int)measurementData.floatData.mc_10p0;
            return true;
        }
        else
        {
            pm1 = measurementData.unsignedData.mc_1p0;
            pm25 = measurementData.unsignedData.mc_2p5;
            pm10 = measurementData.unsignedData.mc_10p0;
        }
        return true;
    }
    return false;
}
