/**
 * ShitBird Firmware - Main Entry Point
 * Target: LilyGo T-Deck Plus (ESP32-S3)
 *
 * A comprehensive penetration testing toolkit firmware
 */

#include <Arduino.h>
#include "config.h"
#include "core/system.h"
#include "core/display.h"
#include "core/keyboard.h"
#include "core/storage.h"
#include "core/power.h"
#include "ui/ui_manager.h"
#include "ui/splash.h"

// Module includes
#if ENABLE_WIFI
#include "modules/wifi/wifi_module.h"
#endif

#if ENABLE_BLE
#include "modules/ble/ble_module.h"
#endif

#if ENABLE_LORA
#include "modules/lora/lora_module.h"
#endif

#if ENABLE_GPS
#include "modules/gps/gps_module.h"
#endif

#if ENABLE_IR
#include "modules/ir/ir_module.h"
#endif

#if ENABLE_BADUSB
#include "modules/badusb/badusb_module.h"
#endif

#if ENABLE_WEB_SERVER
#include "web/web_server.h"
#endif

// Global system state
SystemState g_systemState;

void setup() {
    // Initialize serial for debugging
    Serial.begin(115200);
    delay(100);

    Serial.println("\n");
    Serial.println("╔════════════════════════════════════════╗");
    Serial.println("║         ShitBird Firmware v1.0         ║");
    Serial.println("║     Penetration Testing Toolkit        ║");
    Serial.println("╚════════════════════════════════════════╝");
    Serial.println();

    // Enable peripheral power (CRITICAL for T-Deck Plus)
    pinMode(POWER_ON_PIN, OUTPUT);
    digitalWrite(POWER_ON_PIN, HIGH);
    delay(100);

    // Initialize core systems
    Serial.println("[BOOT] Initializing display...");
    Display::init();

    // Show splash screen
    Splash::show();

    // Skip settings load - use defaults from constructor
    // Serial.println("[BOOT] Loading settings...");
    // g_systemState.loadSettings();

    Serial.println("[BOOT] Initializing keyboard...");
    Keyboard::init();

    // Skip power init for now
    // Serial.println("[BOOT] Initializing power management...");
    // Power::init();

    // Initialize optional modules
    #if ENABLE_WIFI
    Serial.println("[BOOT] Initializing WiFi module...");
    WiFiModule::init();
    #endif

    #if ENABLE_BLE
    Serial.println("[BOOT] Initializing BLE module...");
    BLEModule::init();
    #endif

    #if ENABLE_LORA
    Serial.println("[BOOT] Initializing LoRa module...");
    LoRaModule::init();
    #endif

    // SD card init after LoRa (SPI must be initialized first)
    #if ENABLE_SD
    Serial.println("[BOOT] Initializing storage...");
    Storage::init();
    #endif

    #if ENABLE_GPS
    Serial.println("[BOOT] Initializing GPS module...");
    GPSModule::init();
    #endif

    #if ENABLE_IR
    Serial.println("[BOOT] Initializing IR module...");
    IRModule::init();
    #endif

    #if ENABLE_BADUSB
    Serial.println("[BOOT] Initializing BadUSB module...");
    BadUSBModule::init();
    #endif

    // Initialize UI
    Serial.println("[BOOT] Initializing UI...");
    UIManager::init();

    // Boot complete
    Serial.println("[BOOT] Boot complete!");
    Serial.printf("[BOOT] Free heap: %d bytes\n", ESP.getFreeHeap());
    Serial.printf("[BOOT] Free PSRAM: %d bytes\n", ESP.getFreePsram());

    // Clear splash and show main menu
    Splash::hide();
    UIManager::showMainMenu();
}

void loop() {
    // Update keyboard input
    Keyboard::update();

    // Update GPS
    #if ENABLE_GPS
    GPSModule::update();
    #endif

    // Update UI
    UIManager::update();

    // Yield to other tasks
    delay(10);
}
