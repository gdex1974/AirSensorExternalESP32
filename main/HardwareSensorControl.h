#pragma once

#ifdef IDF_TARGET_ESP32
#include "esp32/HardwareSensorControl.h"
#elif defined(IDF_TARGET_ESP32C3)
#include "esp32c3/HardwareSensorControl.h"
#endif
