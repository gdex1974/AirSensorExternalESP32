#include "EspNowTransport.h"

#include "AppConfig.h"
#include "TimeFunctions.h"

#include "PersistentStorage.h"
#include "Delays.h"

#include <esp_now.h>
#include <esp_wifi.h>
#include <variant>
#include <freertos/task.h>
#include <freertos/event_groups.h>

#include "Debug.h"

namespace
{

enum class EventType {DataReady, SendCallback, ReceiveCallback, Exit};

constexpr std::string_view transportDataTag = "ESPN";
constexpr suseconds_t firstAttemptMicroseconds = 800000;

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

struct CorrectionMessage
{
    int64_t currentTime;
    int64_t receiveTime;
};

struct EventData
{
    EventType type = EventType::Exit;
    std::array<uint8_t, 6> macAddr = {};
    std::variant<int64_t, esp_now_send_status_t> data;
};

volatile uint64_t lastPacketMicroseconds = 0;
volatile int64_t lastPacketTimestamp = 0;
volatile uint64_t responseMicroseconds = 0;
auto espnowQueue = std::unique_ptr<std::remove_pointer_t<QueueHandle_t>, decltype(&vQueueDelete)>(nullptr, &vQueueDelete);
EventGroupHandle_t espnowEventGroup = nullptr;

void onDataSent(const uint8_t* macAddr, esp_now_send_status_t status)
{
    EventData evt;
    evt.type = EventType::SendCallback;
    memcpy(evt.macAddr.begin(), macAddr, evt.macAddr.size());
    evt.data = status;
    xQueueSend(espnowQueue.get(), &evt, portMAX_DELAY);
}

void onDataReceive(const uint8_t* mac_addr, const uint8_t* data, int data_len)
{
    if (data_len == sizeof(CorrectionMessage))
    {
        union
        {
            CorrectionMessage correctionMessage;
            std::array<uint8_t, sizeof(CorrectionMessage)> bytes;
        } correctionData;

        responseMicroseconds = embedded::getMicrosecondTicks();
        memcpy(correctionData.bytes.begin(), data, data_len);
        const auto &remoteReceivedTime = correctionData.correctionMessage.receiveTime;
        const auto &remoteSentTime = correctionData.correctionMessage.currentTime;
        const auto remoteDelta = remoteSentTime - remoteReceivedTime;
        const auto localDelta = static_cast<int64_t>(responseMicroseconds - lastPacketMicroseconds);

        const auto correctionFactor = (localDelta - remoteDelta) / 2;
        const int64_t rtcCorrection = remoteReceivedTime - lastPacketTimestamp - correctionFactor;
        EventData evt {
                .type = EventType::ReceiveCallback, .data = rtcCorrection
        };
        std::memcpy(evt.macAddr.begin(), mac_addr, evt.macAddr.size());
        xQueueSend(espnowQueue.get(), &evt, portMAX_DELAY);
    }
    else
    {
        DEBUG_LOG("Received unexpected data length " << data_len << " from " << embedded::BytesView(const_cast<uint8_t*>(mac_addr), 6) << " expected " << sizeof(CorrectionMessage) << " bytes")
    }
}

void espnowTask(void *pvParameter)
{
    auto* transport = reinterpret_cast<EspNowTransport*>(pvParameter);
    transport->threadFunction();
}

[[noreturn]] void delayedSend(void* /*pvParameter*/)
{
    while(true)
    {
        if (xEventGroupWaitBits(espnowEventGroup, BIT0, pdTRUE, pdTRUE, portMAX_DELAY) == BIT0)
        {
            embedded::delay(100);
            EventData evt { .type = EventType::DataReady, .data = 0 };
            xQueueSend(espnowQueue.get(), &evt, portMAX_DELAY);
        }
    }
}

} // namespace


void EspNowTransport::threadFunction()
{
    EventData evt;
    while (xQueueReceive(espnowQueue.get(), &evt, portMAX_DELAY) == pdTRUE) {
        switch (evt.type) {
            case EventType::DataReady:
                if (attemptsCounter == 0)
                {
                    timeval lastPacketTime;
                    gettimeofday(&lastPacketTime, nullptr);
                    if (lastPacketTime.tv_usec < firstAttemptMicroseconds)
                    {
                        embedded::delay((firstAttemptMicroseconds - lastPacketTime.tv_usec) / 1000);
                    }
                }
                sendData();
                break;
            case EventType::SendCallback:
                if (std::holds_alternative<esp_now_send_status_t>(evt.data))
                {
                     if (const auto status = std::get<esp_now_send_status_t>(evt.data); status != ESP_NOW_SEND_SUCCESS)
                     {
                         DEBUG_LOG("Last Packet delivery to " << embedded::BytesView(evt.macAddr) << " fail")
                         if (attemptsCounter < maxAttempts)
                         {
                             DEBUG_LOG("Retrying to send packet to " << embedded::BytesView(evt.macAddr) << " attempt " << (attemptsCounter + 1))
                             xEventGroupSetBits(espnowEventGroup, BIT0);
                         }
                         else
                         {
                             DEBUG_LOG("Max delivery attempts to " << embedded::BytesView(evt.macAddr) << " reached")
                             sendStatus = SendStatus::Failed;
                             xEventGroupSetBits(espnowEventGroup, BIT1);
                         }
                     }
                     else
                     {
                         sendStatus = SendStatus::Awaiting;
                         DEBUG_LOG("Last packet successfully sent to " << embedded::BytesView(evt.macAddr) << " from " << attemptsCounter << " attempt")
                     }
                }
                break;
            case EventType::ReceiveCallback:
                if (std::holds_alternative<int64_t>(evt.data))
                {
                    rtcCorrection = std::get<int64_t>(evt.data);
                    sendStatus = EspNowTransport::SendStatus::Completed;
                    xEventGroupSetBits(espnowEventGroup, BIT2);
                }
                break;
            case EventType::Exit:
                return;
        }
    }
}

