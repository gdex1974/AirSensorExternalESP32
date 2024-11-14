#pragma once

#include "PTHProvider.h"
#include "EspNowTransport.h"
#include "SPS30DataProvider.h"

#include <esp_attr.h>
#include <cstdint>
#include <ctime>

class DustMonitorController {
public:
    enum class ResetReason
    {
        PowerOn,
        DeepSleep,
        BrownOut,
    };
    DustMonitorController(embedded::PersistentStorage& storage, embedded::PacketUart& uart, embedded::I2CHelper& i2CHelper, bool restrictTxPower)
    : storage(storage)
    , meteoData(storage, i2CHelper)
    , dustData(storage, uart)
    , transport(storage, restrictTxPower)
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
        int16_t pm01 = -1;
        int16_t pm25 = -1;
        int16_t pm10 = -1;
        uint16_t voltageRaw = 0;
        char sps30Serial[32] = {};
        time_t lastPMMeasureStarted = 0;
        time_t firstSyncTime = 0;
        bool insufficientPower = false;
    } controllerData;
    embedded::PersistentStorage& storage;
    PTHProvider meteoData;
    SPS30DataProvider dustData;
    EspNowTransport transport;
    bool needSend = false;
    bool sensorPresent = false;
};
