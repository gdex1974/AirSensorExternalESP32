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

using ResetReason = DustMonitorController::ResetReason;
namespace
{
class ControllerHolder
{
public:
    ControllerHolder(embedded::PersistentStorage& storage, int serialPortNum, int8_t address, bool restrictTxPower)
        : i2CBus(0)
        , i2CDevice(i2CBus, address)
        , uart2Device(serialPortNum)
        , uart2(uart2Device)
        , controller(storage, uart2, i2CDevice, restrictTxPower)
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
std::optional<ControllerHolder> controllerHolder;

ResetReason getResetReason()
{
    ResetReason resetReason;
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
    return resetReason;
}

bool setup(ResetReason resetReason)
{
#ifdef DEBUG_SERIAL_OUT
    embedded::PacketUart::UartDevice::init(0, AppConfig::serial1RxPin, AppConfig::serial1TxPin, 115200);
#endif
    DEBUG_LOG((resetReason == ResetReason::DeepSleep ? "Wake up after sleep" : (resetReason == ResetReason::BrownOut ? "Reset after voltage drop" : "Initial startup")))
    persistentStorage.emplace(persistentArray, resetReason != ResetReason::DeepSleep);
    embedded::PacketUart::UartDevice::init(AppConfig::Sps30UartNum, AppConfig::sps30RxPin, AppConfig::sps30TxPin, 115200);
    controllerHolder.emplace(*persistentStorage, AppConfig::Sps30UartNum, AppConfig::bme280Address, AppConfig::restrictTxPower);
    return controllerHolder->getController().setup(resetReason);
}

}

extern "C" [[noreturn]] void app_main()
{
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK( nvs_flash_erase() );
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK( ret );
    ResetReason resetReason = getResetReason();
    if (!setup(resetReason))
    {
        DEBUG_LOG("Setup failed. No sensors found of failed to initialize WiFi. Terminating...")
        embedded::delay(1000);
        std::terminate();
    }

    const auto delayTime = controllerHolder->getController().process();
    controllerHolder->getController().hibernate();
    DEBUG_LOG("Going to deep sleep for " << delayTime << " ms")
    embedded::deepSleep(delayTime);
}
