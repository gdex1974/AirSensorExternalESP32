#pragma once

#include "SPS30/Sps30Uart.h"

class SPS30DataProvider
{
public:
    explicit SPS30DataProvider(embedded::PacketUart& packetUart)
    : sps30(packetUart)
    {
    }

    bool setup(bool wakeUp);
    bool startMeasure();
    bool wakeUp();
    bool hibernate();
    bool getMeasureData(int& pm1, int& pm25, int& pm10);
    std::string_view getSpsSerial() const { return serialNumber.serial; }
private:
    embedded::Sps30Uart sps30;
    embedded::Sps30SerialNumber serialNumber {};
};