bool EspNowTransport::setup(embedded::CharView serial, bool /*wakeUp*/)
{
    sps30Serial = serial;
    espnowQueue.reset(xQueueCreate(6, sizeof(EventData)));
    espnowEventGroup = xEventGroupCreate();
    xTaskCreate(espnowTask, "espnowTask", 2048, this, 4, nullptr);
    xTaskCreate(delayedSend, "delayedSend", 1024, nullptr, 4, nullptr);
    return true;
}

bool EspNowTransport::prepareEspNow()
{
    if (espNowPrepared)
    {
        return true;
    }
    initWiFi();
    if (restrictTxPower)
    {
        esp_wifi_set_max_tx_power(34);
    }

    if (esp_now_init() != ESP_OK)
    {
        DEBUG_LOG("Error initializing ESP-NOW")
        return false;
    }
    if (espnowQueue == nullptr) {
        DEBUG_LOG("Failed to create event queue")
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
        DEBUG_LOG("Failed to add peer: " << esp_err_to_name(result))
        return false;
    }

    espNowPrepared = true;
    return true;
}

bool EspNowTransport::sendData(const Data &transportData)
{
    data = transportData;
    if (prepareEspNow())
    {
        EventData evt { .type = EventType::DataReady, .data = 0 };
        xQueueSend(espnowQueue.get(), &evt, portMAX_DELAY);
        if (xEventGroupWaitBits(espnowEventGroup, BIT1 | BIT2, pdFALSE, pdFALSE, pdMS_TO_TICKS(1000)) == BIT2)
        {
            return true;
        }
    }
    sendStatus = SendStatus::Failed;
    return false;
}

bool EspNowTransport::sendData()
{
    union
    {
        struct
        {
            char spsSerial[32];
            int16_t pm01;
            int16_t pm25;
            int16_t pm10;
            float humidity;
            float temperature;
            float pressure;
            float voltage;
            int64_t timestamp;
            uint32_t flags;
        } message;
        std::array<uint8_t, sizeof(message)> bytes;
    } measurementDataMessage;

    memcpy(measurementDataMessage.message.spsSerial, sps30Serial.begin(),
           std::min(sizeof(measurementDataMessage.message.spsSerial), (std::size_t)sps30Serial.size()));
    measurementDataMessage.message.pm01 = data.pm01;
    measurementDataMessage.message.pm25 = data.pm25;
    measurementDataMessage.message.pm10 = data.pm10;
    measurementDataMessage.message.pressure = data.pressure;
    measurementDataMessage.message.humidity = data.humidity;
    measurementDataMessage.message.temperature = data.temperature;
    measurementDataMessage.message.voltage = data.batteryVoltage;
    measurementDataMessage.message.flags = data.flags;

    lastPacketMicroseconds = embedded::getMicrosecondTicks();
    measurementDataMessage.message.timestamp = microsecondsNow();
    lastPacketTimestamp = measurementDataMessage.message.timestamp;
    ++attemptsCounter;
    sendStatus = SendStatus::Requested;

    if (const auto result = esp_now_send(nullptr, measurementDataMessage.bytes.begin(),
                                   measurementDataMessage.bytes.size()); result != ESP_OK)
    {
        sendStatus = SendStatus::Failed;
        DEBUG_LOG("Error sending the data: " << esp_err_to_name(result));
        return false;
    }
    return true;
}

int64_t EspNowTransport::getCorrection() const
{
    return rtcCorrection;
}

EspNowTransport::SendStatus EspNowTransport::getStatus() const
{
    return sendStatus;
}

bool EspNowTransport::hibernate()
{
    sendStatus = SendStatus::Idle;
    if (espNowPrepared)
    {
        esp_now_deinit();
        esp_wifi_stop();
    }
    return true;
}

int64_t EspNowTransport::getLastPacketTimestamp() const
{
    return lastPacketTimestamp;
}
