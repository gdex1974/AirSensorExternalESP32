#include "DustMonitorController.h"

#include "AppConfig.h"

#include "Delays.h"
#include "PacketUart.h"
#include "PersistentStorage.h"

#include "esp32-esp-idf/PacketUartImpl.h"
#include "esp32-esp-idf/I2CBus.h"
#include "esp32-esp-idf/SleepFunctions.h"

#include <esp_system.h>
#include <nvs_flash.h>

#include "Debug.h"

namespace
{
class ControllerHolder
{
public:
    ControllerHolder(embedded::PersistentStorage& storage, int i2CBusNum, int serialPortNum, int8_t address)
        : i2CBus(i2CBusNum)
        , i2CDevice(i2CBus, address)
        , uart2Device(serialPortNum)
        , uart2(uart2Device)
        , controller(storage, uart2, i2CDevice)
    {
        i2CBus.init(AppConfig::SDA, AppConfig::SCL, 100000);
    }
    DustMonitorController& getController() { return controller; }
private:
    embedded::I2CBus i2CBus;
    embedded::I2CHelper i2CDevice;
    embedded::PacketUart::UartDevice uart2Device;
    embedded::PacketUart uart2;
    DustMonitorController controller;
};

RTC_DATA_ATTR std::array<uint8_t, 2048> persistentArray;
std::optional<embedded::PersistentStorage> persistentStorage;
std::optional<ControllerHolder> driversHolder;

DustMonitorController::ResetReason resetReason = DustMonitorController::ResetReason::PowerOn;
}

void setup()
{
    using ResetReason = DustMonitorController::ResetReason;
    switch (esp_reset_reason())
    {
    case ESP_RST_DEEPSLEEP:
        resetReason = ResetReason::DeepSleep;
        break;
    case ESP_RST_BROWNOUT:
        resetReason = ResetReason::BrownOut;
        break;
    case ESP_RST_POWERON:
    default:
        resetReason = ResetReason::PowerOn;
        break;
    }
    persistentStorage.emplace(persistentArray, resetReason != ResetReason::DeepSleep);
#ifdef DEBUG_SERIAL_OUT
    embedded::PacketUart::UartDevice::init(0, AppConfig::serial1RxPin, AppConfig::serial1TxPin, 115200);
#endif
    DEBUG_LOG((resetReason == ResetReason::DeepSleep ? "Wake up after sleep" : (resetReason == ResetReason::BrownOut ? "Reset after voltage drop" : "Initial startup")))
    embedded::PacketUart::UartDevice::init(2, AppConfig::serial2RxPin, AppConfig::serial2TxPin, 115200);
    driversHolder.emplace(*persistentStorage, 0, 2, AppConfig::bme280Address);
    if (!driversHolder->getController().setup(resetReason))
    {
        DEBUG_LOG("Setup failed!")
    }
}

void loop()
{
    auto delayTime = driversHolder->getController().process();
    if (delayTime > 1000)
    {
        driversHolder->getController().hibernate();
        DEBUG_LOG("Going to deep sleep for " << delayTime << " ms")
        embedded::deepSleep(delayTime);
    }
    embedded::delay(delayTime);
}

extern "C" void app_main()
{
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK( nvs_flash_erase() );
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK( ret );
    setup();
    while (true)
    {
        loop();
    }
}
