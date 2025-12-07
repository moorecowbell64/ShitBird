/**
 * ShitBird Firmware - Power Management Implementation
 */

#include "power.h"
#include "system.h"
#include <esp_sleep.h>

float Power::voltage = 0.0;
uint8_t Power::percent = 0;
bool Power::charging = false;
unsigned long Power::lastUpdate = 0;

void Power::init() {
    Serial.println("[POWER] Initializing...");

    // Configure battery ADC pin
    pinMode(BAT_ADC_PIN, INPUT);
    analogSetAttenuation(ADC_11db);  // Full range 0-3.3V
    analogReadResolution(12);         // 12-bit resolution

    // Ensure peripheral power is enabled
    enablePeripherals();

    // Initial reading
    update();

    Serial.printf("[POWER] Battery: %.2fV (%d%%)\n", voltage, percent);
    Serial.println("[POWER] Initialized");
}

void Power::update() {
    // Update every 5 seconds to avoid ADC noise
    if (millis() - lastUpdate < 5000 && lastUpdate > 0) {
        return;
    }
    lastUpdate = millis();

    // Read battery voltage (average multiple readings)
    voltage = readBatteryVoltage();
    percent = voltageToPercent(voltage);

    // Update global state
    g_systemState.batteryVoltage = voltage;
    g_systemState.batteryPercent = percent;
    g_systemState.charging = charging;

    // Check for critical battery
    if (isCriticalBattery()) {
        Serial.println("[POWER] CRITICAL: Battery critically low!");
        // TODO: Show warning, save state, prepare for shutdown
    }
}

float Power::readBatteryVoltage() {
    // Take multiple readings and average
    const int numReadings = 10;
    uint32_t sum = 0;

    for (int i = 0; i < numReadings; i++) {
        sum += analogRead(BAT_ADC_PIN);
        delay(5);
    }

    uint32_t avgReading = sum / numReadings;

    // Convert ADC reading to voltage
    // T-Deck Plus uses a voltage divider (typically 2:1)
    // ADC reference is 3.3V with 11dB attenuation
    // 12-bit ADC = 4096 steps
    float adcVoltage = (avgReading / 4095.0) * 3.3;
    float batteryVoltage = adcVoltage * 2.0;  // Account for voltage divider

    return batteryVoltage;
}

uint8_t Power::voltageToPercent(float v) {
    // Li-Po battery discharge curve approximation
    // Full: 4.2V, Empty: 3.3V (with some margin)

    if (v >= 4.2) return 100;
    if (v <= 3.3) return 0;

    // Non-linear approximation of Li-Po discharge curve
    if (v >= 4.0) {
        // 4.0V - 4.2V = 80% - 100%
        return 80 + ((v - 4.0) / 0.2) * 20;
    } else if (v >= 3.8) {
        // 3.8V - 4.0V = 40% - 80%
        return 40 + ((v - 3.8) / 0.2) * 40;
    } else if (v >= 3.6) {
        // 3.6V - 3.8V = 15% - 40%
        return 15 + ((v - 3.6) / 0.2) * 25;
    } else {
        // 3.3V - 3.6V = 0% - 15%
        return ((v - 3.3) / 0.3) * 15;
    }
}

float Power::getVoltage() {
    return voltage;
}

uint8_t Power::getPercent() {
    return percent;
}

bool Power::isCharging() {
    // TODO: Detect charging state from charger IC
    // The TP4065B charger may have a status pin
    return charging;
}

bool Power::isLowBattery() {
    return percent <= 20;
}

bool Power::isCriticalBattery() {
    return percent <= 5;
}

void Power::enablePeripherals() {
    pinMode(POWER_ON_PIN, OUTPUT);
    digitalWrite(POWER_ON_PIN, HIGH);
}

void Power::disablePeripherals() {
    digitalWrite(POWER_ON_PIN, LOW);
}

void Power::sleep() {
    Serial.println("[POWER] Entering light sleep...");

    // Disable peripherals to save power
    disablePeripherals();

    // Configure wake sources
    esp_sleep_enable_ext0_wakeup((gpio_num_t)KB_INT_PIN, 0);  // Wake on key press
    esp_sleep_enable_timer_wakeup(60 * 1000000ULL);  // Wake every 60 seconds

    // Enter light sleep
    esp_light_sleep_start();

    // Woke up - re-enable peripherals
    enablePeripherals();
    Serial.println("[POWER] Woke from light sleep");
}

void Power::deepSleep(uint32_t seconds) {
    Serial.printf("[POWER] Entering deep sleep for %d seconds...\n", seconds);

    // Save any pending data
    g_systemState.saveSettings();

    // Disable peripherals
    disablePeripherals();

    // Configure wake sources
    esp_sleep_enable_ext0_wakeup((gpio_num_t)KB_INT_PIN, 0);

    if (seconds > 0) {
        esp_sleep_enable_timer_wakeup(seconds * 1000000ULL);
    }

    // Enter deep sleep (will reset on wake)
    esp_deep_sleep_start();
}

void Power::restart() {
    Serial.println("[POWER] Restarting...");
    g_systemState.saveSettings();
    delay(100);
    ESP.restart();
}

void Power::shutdown() {
    Serial.println("[POWER] Shutting down...");
    g_systemState.saveSettings();

    // Disable all peripherals
    disablePeripherals();

    // Enter deep sleep indefinitely (wake on button press)
    esp_sleep_enable_ext0_wakeup((gpio_num_t)KB_INT_PIN, 0);
    esp_deep_sleep_start();
}
