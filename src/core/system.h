/**
 * ShitBird Firmware - System State & Configuration
 */

#ifndef SHITBIRD_SYSTEM_H
#define SHITBIRD_SYSTEM_H

#include <Arduino.h>
#include <Preferences.h>
#include "config.h"

// ============================================================================
// ENUMS
// ============================================================================

enum class OperationMode {
    IDLE,
    WIFI_SCAN,
    WIFI_ATTACK,
    BLE_SCAN,
    BLE_ATTACK,
    LORA_SCAN,
    LORA_ATTACK,
    IR_TX,
    IR_RX,
    BADUSB,
    GPS_ACTIVE
};

enum class Profile {
    RECON_ONLY,         // Passive scanning, no transmission
    WIFI_ASSESSMENT,    // Full WiFi toolkit
    BLE_HUNT,           // BLE-focused aggressive scanning
    PHYSICAL_SECURITY,  // IR, BadUSB, RF
    STEALTH_MODE,       // Minimal RF emissions, logging only
    FULL_ASSAULT,       // All modules active
    CUSTOM              // User-defined
};

enum class Theme {
    HACKER,         // Green on black
    CYBERPUNK,      // Cyan/magenta on dark
    STEALTH,        // Dark gray minimal
    RETRO,          // Amber on black
    BLOOD,          // Red on black
    OCEAN,          // Blue tones
    CUSTOM          // User-defined
};

// ============================================================================
// STRUCTURES
// ============================================================================

struct ThemeColors {
    uint16_t bgPrimary;
    uint16_t bgSecondary;
    uint16_t textPrimary;
    uint16_t textSecondary;
    uint16_t accent;
    uint16_t warning;
    uint16_t error;
    uint16_t success;
};

struct WiFiSettings {
    bool enabled;
    uint8_t defaultChannel;
    uint16_t deauthInterval;
    uint16_t beaconInterval;
    bool captureHandshakes;
    bool autoSavePcap;
};

struct BLESettings {
    bool enabled;
    uint8_t scanDuration;
    uint16_t spamInterval;
    bool autoEnumerate;
};

struct LoRaSettings {
    bool enabled;
    float frequency;
    float bandwidth;
    uint8_t spreadFactor;
    uint8_t codingRate;
    int8_t txPower;
    bool meshtasticMode;
};

struct IRSettings {
    bool enabled;
    uint8_t txPin;
    uint8_t rxPin;
    bool learningMode;
};

struct AudioSettings {
    bool enabled;
    uint8_t volume;
    bool keyClickSound;
    bool alertSounds;
};

struct SecuritySettings {
    bool pinEnabled;
    char pin[SECURITY_PIN_LENGTH + 1];
    uint8_t maxAttempts;
    bool autoLockEnabled;
    uint16_t autoLockTimeout;
    bool encryptLogs;
    bool panicWipeEnabled;
    char panicSequence[16];
};

struct DisplaySettings {
    uint8_t brightness;
    uint8_t sleepTimeout;
    Theme theme;
    ThemeColors customColors;
    bool animationsEnabled;
};

struct SystemSettings {
    WiFiSettings wifi;
    BLESettings ble;
    LoRaSettings lora;
    IRSettings ir;
    AudioSettings audio;
    SecuritySettings security;
    DisplaySettings display;
    Profile activeProfile;
    char deviceName[32];
};

// ============================================================================
// SYSTEM STATE CLASS
// ============================================================================

class SystemState {
public:
    // Current state
    bool locked = false;
    OperationMode currentMode = OperationMode::IDLE;
    unsigned long lastActivityTime = 0;

    // Settings
    SystemSettings settings;

    // Constructor to set defaults
    SystemState() {
        settings.wifi = {true, 1, 100, 100, true, true};
        settings.ble = {true, 10, 20, false};
        settings.lora = {true, 915.0f, 125.0f, 7, 5, 22, true};
        settings.ir = {true, 2, 1, false};
        settings.audio = {true, 50, true, true};
        settings.security = {false, {'0','0','0','0','0','0',0}, 3, false, 300, false, false, {'*','*','*','*',0}};
        settings.display = {200, 60, Theme::HACKER, {}, true};
        settings.activeProfile = Profile::RECON_ONLY;
        strncpy(settings.deviceName, "ShitBird", sizeof(settings.deviceName));
    }

    // Status flags
    bool wifiConnected = false;
    bool bleConnected = false;
    bool loraActive = false;
    bool gpsFixed = false;
    bool sdMounted = false;

    // Battery
    float batteryVoltage = 0.0;
    uint8_t batteryPercent = 0;
    bool charging = false;

    // Statistics
    uint32_t packetsCapture = 0;
    uint32_t deauthsSent = 0;
    uint32_t beaconsSent = 0;
    uint32_t bleDevicesFound = 0;

    // GPS data
    double latitude = 0.0;
    double longitude = 0.0;
    double altitude = 0.0;
    uint8_t satellites = 0;

    // Methods
    void loadSettings();
    void saveSettings();
    void resetSettings();
    void applyProfile(Profile profile);
    ThemeColors getThemeColors();

    // Activity tracking
    void recordActivity() {
        lastActivityTime = millis();
    }

private:
    Preferences prefs;
};

// Global system state (defined in main.cpp)
extern SystemState g_systemState;

// ============================================================================
// THEME DEFINITIONS
// ============================================================================

const ThemeColors THEME_HACKER = {
    .bgPrimary = 0x0000,      // Black
    .bgSecondary = 0x0841,    // Dark gray
    .textPrimary = 0x07E0,    // Bright green
    .textSecondary = 0x03E0,  // Dark green
    .accent = 0x07FF,         // Cyan
    .warning = 0xFBE0,        // Yellow
    .error = 0xF800,          // Red
    .success = 0x07E0         // Green
};

const ThemeColors THEME_CYBERPUNK = {
    .bgPrimary = 0x0000,
    .bgSecondary = 0x1082,
    .textPrimary = 0x07FF,    // Cyan
    .textSecondary = 0xF81F,  // Magenta
    .accent = 0xFFE0,         // Yellow
    .warning = 0xFBE0,
    .error = 0xF800,
    .success = 0x07E0
};

const ThemeColors THEME_STEALTH = {
    .bgPrimary = 0x0000,
    .bgSecondary = 0x0841,
    .textPrimary = 0x6B6D,    // Gray
    .textSecondary = 0x4228,  // Darker gray
    .accent = 0x6B6D,
    .warning = 0x6B6D,
    .error = 0x6B6D,
    .success = 0x6B6D
};

const ThemeColors THEME_RETRO = {
    .bgPrimary = 0x0000,
    .bgSecondary = 0x1082,
    .textPrimary = 0xFC00,    // Amber
    .textSecondary = 0x8400,  // Dark amber
    .accent = 0xFE60,
    .warning = 0xFC00,
    .error = 0xF800,
    .success = 0xFC00
};

const ThemeColors THEME_BLOOD = {
    .bgPrimary = 0x0000,
    .bgSecondary = 0x1000,
    .textPrimary = 0xF800,    // Red
    .textSecondary = 0x8000,  // Dark red
    .accent = 0xFBE0,
    .warning = 0xFBE0,
    .error = 0xF800,
    .success = 0x07E0
};

const ThemeColors THEME_OCEAN = {
    .bgPrimary = 0x0010,
    .bgSecondary = 0x0018,
    .textPrimary = 0x07FF,    // Cyan
    .textSecondary = 0x041F,  // Blue
    .accent = 0x07FF,
    .warning = 0xFBE0,
    .error = 0xF800,
    .success = 0x07E0
};

#endif // SHITBIRD_SYSTEM_H
