/**
 * ShitBird Firmware - System State Implementation
 */

#include "system.h"

void SystemState::loadSettings() {
    prefs.begin("shitbird", true);  // Read-only mode

    // Device name
    String name = prefs.getString("deviceName", FIRMWARE_NAME);
    strncpy(settings.deviceName, name.c_str(), sizeof(settings.deviceName) - 1);

    // Active profile
    settings.activeProfile = (Profile)prefs.getUChar("profile", (uint8_t)Profile::RECON_ONLY);

    // WiFi settings
    settings.wifi.enabled = prefs.getBool("wifi_en", true);
    settings.wifi.defaultChannel = prefs.getUChar("wifi_ch", 1);
    settings.wifi.deauthInterval = prefs.getUShort("wifi_deauth_int", WIFI_DEAUTH_INTERVAL);
    settings.wifi.beaconInterval = prefs.getUShort("wifi_beacon_int", WIFI_BEACON_INTERVAL);
    settings.wifi.captureHandshakes = prefs.getBool("wifi_cap_hs", true);
    settings.wifi.autoSavePcap = prefs.getBool("wifi_auto_pcap", true);

    // BLE settings
    settings.ble.enabled = prefs.getBool("ble_en", true);
    settings.ble.scanDuration = prefs.getUChar("ble_scan_dur", BLE_SCAN_DURATION);
    settings.ble.spamInterval = prefs.getUShort("ble_spam_int", BLE_SPAM_INTERVAL);
    settings.ble.autoEnumerate = prefs.getBool("ble_auto_enum", false);

    // LoRa settings
    settings.lora.enabled = prefs.getBool("lora_en", true);
    settings.lora.frequency = prefs.getFloat("lora_freq", LORA_FREQUENCY);
    settings.lora.bandwidth = prefs.getFloat("lora_bw", LORA_BANDWIDTH);
    settings.lora.spreadFactor = prefs.getUChar("lora_sf", LORA_SPREAD_FACTOR);
    settings.lora.codingRate = prefs.getUChar("lora_cr", LORA_CODING_RATE);
    settings.lora.txPower = prefs.getChar("lora_pwr", LORA_TX_POWER);
    settings.lora.meshtasticMode = prefs.getBool("lora_mesh", true);

    // IR settings
    settings.ir.enabled = prefs.getBool("ir_en", true);
    settings.ir.txPin = prefs.getUChar("ir_tx", IR_TX_PIN);
    settings.ir.rxPin = prefs.getUChar("ir_rx", IR_RX_PIN);
    settings.ir.learningMode = prefs.getBool("ir_learn", false);

    // Audio settings
    settings.audio.enabled = prefs.getBool("audio_en", true);
    settings.audio.volume = prefs.getUChar("audio_vol", 50);
    settings.audio.keyClickSound = prefs.getBool("audio_click", true);
    settings.audio.alertSounds = prefs.getBool("audio_alert", true);

    // Security settings
    settings.security.pinEnabled = prefs.getBool("sec_pin_en", false);
    String pin = prefs.getString("sec_pin", "000000");
    strncpy(settings.security.pin, pin.c_str(), SECURITY_PIN_LENGTH);
    settings.security.maxAttempts = prefs.getUChar("sec_max_att", SECURITY_MAX_ATTEMPTS);
    settings.security.autoLockEnabled = prefs.getBool("sec_auto_lock", false);
    settings.security.autoLockTimeout = prefs.getUShort("sec_lock_time", AUTO_LOCK_TIMEOUT);
    settings.security.encryptLogs = prefs.getBool("sec_encrypt", ENCRYPT_SD_LOGS);
    settings.security.panicWipeEnabled = prefs.getBool("sec_panic_en", false);
    String panic = prefs.getString("sec_panic_seq", "****");
    strncpy(settings.security.panicSequence, panic.c_str(), sizeof(settings.security.panicSequence) - 1);

    // Display settings
    settings.display.brightness = prefs.getUChar("disp_bright", 200);
    settings.display.sleepTimeout = prefs.getUChar("disp_sleep", 60);
    settings.display.theme = (Theme)prefs.getUChar("disp_theme", (uint8_t)Theme::HACKER);
    settings.display.animationsEnabled = prefs.getBool("disp_anim", true);

    // Custom theme colors
    settings.display.customColors.bgPrimary = prefs.getUShort("theme_bg1", 0x0000);
    settings.display.customColors.bgSecondary = prefs.getUShort("theme_bg2", 0x0841);
    settings.display.customColors.textPrimary = prefs.getUShort("theme_txt1", 0x07E0);
    settings.display.customColors.textSecondary = prefs.getUShort("theme_txt2", 0x03E0);
    settings.display.customColors.accent = prefs.getUShort("theme_accent", 0x07FF);
    settings.display.customColors.warning = prefs.getUShort("theme_warn", 0xFBE0);
    settings.display.customColors.error = prefs.getUShort("theme_err", 0xF800);
    settings.display.customColors.success = prefs.getUShort("theme_succ", 0x07E0);

    prefs.end();

    Serial.println("[SYSTEM] Settings loaded");
}

