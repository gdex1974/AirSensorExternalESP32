#pragma once

#include "MemoryView.h"
#include <cstdint>

class EspNowTransport {
public:
    enum class SendStatus {Idle, Requested, Failed, Awaiting, Completed};

    EspNowTransport();
    bool setup(embedded::CharView sps30Serial, bool wakeUp);
    void updateView(float h, float t, float p, int p1, int p25, int p10, float batteryVoltage);
    SendStatus getStatus() const;
    int64_t getCorrection() const;

private:

    bool prepareEspNow() const;
    mutable bool espNowPrepared = false;
};
