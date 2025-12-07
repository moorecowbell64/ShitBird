/**
 * ShitBird Firmware - WiFi Module
 * Comprehensive WiFi scanning, attacks, and analysis
 */

#ifndef SHITBIRD_WIFI_MODULE_H
#define SHITBIRD_WIFI_MODULE_H

#include <Arduino.h>
#include <WiFi.h>
#include <esp_wifi.h>
#include <vector>
#include <map>
#include "config.h"

// WiFi Attack Types
enum class WiFiAttackType {
    NONE,
    DEAUTH_FLOOD,         // Broadcast deauth to all clients
    DEAUTH_TARGETED,      // Deauth specific client(s)
    BEACON_SPAM_LIST,     // Beacon spam from list
    BEACON_SPAM_RANDOM,   // Random SSID beacon spam
    BEACON_SPAM_CLONE,    // Clone existing networks
    PROBE_FLOOD,          // Probe request flood
    EVIL_PORTAL,          // Captive portal
    PMKID_ATTACK,         // PMKID capture
    HANDSHAKE_CAPTURE,    // 4-way handshake capture
    RICKROLL_BEACON       // Rick Roll SSIDs
};

// WiFi Operation Mode
enum class WiFiOpMode {
    IDLE,
    SCANNING,
    MONITOR,
    ATTACK,
    AP_MODE,
    STATION
};

// Access Point Info
struct APInfo {
    String ssid;
    String bssid;
    int32_t rssi;
    uint8_t channel;
    wifi_auth_mode_t encryption;
    bool isHidden;
    uint32_t lastSeen;
    uint16_t clientCount;
    bool selected;  // For targeting

    // WPA info
    bool hasWPA;
    bool hasWPA2;
    bool hasWPA3;
    bool hasWPS;

    // Captured data
    bool pmkidCaptured;
    bool handshakeCaptured;
};

// Client/Station Info
struct ClientInfo {
    String mac;
    String apBssid;  // Associated AP
    int32_t rssi;
    uint32_t lastSeen;
    uint16_t probeCount;
    std::vector<String> probedSSIDs;
    bool selected;
};

// Deauth Packet
struct DeauthPacket {
    uint8_t receiverAddr[6];
    uint8_t transmitterAddr[6];
    uint8_t bssid[6];
    uint16_t reason;
};

// Beacon Frame Info
struct BeaconInfo {
    String ssid;
    uint8_t bssid[6];
    uint8_t channel;
    bool hidden;
    wifi_auth_mode_t auth;
};

// PCAP Packet
struct WiFiPacket {
    uint32_t timestamp;
    uint32_t microseconds;
    uint16_t length;
    int8_t rssi;
    uint8_t channel;
    std::vector<uint8_t> data;
};

// Evil Portal Credential
struct CapturedCredential {
    String ssid;
    String username;
    String password;
    String userAgent;
    String ip;
    uint32_t timestamp;
};

class WiFiModule {
public:
    static void init();
    static void update();
    static void deinit();

    // Scanning
    static void startScan(bool passive = false);
    static void stopScan();
    static bool isScanning();
    static std::vector<APInfo>& getAccessPoints();
    static std::vector<ClientInfo>& getClients();
    static void clearResults();

    // Channel hopping
    static void setChannel(uint8_t channel);
    static uint8_t getChannel();
    static void startChannelHop();
    static void stopChannelHop();

    // Monitor mode
    static void startMonitor();
    static void stopMonitor();
    static bool isMonitoring();

    // Deauth attacks
    static void startDeauthFlood(const String& bssid);
    static void startDeauthTargeted(const String& bssid, const String& clientMac);
    static void startDeauthAll();  // All selected targets
    static void stopDeauth();
    static bool isDeauthing();
    static uint32_t getDeauthCount();

    // Beacon attacks
    static void startBeaconSpam(const std::vector<String>& ssids);
    static void startBeaconSpamRandom(uint8_t count = 50);
    static void startBeaconClone();
    static void startRickrollBeacon();
    static void stopBeaconSpam();
    static bool isBeaconSpamming();
    static uint32_t getBeaconCount();

    // Probe flood
    static void startProbeFlood();
    static void stopProbeFlood();

    // Evil Portal
    static void startEvilPortal(const String& ssid, const String& templateName = "default");
    static void stopEvilPortal();
    static bool isEvilPortalActive();
    static std::vector<CapturedCredential>& getCapturedCredentials();

    // Handshake/PMKID capture
    static void startHandshakeCapture(const String& bssid);
    static void stopHandshakeCapture();
    static bool isCapturing();
    static bool hasHandshake();
    static bool hasPMKID();

