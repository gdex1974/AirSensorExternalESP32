#pragma once

#include "MemoryView.h"
#include <cstdint>

class EspNowTransport {
public:
    struct Data
    {
        float humidity {};
        float temperature {};
        float pressure {};
        int pm01 {};
        int pm25 {};
        int pm10 {};
        float batteryVoltage {};
        uint32_t flags {};
    };
    enum class SendStatus {Idle, Requested, Failed, Awaiting, Completed};

    EspNowTransport() = default;
    bool setup(embedded::CharView serial, bool wakeUp);
    void updateView(const Data& data);
    SendStatus getStatus() const;
    int64_t getCorrection() const;

private:
    bool prepareEspNow() const;
    mutable bool espNowPrepared = false;
    embedded::CharView sps30Serial;
};
