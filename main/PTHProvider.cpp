#include "PTHProvider.h"

#include "Debug.h"
#include "Delays.h"

#include <cmath>

bool PTHProvider::activate() {
    return bme.startMeasurement();
}

bool PTHProvider::hibernate() {
    return bme.stopMeasurement();
}

bool PTHProvider::setup(bool /* wakeUp */)
{
    auto result = bme.init();
    DEBUG_LOG("BME280 is initialized with the result:" << result)
    return result == 0;
}

bool PTHProvider::doMeasure()
{
    while (bme.isMeasuring())
        embedded::delay(10);
    return bme.getMeasureData(temperature, pressure, humidity);
}
