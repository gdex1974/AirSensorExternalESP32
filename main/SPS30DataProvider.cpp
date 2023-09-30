#include "SPS30DataProvider.h"

#include "PersistentStorage.h"
#include "Debug.h"

namespace
{
    constexpr std::string_view sps30DataKey = "SPSD";
}

using embedded::Sps30Error;

bool SPS30DataProvider::setup(bool wakeUp)
{
    if (wakeUp)
    {
        if (const auto storedData = storage.get<Data>(sps30DataKey); storedData)
        {
            data = *storedData;
            DEBUG_LOG("Restored SPS30 data: " << (data.sensorPresent ? data.serialNumber.serial : "sensor not found"))
            return data.sensorPresent;
        }
        DEBUG_LOG("SPS30 data is not found")
    }
    DEBUG_LOG("Probing SPS30")
    auto spsInitResult = sps30.probe();
    if (spsInitResult == Sps30Error::Success)
    {
        spsInitResult = sps30.getSerial(data.serialNumber);
        DEBUG_LOG("Serial number: " << data.serialNumber.serial)
        sps30.resetSensor();
        auto versionResult = sps30.getVersion();
        if (std::holds_alternative<embedded::Sps30VersionInformation>(versionResult))
        {
            const auto& sps30Version = std::get<embedded::Sps30VersionInformation>(versionResult);
            DEBUG_LOG("Firmware revision: " << (int) sps30Version.firmware_major << "." << (int) sps30Version.firmware_minor)
            data.firmwareMajorVersion = sps30Version.firmware_major;
            if (sps30Version.shdlc)
            {
                DEBUG_LOG("Hardware revision: " << (int)sps30Version.shdlc->hardware_revision)
                DEBUG_LOG("SHDLC revision: " << (int)sps30Version.shdlc->shdlc_major << "." << (int)sps30Version.shdlc->shdlc_minor)
            }
        } else
        {
            DEBUG_LOG("Firmware revision reading failed with code: " << (int) spsInitResult)
        }
    }
    else
    {
        DEBUG_LOG("Probe is failed with code: " << (int)spsInitResult)
    }
    data.sensorPresent = spsInitResult == Sps30Error::Success;
    storage.set(sps30DataKey, data);
    return data.sensorPresent;
}

bool SPS30DataProvider::startMeasure()
{
    if (!data.sensorPresent)
    {
        return false;
    }
    auto result =  sps30.startMeasurement(true) == Sps30Error::Success;
    if (result && (data.measurementsCounter++ % 168 == 0))
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

bool SPS30DataProvider::hibernate()
{
    storage.set(sps30DataKey, data);
    DEBUG_LOG("SPS30 data saved")
    return true;
}

bool SPS30DataProvider::getMeasureData(uint16_t &pm1, uint16_t &pm25, uint16_t &pm10)
{
    if (!data.sensorPresent)
    {
        return false;
    }
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