void SystemState::saveSettings() {
    prefs.begin("shitbird", false);  // Read-write mode

    // Device name
    prefs.putString("deviceName", settings.deviceName);

    // Active profile
    prefs.putUChar("profile", (uint8_t)settings.activeProfile);

    // WiFi settings
    prefs.putBool("wifi_en", settings.wifi.enabled);
    prefs.putUChar("wifi_ch", settings.wifi.defaultChannel);
    prefs.putUShort("wifi_deauth_int", settings.wifi.deauthInterval);
    prefs.putUShort("wifi_beacon_int", settings.wifi.beaconInterval);
    prefs.putBool("wifi_cap_hs", settings.wifi.captureHandshakes);
    prefs.putBool("wifi_auto_pcap", settings.wifi.autoSavePcap);

    // BLE settings
    prefs.putBool("ble_en", settings.ble.enabled);
    prefs.putUChar("ble_scan_dur", settings.ble.scanDuration);
    prefs.putUShort("ble_spam_int", settings.ble.spamInterval);
    prefs.putBool("ble_auto_enum", settings.ble.autoEnumerate);

    // LoRa settings
    prefs.putBool("lora_en", settings.lora.enabled);
    prefs.putFloat("lora_freq", settings.lora.frequency);
    prefs.putFloat("lora_bw", settings.lora.bandwidth);
    prefs.putUChar("lora_sf", settings.lora.spreadFactor);
    prefs.putUChar("lora_cr", settings.lora.codingRate);
    prefs.putChar("lora_pwr", settings.lora.txPower);
    prefs.putBool("lora_mesh", settings.lora.meshtasticMode);

    // IR settings
    prefs.putBool("ir_en", settings.ir.enabled);
    prefs.putUChar("ir_tx", settings.ir.txPin);
    prefs.putUChar("ir_rx", settings.ir.rxPin);
    prefs.putBool("ir_learn", settings.ir.learningMode);

    // Audio settings
    prefs.putBool("audio_en", settings.audio.enabled);
    prefs.putUChar("audio_vol", settings.audio.volume);
    prefs.putBool("audio_click", settings.audio.keyClickSound);
    prefs.putBool("audio_alert", settings.audio.alertSounds);

    // Security settings
    prefs.putBool("sec_pin_en", settings.security.pinEnabled);
    prefs.putString("sec_pin", settings.security.pin);
    prefs.putUChar("sec_max_att", settings.security.maxAttempts);
    prefs.putBool("sec_auto_lock", settings.security.autoLockEnabled);
    prefs.putUShort("sec_lock_time", settings.security.autoLockTimeout);
    prefs.putBool("sec_encrypt", settings.security.encryptLogs);
    prefs.putBool("sec_panic_en", settings.security.panicWipeEnabled);
    prefs.putString("sec_panic_seq", settings.security.panicSequence);

    // Display settings
    prefs.putUChar("disp_bright", settings.display.brightness);
    prefs.putUChar("disp_sleep", settings.display.sleepTimeout);
    prefs.putUChar("disp_theme", (uint8_t)settings.display.theme);
    prefs.putBool("disp_anim", settings.display.animationsEnabled);

    // Custom theme colors
    prefs.putUShort("theme_bg1", settings.display.customColors.bgPrimary);
    prefs.putUShort("theme_bg2", settings.display.customColors.bgSecondary);
    prefs.putUShort("theme_txt1", settings.display.customColors.textPrimary);
    prefs.putUShort("theme_txt2", settings.display.customColors.textSecondary);
    prefs.putUShort("theme_accent", settings.display.customColors.accent);
    prefs.putUShort("theme_warn", settings.display.customColors.warning);
    prefs.putUShort("theme_err", settings.display.customColors.error);
    prefs.putUShort("theme_succ", settings.display.customColors.success);

    prefs.end();

    Serial.println("[SYSTEM] Settings saved");
}

void SystemState::resetSettings() {
    prefs.begin("shitbird", false);
    prefs.clear();
    prefs.end();

    loadSettings();  // Load defaults
    Serial.println("[SYSTEM] Settings reset to defaults");
}

void SystemState::applyProfile(Profile profile) {
    settings.activeProfile = profile;

    switch (profile) {
        case Profile::RECON_ONLY:
            // Passive mode - no transmissions
            settings.wifi.enabled = true;
            settings.ble.enabled = true;
            settings.lora.enabled = true;
            // All modules in receive-only mode
            break;

        case Profile::WIFI_ASSESSMENT:
            settings.wifi.enabled = true;
            settings.ble.enabled = false;
            settings.lora.enabled = false;
            settings.ir.enabled = false;
            break;

        case Profile::BLE_HUNT:
            settings.wifi.enabled = false;
            settings.ble.enabled = true;
            settings.ble.autoEnumerate = true;
            settings.lora.enabled = false;
            settings.ir.enabled = false;
            break;

        case Profile::PHYSICAL_SECURITY:
            settings.wifi.enabled = false;
            settings.ble.enabled = false;
            settings.lora.enabled = false;
            settings.ir.enabled = true;
            break;

        case Profile::STEALTH_MODE:
            // Minimal RF, logging only
            settings.wifi.enabled = true;  // Passive only
            settings.ble.enabled = true;   // Passive only
            settings.lora.enabled = true;  // Passive only
            settings.ir.enabled = false;
            settings.audio.enabled = false;
            settings.display.brightness = 50;
            break;

        case Profile::FULL_ASSAULT:
            settings.wifi.enabled = true;
            settings.ble.enabled = true;
            settings.lora.enabled = true;
            settings.ir.enabled = true;
            break;

        case Profile::CUSTOM:
            // Don't modify - use current settings
            break;
    }

    saveSettings();
    Serial.printf("[SYSTEM] Profile applied: %d\n", (int)profile);
}

ThemeColors SystemState::getThemeColors() {
    switch (settings.display.theme) {
        case Theme::HACKER:
            return THEME_HACKER;
        case Theme::CYBERPUNK:
            return THEME_CYBERPUNK;
        case Theme::STEALTH:
            return THEME_STEALTH;
        case Theme::RETRO:
            return THEME_RETRO;
        case Theme::BLOOD:
            return THEME_BLOOD;
        case Theme::OCEAN:
            return THEME_OCEAN;
        case Theme::CUSTOM:
            return settings.display.customColors;
        default:
            return THEME_HACKER;
    }
}
