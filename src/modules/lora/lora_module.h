/**
 * ShitBird Firmware - LoRa Module
 * LoRa radio operations, Meshtastic/MeshCore sniffing, packet analysis
 */

#ifndef SHITBIRD_LORA_MODULE_H
#define SHITBIRD_LORA_MODULE_H

#include <Arduino.h>
#include <RadioLib.h>
#include <vector>
#include "config.h"

// LoRa Operation Modes
enum class LoRaMode {
    IDLE,
    SCANNING,
    RECEIVING,
    TRANSMITTING,
    MESHTASTIC_SNIFF,
    MESHCORE_SNIFF,
    FREQUENCY_ANALYZER
};

// Packet Types
enum class LoRaPacketType {
    UNKNOWN,
    MESHTASTIC,
    MESHCORE,
    LORAWAN,
    RAW
};

// Received Packet
struct LoRaPacket {
    uint32_t timestamp;
    float frequency;
    float rssi;
    float snr;
    uint16_t length;
    std::vector<uint8_t> data;
    LoRaPacketType type;
    bool decoded;
    String decodedText;

    // Meshtastic specific
    uint32_t meshFrom;
    uint32_t meshTo;
    uint8_t meshPortNum;
    uint8_t meshHopLimit;
    bool meshWantAck;
};

// Frequency Scan Result
struct FrequencyScanResult {
    float frequency;
    float rssi;
    bool hasSignal;
};

// Meshtastic Node Info
struct MeshtasticNode {
    uint32_t nodeId;
    String longName;
    String shortName;
    float lastLat;
    float lastLon;
    int32_t lastRssi;
    uint32_t lastSeen;
    uint8_t hopLimit;
    bool isGateway;
};

class LoRaModule {
public:
    static void init();
    static void update();
    static void deinit();

    // Configuration
    static void setFrequency(float freq);
    static void setBandwidth(float bw);
    static void setSpreadingFactor(uint8_t sf);
    static void setCodingRate(uint8_t cr);
    static void setSyncWord(uint8_t sw);
    static void setTxPower(int8_t power);

    static float getFrequency();
    static float getBandwidth();
    static uint8_t getSpreadingFactor();

    // Reception
    static void startReceive();
    static void stopReceive();
    static bool isReceiving();
    static bool hasPacket();
    static LoRaPacket getLastPacket();
    static std::vector<LoRaPacket>& getPacketHistory();
    static void clearPacketHistory();

    // Transmission
    static bool transmit(const uint8_t* data, size_t len);
    static bool transmitString(const String& str);

    // Meshtastic Sniffing
    static void startMeshtasticSniff();
    static void stopMeshtasticSniff();
    static bool isMeshtasticSniffing();
    static std::vector<MeshtasticNode>& getMeshtasticNodes();
    static void setMeshtasticKey(const uint8_t* key, size_t len);

    // Meshtastic Node Functions (legitimate communication)
    static void setNodeId(uint32_t id);
    static uint32_t getNodeId();
    static void setNodeName(const String& longName, const String& shortName);
    static bool sendMeshtasticText(const String& message, uint32_t destNode = 0xFFFFFFFF);
    static bool sendMeshtasticPosition(float lat, float lon, int32_t altitude = 0);
    static bool sendMeshtasticNodeInfo();
    static void setChannelPSK(const uint8_t* psk, size_t len);
    static void setDefaultChannel();  // Uses default "AQ==" key

    // MeshCore Sniffing
    static void startMeshCoreSniff();
    static void stopMeshCoreSniff();
    static bool isMeshCoreSniffing();

    // Frequency Analysis
    static void startFrequencyAnalyzer(float startFreq, float endFreq, float step);
    static void stopFrequencyAnalyzer();
    static bool isAnalyzing();
    static std::vector<FrequencyScanResult>& getFrequencyResults();

    // Scanning for active frequencies
    static void startScan();
    static void stopScan();
    static bool isScanning();

    // Packet Analysis
    static LoRaPacketType identifyPacket(const uint8_t* data, size_t len);
    static bool decodeMeshtasticPacket(LoRaPacket& packet);
    static bool decodeMeshCorePacket(LoRaPacket& packet);
    static String packetToHex(const uint8_t* data, size_t len);

