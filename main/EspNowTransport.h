#pragma once

#include "MemoryView.h"
#include <cstdint>

namespace embedded
{
class PersistentStorage;
}

class EspNowTransport {
public:
    struct Data
    {
        float humidity {};
        float temperature {};
        float pressure {};
        int16_t pm01 {};
        int16_t pm25 {};
        int16_t pm10 {};
        float batteryVoltage {};
        uint32_t flags {};
    };
    enum class SendStatus {Idle, Requested, Failed, Awaiting, Completed};

    EspNowTransport(embedded::PersistentStorage &storage, bool restrictTxPower)
    : storage(storage), restrictTxPower(restrictTxPower) {}
    bool setup(embedded::CharView serial, bool wakeUp);
    bool sendData(const Data& transportData);
    SendStatus getStatus() const;
    int64_t getCorrection() const;
    bool hibernate();
    void threadFunction();

    int64_t getLastPacketTimestamp() const;
private:
    bool prepareEspNow();
    bool sendData();
    embedded::PersistentStorage &storage;
    Data data;
    volatile SendStatus sendStatus = EspNowTransport::SendStatus::Idle;
    mutable bool espNowPrepared = false;
    bool restrictTxPower;
    embedded::CharView sps30Serial;
    volatile int64_t rtcCorrection = 0;
    int attemptsCounter = 0;
    static constexpr int maxAttempts = 10;
};
