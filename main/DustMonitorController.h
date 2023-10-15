#pragma once

#include "PTHProvider.h"
#include "EspNowTransport.h"
#include "SPS30DataProvider.h"

#include <esp_attr.h>
#include <cstdint>

class DustMonitorController {
public:
    enum class ResetReason
    {
        PowerOn,
        DeepSleep,
        BrownOut,
    };
    DustMonitorController(embedded::PersistentStorage& storage, embedded::PacketUart& uart, embedded::I2CHelper& i2CHelper)
    : storage(storage)
    , meteoData(storage, i2CHelper)
    , dustData(storage, uart)
    {}

    bool setup(ResetReason resetReason);
    uint32_t process();
    bool hibernate();

private:
    void processSPS30Measurement();


    enum class SPS30Status
    {
        Startup,
        Sleep,
        Measuring,
    };

    struct ControllerData
    {
        SPS30Status sps30Status = SPS30Status::Startup;
        int pm01 = -1;
        int pm25 = -1;
        int pm10 = -1;
        uint16_t voltageRaw = 0;
        char sps30Serial[32] = {};
        time_t lastPMMeasureStarted = 0;
        bool insufficientPower = false;
    } controllerData;
    embedded::PersistentStorage& storage;
    PTHProvider meteoData;
    SPS30DataProvider dustData;
    EspNowTransport transport;
    int attemptsCounter = 0;
    bool isMeasured = false;
    bool needSend = false;
    bool sensorPresent = false;
};
