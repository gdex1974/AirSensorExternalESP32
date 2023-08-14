#pragma once

#include "PTHProvider.h"
#include "EspNowTransport.h"
#include "SPS30DataProvider.h"

#include <esp_attr.h>
#include <cstdint>

class DustMonitorController {
public:
    explicit DustMonitorController(embedded::PacketUart& uart, embedded::I2CHelper& i2CHelper)
    : meteoData(i2CHelper)
    , dustData(uart)
    {}

    bool setup(bool wakeUp);
    uint32_t process();

private:
    PTHProvider meteoData;
    SPS30DataProvider dustData;
    EspNowTransport view;
    static bool needPMMeasure;
    static uint32_t wakeupCounter;
    bool isMeasured = false;
    bool needSend = false;
};
