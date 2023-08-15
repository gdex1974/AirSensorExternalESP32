#include "EspNowTransport.h"

#include "Debug.h"
#include "AppConfig.h"
#include <esp_now.h>
#include <WiFi.h>

namespace
{
RTC_DATA_ATTR float humidity = 0;
RTC_DATA_ATTR float temperature = 0;
RTC_DATA_ATTR float pressure = 0;
RTC_DATA_ATTR int pm01 = 0;
RTC_DATA_ATTR int pm2p5 = 0;
RTC_DATA_ATTR int pm10 = 0;
RTC_DATA_ATTR float voltage = 0;

struct MeasurementDataMessage
{
    char spsSerial[32];
    uint16_t pm01;
    uint16_t pm25;
    uint16_t pm10;
    float humidity;
    float temperature;
    float pressure;
    float voltage;
    int64_t timestamp;
};

struct CorrectionMessage
{
    int64_t currentTime;
    int64_t receiveTime;
};

inline int64_t microsecondsFromTimeval(const timeval &tv)
{
    return ((int64_t)tv.tv_sec) * 1000000LL + tv.tv_usec;
}

MeasurementDataMessage measurementDataMessage;
CorrectionMessage correctionMessage;

volatile EspNowTransport::SendStatus sendStatus = EspNowTransport::SendStatus::Idle;
volatile unsigned long lastPacketMicroseconds = 0;
volatile int64_t lastPacketTimestamp = 0;
volatile unsigned long responseMicroseconds = 0;
volatile int64_t rtcCorrection = 0;

void onDataSent(const uint8_t* /*macAddr*/, esp_now_send_status_t status)
{
    sendStatus = status != ESP_NOW_SEND_SUCCESS ? EspNowTransport::SendStatus::Failed
                                                : EspNowTransport::SendStatus::Awaiting;
    DEBUG_LOG("Last Packet Send Status: " << ((status == ESP_NOW_SEND_SUCCESS) ? "Delivery Success" : "Delivery Fail"))
}

void onDataReceive(const uint8_t* /*mac_addr*/, const uint8_t* data, int data_len)
{
    if (data_len == sizeof(CorrectionMessage))
    {
        responseMicroseconds = micros();
        memcpy(&correctionMessage, data, data_len);
        const auto remoteReceivedTime = correctionMessage.receiveTime;
        const auto remoteSentTime = correctionMessage.currentTime;
        const auto remoteDelta = remoteSentTime - remoteReceivedTime;
        const auto localDelta = responseMicroseconds - lastPacketMicroseconds;

        const auto correctionFactor = (localDelta - remoteDelta) / 2;
        rtcCorrection = remoteReceivedTime - lastPacketTimestamp - correctionFactor;
        sendStatus = EspNowTransport::SendStatus::Completed;
    }
    else
    {
        DEBUG_LOG("Received unexpected data");
    }
}
}

EspNowTransport::EspNowTransport() = default;

bool EspNowTransport::setup(embedded::CharView sps30Serial, bool /*wakeUp*/)
{
    memset(measurementDataMessage.spsSerial, 0, sizeof(measurementDataMessage.spsSerial));
    memcpy(measurementDataMessage.spsSerial, sps30Serial.begin(),
           std::min(sizeof(measurementDataMessage.spsSerial), (std::size_t)sps30Serial.size()));
    return true;

}

bool EspNowTransport::prepareEspNow() const
{
    if (espNowPrepared)
    {
        return true;
    }
    WiFiClass::mode(WIFI_STA);

    if (esp_now_init() != ESP_OK)
    {
        Serial.println("Error initializing ESP-NOW");
        return false;
    }

    esp_now_register_send_cb(onDataSent);
    esp_now_register_recv_cb(onDataReceive);

    // Register peer
    esp_now_peer_info_t peerInfo;
    memset(&peerInfo, 0, sizeof(peerInfo));
    static_assert(sizeof(peerInfo.peer_addr) == sizeof(AppConfig::MacAddress), "MAC address size mismatch");
    memcpy(peerInfo.peer_addr, AppConfig::MacAddress.begin(), AppConfig::MacAddress.size());
    peerInfo.channel = 0;
    peerInfo.encrypt = false;
    peerInfo.ifidx = WIFI_IF_STA;

    // Add peer
    if (auto result = esp_now_add_peer(&peerInfo); result != ESP_OK)
    {
        DEBUG_LOG("Failed to add peer: " << esp_err_to_name(result));
        return false;
    }

    espNowPrepared = true;
    return true;
}

void EspNowTransport::updateView(float h, float t, float p, int p1, int p25, int p10, float batteryVoltage)
{
    if (h != humidity)
    {
        humidity = h;
        DEBUG_LOG("Humidity = " << humidity << "%")
    }

    if (t != temperature)
    {
        temperature = t;
        DEBUG_LOG("Temperature = " << temperature << " *C")
    }

    if (p != pressure)
    {
        pressure = p;
        DEBUG_LOG("Pressure = " << pressure << " Pa")
    }

    if (p1 != pm01)
    {
        pm01 = p1;
        DEBUG_LOG("PM1 = " << pm01)
    }

    if (p25 != pm2p5)
    {
        pm2p5 = p25;
        DEBUG_LOG("PM2.5 = " << pm2p5)
    }

    if (p10 != pm10)
    {
        pm10 = p10;
        DEBUG_LOG("PM10 = " << pm10)
    }

    if (voltage != batteryVoltage)
    {
        voltage = batteryVoltage;
        DEBUG_LOG("Voltage = " << voltage)
    }

    measurementDataMessage.pm01 = pm01;
    measurementDataMessage.pm25 = pm2p5;
    measurementDataMessage.pm10 = pm10;
    measurementDataMessage.pressure = pressure;
    measurementDataMessage.humidity = humidity;
    measurementDataMessage.temperature = temperature;
    measurementDataMessage.voltage = batteryVoltage;
    timeval tv {};
    gettimeofday(&tv, nullptr);
    measurementDataMessage.timestamp = microsecondsFromTimeval(tv);
    if (prepareEspNow())
    {
        lastPacketMicroseconds = micros();
        lastPacketTimestamp = measurementDataMessage.timestamp;
        auto result = esp_now_send(AppConfig::MacAddress.begin(), (uint8_t*)&measurementDataMessage,
                                   sizeof(measurementDataMessage));
        if (result == ESP_OK)
        {
            sendStatus = SendStatus::Requested;
        }
        else
        {
            DEBUG_LOG("Error sending the data");
        }
    }
}

int64_t EspNowTransport::getCorrection() const
{
    return rtcCorrection;
}

EspNowTransport::SendStatus EspNowTransport::getStatus() const
{
    return sendStatus;
}
