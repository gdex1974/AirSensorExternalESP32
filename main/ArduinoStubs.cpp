#include "DustMonitorController.h"

#include "AppConfig.h"

#include "PacketUart.h"
#include "Delays.h"
#include "Debug.h"
#include "esp32-arduino/PacketUartImpl.h"
#include "esp32-arduino/I2CBus.h"

#include <Arduino.h>
#include "Wire.h"

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
    DriversHolder(TwoWire& wire, HardwareSerial& serial, int8_t address)
        : i2CBus(wire)
        , i2CDevice(i2CBus, address)
        , uart2(static_cast<embedded::PacketUart::UartDevice&>(serial))
        , controller(uart2, i2CDevice)
    {
    }
    embedded::I2CBus i2CBus;
    embedded::I2CHelper i2CDevice;
    embedded::PacketUart uart2;
    DustMonitorController controller;
};

std::optional<DriversHolder> driversHolder;

bool wakeUp = false;
}

void setup()
{
    wakeUp = esp_sleep_get_wakeup_cause() == ESP_SLEEP_WAKEUP_TIMER;
    Serial.begin(115200, SERIAL_8N1, AppConfig::Serial1RxPin, AppConfig::Serial1TxPin);
    DEBUG_LOG((wakeUp ? "Woke up after sleep" : "Initial startup"))
    Serial2.begin(115200, SERIAL_8N1, AppConfig::Serial2RxPin, AppConfig::Serial2TxPin);
    Wire.begin(AppConfig::SDA, AppConfig::SCL);
    driversHolder.emplace(Wire, Serial2, AppConfig::BME280Address);
    if (!driversHolder->controller.setup(wakeUp)) {
        DEBUG_LOG("Setup failed!");
    }
}

void loop()
{
    auto delayTime = driversHolder->controller.process();
    if (delayTime > 700)
    {
        DEBUG_LOG("Going to deep sleep for " << delayTime << " ms")
        esp_sleep_enable_timer_wakeup(delayTime * 1000ll);
        esp_deep_sleep_start();
    }
    embedded::delay(delayTime);
}
