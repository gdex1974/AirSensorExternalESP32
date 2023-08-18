#include "DustMonitorController.h"

#include "AppConfig.h"

#include "Delays.h"
#include "PacketUart.h"
#include "PersistentStorage.h"

#include "esp32-arduino/PacketUartImpl.h"
#include "esp32-arduino/I2CBus.h"

#include <Arduino.h>
#include <Wire.h>

#include "Debug.h"

namespace
{
// for STM32 platform or pure esp-idf UartDevice is a nontrivial class,
// but for Arduino-based platforms Hardware Serial provides enough functionality.
// We can't just make name aliasing to allow forward declaration, so
// we have to use trivial inheritance.
static_assert(sizeof(embedded::PacketUart::UartDevice) == sizeof(HardwareSerial));
static_assert(std::is_base_of_v<HardwareSerial, embedded::PacketUart::UartDevice>);

struct DriversHolder
{
    DriversHolder(embedded::PersistentStorage& storage, TwoWire& wire, HardwareSerial& serial, int8_t address)
        : i2CBus(wire)
        , i2CDevice(i2CBus, address)
        , uart2(static_cast<embedded::PacketUart::UartDevice&>(serial))
        , controller(storage, uart2, i2CDevice)
    {
    }
    embedded::I2CBus i2CBus;
    embedded::I2CHelper i2CDevice;
    embedded::PacketUart uart2;
    DustMonitorController controller;
};

RTC_DATA_ATTR std::array<uint8_t, 2048> persistentArray;
std::optional<embedded::PersistentStorage> persistentStorage;
std::optional<DriversHolder> driversHolder;

bool wakeUp = false;
}

void setup()
{
    persistentStorage.emplace(persistentArray,esp_reset_reason() != ESP_RST_DEEPSLEEP);
    wakeUp = esp_sleep_get_wakeup_cause() == ESP_SLEEP_WAKEUP_TIMER;
#ifdef DEBUG_SERIAL_OUT
    Serial.begin(115200, SERIAL_8N1, AppConfig::serial1RxPin, AppConfig::serial1TxPin);
#endif
    DEBUG_LOG((wakeUp ? "Wake up after sleep" : "Initial startup"))
    Serial2.begin(115200, SERIAL_8N1, AppConfig::serial2RxPin, AppConfig::serial2TxPin);
    Wire.begin(AppConfig::SDA, AppConfig::SCL);
    driversHolder.emplace(*persistentStorage, Wire, Serial2, AppConfig::bme280Address);
    if (!driversHolder->controller.setup(wakeUp)) {
        DEBUG_LOG("Setup failed!");
    }
}

void loop()
{
    auto delayTime = driversHolder->controller.process();
    if (delayTime > 700)
    {
        driversHolder->controller.hibernate();
        DEBUG_LOG("Going to deep sleep for " << delayTime << " ms")
        esp_sleep_enable_timer_wakeup(delayTime * 1000ll);
        esp_deep_sleep_start();
    }
    embedded::delay(delayTime);
}
