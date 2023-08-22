#include "EspNowTransport.h"

#include "AppConfig.h"
#include "TimeFunctions.h"

#include "PersistentStorage.h"
#include "Delays.h"

#include <esp_now.h>
#include <esp_wifi.h>

#include "Debug.h"

namespace
{

void initWiFi()
{
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK( esp_wifi_init(&cfg) );
    ESP_ERROR_CHECK( esp_wifi_set_storage(WIFI_STORAGE_RAM) );
    ESP_ERROR_CHECK( esp_wifi_set_mode(WIFI_MODE_STA) );
    ESP_ERROR_CHECK( esp_wifi_start());
}

struct MeasurementDataMessage
{
    char spsSerial[32]{};
    uint16_t pm01{};
    uint16_t pm25{};
    uint16_t pm10{};
    float humidity{};
    float temperature{};
    float pressure{};
    float voltage{};
    int64_t timestamp{};
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

constexpr std::string_view transportDataTag = "ESPN";

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
        responseMicroseconds = embedded::getMillisecondTicks();
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

bool EspNowTransport::setup(embedded::CharView sps30Serial, bool /*wakeUp*/)
{
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
    initWiFi();

    if (esp_now_init() != ESP_OK)
    {
        DEBUG_LOG("Error initializing ESP-NOW");
        return false;
    }

    esp_now_register_send_cb(onDataSent);
    esp_now_register_recv_cb(onDataReceive);

    // Register peer
    esp_now_peer_info_t peerInfo;
    memset(&peerInfo, 0, sizeof(peerInfo));
    static_assert(sizeof(peerInfo.peer_addr) == sizeof(AppConfig::macAddress), "MAC address size mismatch");
    memcpy(peerInfo.peer_addr, AppConfig::macAddress.begin(), AppConfig::macAddress.size());
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

void EspNowTransport::updateView(const EspNowTransport::Data &transportData)
{
    measurementDataMessage.pm01 = transportData.pm01;
    measurementDataMessage.pm25 = transportData.pm25;
    measurementDataMessage.pm10 = transportData.pm10;
    measurementDataMessage.pressure = transportData.pressure;
    measurementDataMessage.humidity = transportData.humidity;
    measurementDataMessage.temperature = transportData.temperature;
    measurementDataMessage.voltage = transportData.batteryVoltage;

    if (prepareEspNow())
    {
        lastPacketMicroseconds = embedded::getMicrosecondTicks();
        measurementDataMessage.timestamp = microsecondsNow();
        lastPacketTimestamp = measurementDataMessage.timestamp;
        auto result = esp_now_send(AppConfig::macAddress.begin(), (uint8_t*)&measurementDataMessage,
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
