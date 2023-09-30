#pragma once

#include "SPS30/Sps30Uart.h"

namespace embedded
{
class PersistentStorage;
}

class SPS30DataProvider
{
public:
    SPS30DataProvider(embedded::PersistentStorage &storage, embedded::PacketUart& packetUart)
    : sps30(packetUart), storage(storage)
    {
    }

    bool setup(bool wakeUp);
    bool startMeasure();
    bool stopMeasure()
    {
        return data.sensorPresent && sps30.stopMeasurement() == embedded::Sps30Error::Success;
    }

    bool wakeUp()
    {
        return data.firmwareMajorVersion > 1 && sps30.wakeUp() == embedded::Sps30Error::Success;
    }

    bool sleep()
    {
        return data.firmwareMajorVersion > 1 && sps30.sleep() == embedded::Sps30Error::Success;
    }

    bool activate()
    {
        return sps30.activateTransport() == embedded::Sps30Error::Success;
    }

    bool hibernate();
    bool getMeasureData(uint16_t &pm1, uint16_t &pm25, uint16_t &pm10);
    std::string_view getSpsSerial() const { return data.serialNumber.serial; }
private:
    struct Data
    {
        uint32_t measurementsCounter = 0;
        embedded::Sps30SerialNumber serialNumber {};
        int16_t firmwareMajorVersion;
        bool sensorPresent = false;
    } data;
    embedded::Sps30Uart sps30;
    embedded::PersistentStorage &storage;
};
