/**
 * ShitBird Firmware - Power Management
 */

#ifndef SHITBIRD_POWER_H
#define SHITBIRD_POWER_H

#include <Arduino.h>
#include "config.h"

class Power {
public:
    static void init();
    static void update();

    // Battery status
    static float getVoltage();
    static uint8_t getPercent();
    static bool isCharging();
    static bool isLowBattery();
    static bool isCriticalBattery();

    // Power control
    static void sleep();
    static void deepSleep(uint32_t seconds = 0);
    static void restart();
    static void shutdown();

    // Peripheral power
    static void enablePeripherals();
    static void disablePeripherals();

private:
    static float voltage;
    static uint8_t percent;
    static bool charging;
    static unsigned long lastUpdate;

    static float readBatteryVoltage();
    static uint8_t voltageToPercent(float v);
};

#endif // SHITBIRD_POWER_H