    // Replay
    static bool replayPacket(const LoRaPacket& packet);

    // Signal Info
    static float getLastRSSI();
    static float getLastSNR();

    // Export
    static bool exportPackets(const char* filename);

    // Meshtastic Presets
    static void setMeshtasticLongFast();    // Default US preset
    static void setMeshtasticShortFast();
    static void setMeshtasticLongSlow();
    static void setMeshtasticMediumFast();

    // Menu integration
    static void buildMenu(void* menuScreen);

private:
    static SX1262* radio;
    static bool initialized;
    static LoRaMode currentMode;

    static volatile bool receivedFlag;
    static volatile bool transmittedFlag;

    static LoRaPacket lastPacket;
    static std::vector<LoRaPacket> packetHistory;
    static std::vector<MeshtasticNode> meshtasticNodes;
    static std::vector<FrequencyScanResult> frequencyResults;

    static float currentFrequency;
    static float currentBandwidth;
    static uint8_t currentSF;
    static uint8_t currentCR;
    static uint8_t currentSyncWord;
    static int8_t currentTxPower;

    static uint8_t meshtasticKey[32];
    static bool hasMeshtasticKey;

    // Node identity
    static uint32_t myNodeId;
    static String myLongName;
    static String myShortName;
    static uint32_t packetIdCounter;

    static TaskHandle_t scanTaskHandle;
    static TaskHandle_t analyzerTaskHandle;

    // Callbacks
    static void setFlag();
    static void receiveCallback();

    // Tasks
    static void scanTask(void* param);
    static void analyzerTask(void* param);

    // Internal helpers
    static void processReceivedPacket();
    static void updateMeshtasticNode(const LoRaPacket& packet);
};

// ============================================================================
// Meshtastic Protocol Constants
// ============================================================================

namespace Meshtastic {
    // Channel presets (US 915MHz)
    const float FREQ_LONG_FAST = 906.875;
    const float FREQ_SHORT_FAST = 906.875;
    const float FREQ_LONG_SLOW = 906.875;

    const float BW_LONG_FAST = 250.0;
    const float BW_SHORT_FAST = 250.0;
    const float BW_LONG_SLOW = 125.0;

    const uint8_t SF_LONG_FAST = 11;
    const uint8_t SF_SHORT_FAST = 7;
    const uint8_t SF_LONG_SLOW = 12;

    const uint8_t CR_DEFAULT = 5;  // 4/5

    // Sync word for Meshtastic
    const uint8_t SYNC_WORD = 0x2B;

    // Default encryption key (AQ==) - base64 decoded
    const uint8_t DEFAULT_KEY[] = {0x01};

    // Packet header structure
    struct PacketHeader {
        uint32_t dest;
        uint32_t sender;
        uint32_t packetId;
        uint8_t flags;
        uint8_t channelHash;
    } __attribute__((packed));

    // Port numbers
    const uint8_t PORT_TEXT_MESSAGE = 1;
    const uint8_t PORT_POSITION = 3;
    const uint8_t PORT_NODEINFO = 4;
    const uint8_t PORT_ROUTING = 5;
    const uint8_t PORT_TELEMETRY = 67;
}

// ============================================================================
// MeshCore Protocol Constants
// ============================================================================

namespace MeshCore {
    // MeshCore uses similar LoRa settings but different packet format
    const float DEFAULT_FREQ = 915.0;
    const float DEFAULT_BW = 125.0;
    const uint8_t DEFAULT_SF = 9;
}

// ============================================================================
// Common LoRaWAN Frequencies (for reference)
// ============================================================================

namespace LoRaWAN_US915 {
    const float UPLINK_START = 902.3;
    const float UPLINK_END = 914.9;
    const float UPLINK_STEP = 0.2;

    const float DOWNLINK_START = 923.3;
    const float DOWNLINK_END = 927.5;
    const float DOWNLINK_STEP = 0.6;
}

#endif // SHITBIRD_LORA_MODULE_H
