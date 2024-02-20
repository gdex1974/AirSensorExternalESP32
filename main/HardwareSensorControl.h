#pragma once

class HardwareSensorControl
{
public:
    static void initStepUpControl(bool set = false);
    static void switchStepUpConversion(bool enable);
    static void holdStepUpConversion();
    static void activateDeepSleepGpioHold();

    static const int bootEstimationMicroseconds;
};