    // PCAP operations
    static void startPcapCapture(const char* filename);
    static void stopPcapCapture();
    static bool isPcapCapturing();
    static uint32_t getPcapPacketCount();

    // Target selection
    static void selectAP(int index, bool selected = true);
    static void selectClient(int index, bool selected = true);
    static void selectAllAPs(bool selected = true);
    static void clearSelection();
    static std::vector<APInfo*> getSelectedAPs();
    static std::vector<ClientInfo*> getSelectedClients();

    // Utility
    static String macToString(const uint8_t* mac);
    static void stringToMac(const String& str, uint8_t* mac);
    static String getEncryptionString(wifi_auth_mode_t auth);
    static String getVendor(const String& mac);

    // Menu integration
    static void buildMenu(void* menuScreen);

private:
    static bool initialized;
    static WiFiOpMode currentMode;
    static WiFiAttackType currentAttack;

    static std::vector<APInfo> accessPoints;
    static std::vector<ClientInfo> clients;
    static std::vector<WiFiPacket> capturedPackets;
    static std::vector<CapturedCredential> credentials;
    static std::vector<String> beaconSSIDs;

    static uint8_t currentChannel;
    static bool channelHopping;
    static bool monitoring;
    static bool deauthing;
    static bool beaconSpamming;
    static bool pcapCapturing;
    static bool handshakeCapturing;

    static uint32_t deauthCount;
    static uint32_t beaconCount;
    static uint32_t packetCount;

    static String targetBSSID;
    static String targetClientMAC;
    static String pcapFilename;

    static TaskHandle_t attackTaskHandle;
    static TaskHandle_t channelHopTaskHandle;
    // pcapFile removed - use Storage class directly

    // Promiscuous mode callback
    static void promiscuousCallback(void* buf, wifi_promiscuous_pkt_type_t type);

    // Attack tasks
    static void deauthTask(void* param);
    static void beaconTask(void* param);
    static void channelHopTask(void* param);

    // Packet crafting
    static void sendDeauthPacket(const uint8_t* ap, const uint8_t* client, uint16_t reason);
    static void sendBeaconPacket(const BeaconInfo& beacon);
    static void sendProbeRequest(const String& ssid);

    // Packet parsing
    static void parseManagementFrame(const wifi_promiscuous_pkt_t* pkt);
    static void parseBeacon(const uint8_t* payload, int len, int rssi, uint8_t channel);
    static void parseProbeResponse(const uint8_t* payload, int len, int rssi);
    static void parseProbeRequest(const uint8_t* payload, int len, int rssi);
    static void parseDeauth(const uint8_t* payload, int len);
    static void parseEAPOL(const uint8_t* payload, int len);

    // PCAP writing
    static void writePcapPacket(const wifi_promiscuous_pkt_t* pkt);
};

// ============================================================================
// Rick Roll SSIDs
// ============================================================================

// Rickroll SSIDs defined in wifi_module.cpp
extern const char* RICKROLL_SSIDS[];
extern const int RICKROLL_COUNT;

// ============================================================================
// Frame Types
// ============================================================================

#define WIFI_PKT_MGMT       0
#define WIFI_PKT_CTRL       1
#define WIFI_PKT_DATA       2

#define WIFI_MGMT_ASSOC_REQ     0x00
#define WIFI_MGMT_ASSOC_RESP    0x10
#define WIFI_MGMT_REASSOC_REQ   0x20
#define WIFI_MGMT_REASSOC_RESP  0x30
#define WIFI_MGMT_PROBE_REQ     0x40
#define WIFI_MGMT_PROBE_RESP    0x50
#define WIFI_MGMT_BEACON        0x80
#define WIFI_MGMT_ATIM          0x90
#define WIFI_MGMT_DISASSOC      0xA0
#define WIFI_MGMT_AUTH          0xB0
#define WIFI_MGMT_DEAUTH        0xC0
#define WIFI_MGMT_ACTION        0xD0

// Deauth reason codes
#define DEAUTH_REASON_UNSPECIFIED       1
#define DEAUTH_REASON_PREV_AUTH_INVALID 2
#define DEAUTH_REASON_LEAVING           3
#define DEAUTH_REASON_INACTIVITY        4
#define DEAUTH_REASON_AP_BUSY           5
#define DEAUTH_REASON_CLASS2_FROM_NOAUTH 6
#define DEAUTH_REASON_CLASS3_FROM_NOASSOC 7

#endif // SHITBIRD_WIFI_MODULE_H
