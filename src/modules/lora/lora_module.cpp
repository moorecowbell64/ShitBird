/**
 * ShitBird Firmware - LoRa Module Implementation
 */

#include "lora_module.h"
#include "../../core/system.h"
#include "../../core/storage.h"
#include "../../ui/ui_manager.h"
#include <SPI.h>

#if ENABLE_GPS
#include "../gps/gps_module.h"
#endif

// Static member initialization
SX1262* LoRaModule::radio = nullptr;
bool LoRaModule::initialized = false;
LoRaMode LoRaModule::currentMode = LoRaMode::IDLE;

volatile bool LoRaModule::receivedFlag = false;
volatile bool LoRaModule::transmittedFlag = false;

LoRaPacket LoRaModule::lastPacket;
std::vector<LoRaPacket> LoRaModule::packetHistory;
std::vector<MeshtasticNode> LoRaModule::meshtasticNodes;
std::vector<FrequencyScanResult> LoRaModule::frequencyResults;

float LoRaModule::currentFrequency = LORA_FREQUENCY;
float LoRaModule::currentBandwidth = LORA_BANDWIDTH;
uint8_t LoRaModule::currentSF = LORA_SPREAD_FACTOR;
uint8_t LoRaModule::currentCR = LORA_CODING_RATE;
uint8_t LoRaModule::currentSyncWord = LORA_SYNC_WORD;
int8_t LoRaModule::currentTxPower = LORA_TX_POWER;

uint8_t LoRaModule::meshtasticKey[32] = {0};
bool LoRaModule::hasMeshtasticKey = false;

// Node identity - generate from MAC
uint32_t LoRaModule::myNodeId = 0;
String LoRaModule::myLongName = "ShitBird";
String LoRaModule::myShortName = "SBIRD";
uint32_t LoRaModule::packetIdCounter = 0;

TaskHandle_t LoRaModule::scanTaskHandle = nullptr;
TaskHandle_t LoRaModule::analyzerTaskHandle = nullptr;

void LoRaModule::setFlag() {
    receivedFlag = true;
}

void LoRaModule::init() {
    if (initialized) return;

    Serial.println("[LORA] Initializing SX1262...");

    // Ensure all SPI chip selects are HIGH before reinitializing SPI
    // This is critical for shared SPI bus on T-Deck
    pinMode(LORA_CS_PIN, OUTPUT);
    pinMode(SD_CS_PIN, OUTPUT);
    pinMode(TFT_CS_PIN, OUTPUT);
    digitalWrite(LORA_CS_PIN, HIGH);
    digitalWrite(SD_CS_PIN, HIGH);
    digitalWrite(TFT_CS_PIN, HIGH);

    // Reinitialize SPI with correct pins (shared bus)
    SPI.end();
    SPI.begin(LORA_SCLK_PIN, LORA_MISO_PIN, LORA_MOSI_PIN);

    // Create SX1262 instance using RadioLib with default SPI
    Module* mod = new Module(LORA_CS_PIN, LORA_DIO1_PIN, LORA_RST_PIN, LORA_BUSY_PIN);
    radio = new SX1262(mod);

    // Basic initialization without parameters (T-Deck style)
    int state = radio->begin();

    if (state != RADIOLIB_ERR_NONE) {
        Serial.printf("[LORA] Init failed, code: %d\n", state);
        return;
    }

    // Configure radio parameters individually (as per official examples)
    state = radio->setFrequency(currentFrequency);
    if (state != RADIOLIB_ERR_NONE) {
        Serial.printf("[LORA] setFrequency failed: %d\n", state);
    }

    state = radio->setBandwidth(currentBandwidth);
    if (state != RADIOLIB_ERR_NONE) {
        Serial.printf("[LORA] setBandwidth failed: %d\n", state);
    }

    state = radio->setSpreadingFactor(currentSF);
    if (state != RADIOLIB_ERR_NONE) {
        Serial.printf("[LORA] setSpreadingFactor failed: %d\n", state);
    }

    state = radio->setCodingRate(currentCR);
    if (state != RADIOLIB_ERR_NONE) {
        Serial.printf("[LORA] setCodingRate failed: %d\n", state);
    }

    state = radio->setSyncWord(currentSyncWord);
    if (state != RADIOLIB_ERR_NONE) {
        Serial.printf("[LORA] setSyncWord failed: %d\n", state);
    }

    state = radio->setOutputPower(currentTxPower);
    if (state != RADIOLIB_ERR_NONE) {
        Serial.printf("[LORA] setOutputPower failed: %d\n", state);
    }

    state = radio->setCurrentLimit(140.0);
    if (state != RADIOLIB_ERR_NONE) {
        Serial.printf("[LORA] setCurrentLimit failed: %d\n", state);
    }

    state = radio->setPreambleLength(15);
    if (state != RADIOLIB_ERR_NONE) {
        Serial.printf("[LORA] setPreambleLength failed: %d\n", state);
    }

    // Set DIO1 as interrupt for RX done
    radio->setDio1Action(setFlag);

    // Configure for best sensitivity
    radio->setRxBoostedGainMode(true);

    initialized = true;
    g_systemState.loraActive = true;

    Serial.printf("[LORA] Initialized: %.3f MHz, BW: %.1f kHz, SF: %d\n",
                  currentFrequency, currentBandwidth, currentSF);
}

void LoRaModule::update() {
    if (!initialized) return;

    // Check for received packets
    if (receivedFlag) {
        receivedFlag = false;
        processReceivedPacket();
    }
}

void LoRaModule::deinit() {
    if (!initialized) return;

    stopReceive();
    stopScan();
    stopFrequencyAnalyzer();

    radio->sleep();
    delete radio;
    radio = nullptr;

    initialized = false;
    g_systemState.loraActive = false;
}

// ============================================================================
// Configuration
// ============================================================================

void LoRaModule::setFrequency(float freq) {
    if (!initialized) return;
    currentFrequency = freq;
    radio->setFrequency(freq);
    Serial.printf("[LORA] Frequency set: %.3f MHz\n", freq);
}

void LoRaModule::setBandwidth(float bw) {
    if (!initialized) return;
    currentBandwidth = bw;
    radio->setBandwidth(bw);
    Serial.printf("[LORA] Bandwidth set: %.1f kHz\n", bw);
}

void LoRaModule::setSpreadingFactor(uint8_t sf) {
    if (!initialized) return;
    currentSF = sf;
    radio->setSpreadingFactor(sf);
    Serial.printf("[LORA] SF set: %d\n", sf);
}

void LoRaModule::setCodingRate(uint8_t cr) {
    if (!initialized) return;
    currentCR = cr;
    radio->setCodingRate(cr);
}

void LoRaModule::setSyncWord(uint8_t sw) {
    if (!initialized) return;
    currentSyncWord = sw;
    radio->setSyncWord(sw);
}

void LoRaModule::setTxPower(int8_t power) {
    if (!initialized) return;
    currentTxPower = power;
    radio->setOutputPower(power);
}

float LoRaModule::getFrequency() { return currentFrequency; }
float LoRaModule::getBandwidth() { return currentBandwidth; }
uint8_t LoRaModule::getSpreadingFactor() { return currentSF; }

// ============================================================================
// Reception
// ============================================================================

void LoRaModule::startReceive() {
    if (!initialized || currentMode == LoRaMode::RECEIVING) return;

    Serial.println("[LORA] Starting receive mode...");
    currentMode = LoRaMode::RECEIVING;
    g_systemState.currentMode = OperationMode::LORA_SCAN;

    int state = radio->startReceive();
    if (state != RADIOLIB_ERR_NONE) {
        Serial.printf("[LORA] Failed to start receive: %d\n", state);
        currentMode = LoRaMode::IDLE;
    }

    Storage::log("lora", "Receive mode started");
}

void LoRaModule::stopReceive() {
    if (currentMode != LoRaMode::RECEIVING &&
        currentMode != LoRaMode::MESHTASTIC_SNIFF &&
        currentMode != LoRaMode::MESHCORE_SNIFF) return;

    Serial.println("[LORA] Stopping receive mode");
    radio->standby();
    currentMode = LoRaMode::IDLE;

    if (g_systemState.currentMode == OperationMode::LORA_SCAN) {
        g_systemState.currentMode = OperationMode::IDLE;
    }
}

bool LoRaModule::isReceiving() {
    return currentMode == LoRaMode::RECEIVING ||
           currentMode == LoRaMode::MESHTASTIC_SNIFF ||
           currentMode == LoRaMode::MESHCORE_SNIFF;
}

bool LoRaModule::hasPacket() {
    return !packetHistory.empty();
}

LoRaPacket LoRaModule::getLastPacket() {
    return lastPacket;
}

std::vector<LoRaPacket>& LoRaModule::getPacketHistory() {
    return packetHistory;
}

void LoRaModule::clearPacketHistory() {
    packetHistory.clear();
}

void LoRaModule::processReceivedPacket() {
    if (!initialized) return;

    // Get packet data
    size_t len = radio->getPacketLength();
    if (len == 0) {
        radio->startReceive();
        return;
    }

    uint8_t* data = new uint8_t[len];
    int state = radio->readData(data, len);

    if (state == RADIOLIB_ERR_NONE) {
        LoRaPacket packet;
        packet.timestamp = millis();
        packet.frequency = currentFrequency;
        packet.rssi = radio->getRSSI();
        packet.snr = radio->getSNR();
        packet.length = len;
        packet.data.assign(data, data + len);
        packet.decoded = false;

        // Identify packet type
        packet.type = identifyPacket(data, len);

        // Try to decode
        if (packet.type == LoRaPacketType::MESHTASTIC) {
            decodeMeshtasticPacket(packet);
        } else if (packet.type == LoRaPacketType::MESHCORE) {
            decodeMeshCorePacket(packet);
        }

        lastPacket = packet;
        packetHistory.push_back(packet);

        // Keep history limited
        if (packetHistory.size() > 100) {
            packetHistory.erase(packetHistory.begin());
        }

        Serial.printf("[LORA] Received %d bytes, RSSI: %.1f, SNR: %.1f\n",
                      len, packet.rssi, packet.snr);

        Storage::logf("lora", "RX: %d bytes, RSSI: %.1f, Type: %d",
                      len, packet.rssi, (int)packet.type);

        g_systemState.packetsCapture++;
    }

    delete[] data;

    // Resume receiving
    if (isReceiving()) {
        radio->startReceive();
    }
}

// ============================================================================
// Transmission
// ============================================================================

bool LoRaModule::transmit(const uint8_t* data, size_t len) {
    if (!initialized) return false;

    Serial.printf("[LORA] Transmitting %d bytes\n", len);
    g_systemState.currentMode = OperationMode::LORA_ATTACK;

    // RadioLib expects non-const pointer, so make a copy
    uint8_t* txBuf = new uint8_t[len];
    memcpy(txBuf, data, len);
    int state = radio->transmit(txBuf, len);
    delete[] txBuf;

    g_systemState.currentMode = OperationMode::IDLE;

    if (state == RADIOLIB_ERR_NONE) {
        Serial.println("[LORA] Transmission successful");
        Storage::logf("lora", "TX: %d bytes", len);
        return true;
    } else {
        Serial.printf("[LORA] Transmission failed: %d\n", state);
        return false;
    }
}

bool LoRaModule::transmitString(const String& str) {
    return transmit((uint8_t*)str.c_str(), str.length());
}

// ============================================================================
// Meshtastic Sniffing
// ============================================================================

void LoRaModule::startMeshtasticSniff() {
    if (!initialized) return;

    Serial.println("[LORA] Starting Meshtastic sniffing...");

    // Set Meshtastic default parameters (LongFast US)
    setMeshtasticLongFast();

    currentMode = LoRaMode::MESHTASTIC_SNIFF;
    meshtasticNodes.clear();

    radio->startReceive();

    Storage::log("lora", "Meshtastic sniffing started");
}

void LoRaModule::stopMeshtasticSniff() {
    if (currentMode != LoRaMode::MESHTASTIC_SNIFF) return;

    stopReceive();
    Serial.printf("[LORA] Meshtastic sniffing stopped, found %d nodes\n",
                  meshtasticNodes.size());

    Storage::logf("lora", "Meshtastic stopped, %d nodes found",
                  meshtasticNodes.size());
}

bool LoRaModule::isMeshtasticSniffing() {
    return currentMode == LoRaMode::MESHTASTIC_SNIFF;
}

std::vector<MeshtasticNode>& LoRaModule::getMeshtasticNodes() {
    return meshtasticNodes;
}

void LoRaModule::setMeshtasticKey(const uint8_t* key, size_t len) {
    memcpy(meshtasticKey, key, min(len, (size_t)32));
    hasMeshtasticKey = true;
}

void LoRaModule::setMeshtasticLongFast() {
    setFrequency(Meshtastic::FREQ_LONG_FAST);
    setBandwidth(Meshtastic::BW_LONG_FAST);
    setSpreadingFactor(Meshtastic::SF_LONG_FAST);
    setCodingRate(Meshtastic::CR_DEFAULT);
    setSyncWord(Meshtastic::SYNC_WORD);
}

void LoRaModule::setMeshtasticShortFast() {
    setFrequency(Meshtastic::FREQ_SHORT_FAST);
    setBandwidth(Meshtastic::BW_SHORT_FAST);
    setSpreadingFactor(Meshtastic::SF_SHORT_FAST);
    setCodingRate(Meshtastic::CR_DEFAULT);
    setSyncWord(Meshtastic::SYNC_WORD);
}

void LoRaModule::setMeshtasticLongSlow() {
    setFrequency(Meshtastic::FREQ_LONG_SLOW);
    setBandwidth(Meshtastic::BW_LONG_SLOW);
    setSpreadingFactor(Meshtastic::SF_LONG_SLOW);
    setCodingRate(Meshtastic::CR_DEFAULT);
    setSyncWord(Meshtastic::SYNC_WORD);
}

void LoRaModule::setMeshtasticMediumFast() {
    setFrequency(906.875);
    setBandwidth(250.0);
    setSpreadingFactor(9);
    setCodingRate(Meshtastic::CR_DEFAULT);
    setSyncWord(Meshtastic::SYNC_WORD);
}

// ============================================================================
// MeshCore Sniffing
// ============================================================================

void LoRaModule::startMeshCoreSniff() {
    if (!initialized) return;

    Serial.println("[LORA] Starting MeshCore sniffing...");

    setFrequency(MeshCore::DEFAULT_FREQ);
    setBandwidth(MeshCore::DEFAULT_BW);
    setSpreadingFactor(MeshCore::DEFAULT_SF);

    currentMode = LoRaMode::MESHCORE_SNIFF;
    radio->startReceive();

    Storage::log("lora", "MeshCore sniffing started");
}

void LoRaModule::stopMeshCoreSniff() {
    if (currentMode != LoRaMode::MESHCORE_SNIFF) return;
    stopReceive();
}

bool LoRaModule::isMeshCoreSniffing() {
    return currentMode == LoRaMode::MESHCORE_SNIFF;
}

// ============================================================================
// Frequency Analyzer
// ============================================================================

void LoRaModule::startFrequencyAnalyzer(float startFreq, float endFreq, float step) {
    if (!initialized || currentMode == LoRaMode::FREQUENCY_ANALYZER) return;

    Serial.printf("[LORA] Starting frequency analyzer: %.3f - %.3f MHz\n",
                  startFreq, endFreq);

    currentMode = LoRaMode::FREQUENCY_ANALYZER;
    frequencyResults.clear();

    // Store parameters for task
    static float params[3];
    params[0] = startFreq;
    params[1] = endFreq;
    params[2] = step;

    xTaskCreatePinnedToCore(
        analyzerTask,
        "LoRa_Analyzer",
        4096,
        params,
        1,
        &analyzerTaskHandle,
        0
    );

    Storage::logf("lora", "Frequency analyzer: %.3f-%.3f MHz", startFreq, endFreq);
}

void LoRaModule::stopFrequencyAnalyzer() {
    if (currentMode != LoRaMode::FREQUENCY_ANALYZER) return;

    if (analyzerTaskHandle) {
        vTaskDelete(analyzerTaskHandle);
        analyzerTaskHandle = nullptr;
    }

    currentMode = LoRaMode::IDLE;
    Serial.println("[LORA] Frequency analyzer stopped");
}

bool LoRaModule::isAnalyzing() {
    return currentMode == LoRaMode::FREQUENCY_ANALYZER;
}

std::vector<FrequencyScanResult>& LoRaModule::getFrequencyResults() {
    return frequencyResults;
}

void LoRaModule::analyzerTask(void* param) {
    float* params = (float*)param;
    float startFreq = params[0];
    float endFreq = params[1];
    float step = params[2];

    for (float freq = startFreq; freq <= endFreq && currentMode == LoRaMode::FREQUENCY_ANALYZER; freq += step) {
        radio->setFrequency(freq);
        vTaskDelay(pdMS_TO_TICKS(10));  // Let frequency settle

        // Read RSSI
        float rssi = radio->getRSSI();

        FrequencyScanResult result;
        result.frequency = freq;
        result.rssi = rssi;
        result.hasSignal = rssi > -120;  // Threshold

        frequencyResults.push_back(result);

        vTaskDelay(pdMS_TO_TICKS(50));
    }

    // Restore original frequency
    radio->setFrequency(currentFrequency);

    currentMode = LoRaMode::IDLE;
    Serial.printf("[LORA] Analyzer complete, %d results\n", frequencyResults.size());

    vTaskDelete(nullptr);
}

// ============================================================================
// Scanning
// ============================================================================

void LoRaModule::startScan() {
    if (!initialized || isScanning()) return;

    Serial.println("[LORA] Starting scan...");
    currentMode = LoRaMode::SCANNING;

    xTaskCreatePinnedToCore(
        scanTask,
        "LoRa_Scan",
        4096,
        nullptr,
        1,
        &scanTaskHandle,
        0
    );
}

void LoRaModule::stopScan() {
    if (!isScanning()) return;

    if (scanTaskHandle) {
        vTaskDelete(scanTaskHandle);
        scanTaskHandle = nullptr;
    }

    currentMode = LoRaMode::IDLE;
}

bool LoRaModule::isScanning() {
    return currentMode == LoRaMode::SCANNING;
}

void LoRaModule::scanTask(void* param) {
    // Scan common frequencies and settings
    const float frequencies[] = {915.0, 906.875, 903.08, 905.32, 907.56, 909.80};
    const uint8_t spreadFactors[] = {7, 8, 9, 10, 11, 12};

    while (currentMode == LoRaMode::SCANNING) {
        for (float freq : frequencies) {
            for (uint8_t sf : spreadFactors) {
                if (currentMode != LoRaMode::SCANNING) break;

                radio->setFrequency(freq);
                radio->setSpreadingFactor(sf);

                // Listen for a bit
                radio->startReceive();
                vTaskDelay(pdMS_TO_TICKS(500));

                // Check RSSI
                float rssi = radio->getRSSI();
                if (rssi > -100) {
                    Serial.printf("[LORA] Signal at %.3f MHz, SF%d, RSSI: %.1f\n",
                                  freq, sf, rssi);
                }
            }
        }
    }

    // Restore settings
    radio->setFrequency(currentFrequency);
    radio->setSpreadingFactor(currentSF);

    vTaskDelete(nullptr);
}

// ============================================================================
// Packet Analysis
// ============================================================================

LoRaPacketType LoRaModule::identifyPacket(const uint8_t* data, size_t len) {
    if (len < 4) return LoRaPacketType::UNKNOWN;

    // Check for Meshtastic packet structure
    // Meshtastic packets have a specific header format
    if (len >= sizeof(Meshtastic::PacketHeader)) {
        // Check if it looks like a Meshtastic packet
        // The first bytes should be destination node ID
        return LoRaPacketType::MESHTASTIC;
    }

    // TODO: Add MeshCore and LoRaWAN detection

    return LoRaPacketType::RAW;
}

bool LoRaModule::decodeMeshtasticPacket(LoRaPacket& packet) {
    if (packet.data.size() < sizeof(Meshtastic::PacketHeader)) {
        return false;
    }

    const uint8_t* data = packet.data.data();
    const Meshtastic::PacketHeader* header = (const Meshtastic::PacketHeader*)data;

    packet.meshFrom = header->sender;
    packet.meshTo = header->dest;
    packet.meshHopLimit = header->flags & 0x07;
    packet.meshWantAck = (header->flags >> 3) & 0x01;

    // Update node list
    updateMeshtasticNode(packet);

    // The payload after header is encrypted with AES-128 or AES-256
    // Decryption requires the channel key
    if (hasMeshtasticKey) {
        // TODO: Implement AES decryption
        packet.decoded = false;
    }

    return true;
}

bool LoRaModule::decodeMeshCorePacket(LoRaPacket& packet) {
    // TODO: Implement MeshCore packet decoding
    return false;
}

void LoRaModule::updateMeshtasticNode(const LoRaPacket& packet) {
    // Check if node already exists
    for (auto& node : meshtasticNodes) {
        if (node.nodeId == packet.meshFrom) {
            node.lastRssi = packet.rssi;
            node.lastSeen = millis();
            return;
        }
    }

    // Add new node
    MeshtasticNode node;
    node.nodeId = packet.meshFrom;
    node.lastRssi = packet.rssi;
    node.lastSeen = millis();
    node.hopLimit = packet.meshHopLimit;
    meshtasticNodes.push_back(node);

    Serial.printf("[LORA] New Meshtastic node: %08X\n", packet.meshFrom);
}

String LoRaModule::packetToHex(const uint8_t* data, size_t len) {
    String hex = "";
    for (size_t i = 0; i < len; i++) {
        char buf[4];
        snprintf(buf, sizeof(buf), "%02X ", data[i]);
        hex += buf;
    }
    return hex;
}

// ============================================================================
// Replay
// ============================================================================

bool LoRaModule::replayPacket(const LoRaPacket& packet) {
    if (packet.data.empty()) return false;

    Serial.printf("[LORA] Replaying packet, %d bytes\n", packet.data.size());

    // Temporarily set frequency if different
    float origFreq = currentFrequency;
    if (packet.frequency != currentFrequency) {
        setFrequency(packet.frequency);
    }

    bool result = transmit(packet.data.data(), packet.data.size());

    // Restore frequency
    if (origFreq != packet.frequency) {
        setFrequency(origFreq);
    }

    Storage::logf("lora", "Replayed packet: %d bytes", packet.data.size());
    return result;
}

// ============================================================================
// Signal Info
// ============================================================================

float LoRaModule::getLastRSSI() {
    return lastPacket.rssi;
}

float LoRaModule::getLastSNR() {
    return lastPacket.snr;
}

// ============================================================================
// Export
// ============================================================================

bool LoRaModule::exportPackets(const char* filename) {
    String path = String(PATH_LORA) + "/" + filename;

    String csv = "timestamp,frequency,rssi,snr,length,type,data\n";
    for (const auto& pkt : packetHistory) {
        csv += String(pkt.timestamp) + ",";
        csv += String(pkt.frequency, 3) + ",";
        csv += String(pkt.rssi, 1) + ",";
        csv += String(pkt.snr, 1) + ",";
        csv += String(pkt.length) + ",";
        csv += String((int)pkt.type) + ",";
        csv += packetToHex(pkt.data.data(), pkt.data.size()) + "\n";
    }

    return Storage::writeFile(path.c_str(), csv.c_str());
}

// ============================================================================
// Meshtastic Node Functions (Legitimate Communication)
// ============================================================================

void LoRaModule::setNodeId(uint32_t id) {
    myNodeId = id;
    Serial.printf("[LORA] Node ID set: %08X\n", id);
}

uint32_t LoRaModule::getNodeId() {
    // Generate from MAC if not set
    if (myNodeId == 0) {
        uint8_t mac[6];
        esp_efuse_mac_get_default(mac);
        // Use last 4 bytes of MAC as node ID (similar to Meshtastic)
        myNodeId = (mac[2] << 24) | (mac[3] << 16) | (mac[4] << 8) | mac[5];
    }
    return myNodeId;
}

void LoRaModule::setNodeName(const String& longName, const String& shortName) {
    myLongName = longName;
    myShortName = shortName;
    Serial.printf("[LORA] Node name: %s (%s)\n", longName.c_str(), shortName.c_str());
}

void LoRaModule::setChannelPSK(const uint8_t* psk, size_t len) {
    memset(meshtasticKey, 0, 32);
    memcpy(meshtasticKey, psk, min(len, (size_t)32));
    hasMeshtasticKey = true;
    Serial.println("[LORA] Channel PSK set");
}

void LoRaModule::setDefaultChannel() {
    // Default Meshtastic key "AQ==" = 0x01
    meshtasticKey[0] = 0x01;
    memset(meshtasticKey + 1, 0, 31);
    hasMeshtasticKey = true;
    setMeshtasticLongFast();
    Serial.println("[LORA] Default channel configured (LongFast)");
}

bool LoRaModule::sendMeshtasticText(const String& message, uint32_t destNode) {
    if (!initialized) {
        Serial.println("[LORA] Not initialized");
        return false;
    }

    // Build Meshtastic packet
    // Header: dest(4) + sender(4) + packetId(4) + flags(1) + channelHash(1) = 14 bytes
    // Then encrypted payload with portnum + text

    uint32_t nodeId = getNodeId();
    uint32_t pktId = ++packetIdCounter;

    // Build header
    uint8_t packet[256];
    size_t pos = 0;

    // Destination (broadcast or specific node)
    packet[pos++] = destNode & 0xFF;
    packet[pos++] = (destNode >> 8) & 0xFF;
    packet[pos++] = (destNode >> 16) & 0xFF;
    packet[pos++] = (destNode >> 24) & 0xFF;

    // Sender
    packet[pos++] = nodeId & 0xFF;
    packet[pos++] = (nodeId >> 8) & 0xFF;
    packet[pos++] = (nodeId >> 16) & 0xFF;
    packet[pos++] = (nodeId >> 24) & 0xFF;

    // Packet ID
    packet[pos++] = pktId & 0xFF;
    packet[pos++] = (pktId >> 8) & 0xFF;
    packet[pos++] = (pktId >> 16) & 0xFF;
    packet[pos++] = (pktId >> 24) & 0xFF;

    // Flags: hopLimit=3, wantAck=0
    packet[pos++] = 0x03;

    // Channel hash (0 for default channel)
    packet[pos++] = 0x08;

    // Payload: portnum (TEXT_MESSAGE=1) + message
    // Note: In real Meshtastic, this is encrypted + protobuf encoded
    // For simplicity, we send unencrypted for testing on your own network
    packet[pos++] = Meshtastic::PORT_TEXT_MESSAGE;

    // Copy message
    size_t msgLen = min(message.length(), (size_t)(256 - pos - 1));
    memcpy(packet + pos, message.c_str(), msgLen);
    pos += msgLen;

    Serial.printf("[LORA] Sending text to %08X: %s\n", destNode, message.c_str());

    return transmit(packet, pos);
}

bool LoRaModule::sendMeshtasticPosition(float lat, float lon, int32_t altitude) {
    if (!initialized) return false;

    uint32_t nodeId = getNodeId();
    uint32_t pktId = ++packetIdCounter;

    uint8_t packet[64];
    size_t pos = 0;

    // Destination (broadcast)
    uint32_t dest = 0xFFFFFFFF;
    packet[pos++] = dest & 0xFF;
    packet[pos++] = (dest >> 8) & 0xFF;
    packet[pos++] = (dest >> 16) & 0xFF;
    packet[pos++] = (dest >> 24) & 0xFF;

    // Sender
    packet[pos++] = nodeId & 0xFF;
    packet[pos++] = (nodeId >> 8) & 0xFF;
    packet[pos++] = (nodeId >> 16) & 0xFF;
    packet[pos++] = (nodeId >> 24) & 0xFF;

    // Packet ID
    packet[pos++] = pktId & 0xFF;
    packet[pos++] = (pktId >> 8) & 0xFF;
    packet[pos++] = (pktId >> 16) & 0xFF;
    packet[pos++] = (pktId >> 24) & 0xFF;

    // Flags
    packet[pos++] = 0x03;
    // Channel hash
    packet[pos++] = 0x08;

    // Port: POSITION
    packet[pos++] = Meshtastic::PORT_POSITION;

    // Position data (simplified, not full protobuf)
    int32_t latI = (int32_t)(lat * 1e7);
    int32_t lonI = (int32_t)(lon * 1e7);

    memcpy(packet + pos, &latI, 4); pos += 4;
    memcpy(packet + pos, &lonI, 4); pos += 4;
    memcpy(packet + pos, &altitude, 4); pos += 4;

    Serial.printf("[LORA] Sending position: %.6f, %.6f\n", lat, lon);

    return transmit(packet, pos);
}

bool LoRaModule::sendMeshtasticNodeInfo() {
    if (!initialized) return false;

    uint32_t nodeId = getNodeId();
    uint32_t pktId = ++packetIdCounter;

    uint8_t packet[128];
    size_t pos = 0;

    // Destination (broadcast)
    uint32_t dest = 0xFFFFFFFF;
    packet[pos++] = dest & 0xFF;
    packet[pos++] = (dest >> 8) & 0xFF;
    packet[pos++] = (dest >> 16) & 0xFF;
    packet[pos++] = (dest >> 24) & 0xFF;

    // Sender
    packet[pos++] = nodeId & 0xFF;
    packet[pos++] = (nodeId >> 8) & 0xFF;
    packet[pos++] = (nodeId >> 16) & 0xFF;
    packet[pos++] = (nodeId >> 24) & 0xFF;

    // Packet ID
    packet[pos++] = pktId & 0xFF;
    packet[pos++] = (pktId >> 8) & 0xFF;
    packet[pos++] = (pktId >> 16) & 0xFF;
    packet[pos++] = (pktId >> 24) & 0xFF;

    // Flags
    packet[pos++] = 0x03;
    // Channel hash
    packet[pos++] = 0x08;

    // Port: NODEINFO
    packet[pos++] = Meshtastic::PORT_NODEINFO;

    // Node info (simplified)
    // Long name
    uint8_t longLen = min((size_t)32, myLongName.length());
    packet[pos++] = longLen;
    memcpy(packet + pos, myLongName.c_str(), longLen);
    pos += longLen;

    // Short name
    uint8_t shortLen = min((size_t)4, myShortName.length());
    packet[pos++] = shortLen;
    memcpy(packet + pos, myShortName.c_str(), shortLen);
    pos += shortLen;

    Serial.printf("[LORA] Sending node info: %s\n", myLongName.c_str());

    return transmit(packet, pos);
}

// ============================================================================
// Menu Integration
// ============================================================================

void LoRaModule::buildMenu(void* menuPtr) {
    MenuScreen* menu = static_cast<MenuScreen*>(menuPtr);

    menu->addItem(MenuItem("Start Receive", []() {
        LoRaModule::startReceive();
        UIManager::showMessage("LoRa", "Receiving...");
    }));

    menu->addItem(MenuItem("Stop Receive", []() {
        LoRaModule::stopReceive();
    }));

    menu->addItem(MenuItem("Meshtastic Sniff", []() {
        LoRaModule::startMeshtasticSniff();
        UIManager::showMessage("LoRa", "Sniffing Meshtastic...");
    }));

    menu->addItem(MenuItem("MeshCore Sniff", []() {
        LoRaModule::startMeshCoreSniff();
        UIManager::showMessage("LoRa", "Sniffing MeshCore...");
    }));

    menu->addItem(MenuItem("Stop Sniffing", []() {
        LoRaModule::stopMeshtasticSniff();
        LoRaModule::stopMeshCoreSniff();
    }));

    menu->addItem(MenuItem("Frequency Scan", []() {
        LoRaModule::startFrequencyAnalyzer(902.0, 928.0, 0.5);
        UIManager::showMessage("LoRa", "Scanning frequencies...");
    }));

    menu->addItem(MenuItem("View Packets", []() {
        auto& packets = LoRaModule::getPacketHistory();
        String msg = String(packets.size()) + " packets captured";
        UIManager::showMessage("LoRa", msg);
    }));

    menu->addItem(MenuItem("View Mesh Nodes", []() {
        auto& nodes = LoRaModule::getMeshtasticNodes();
        String msg = String(nodes.size()) + " nodes found";
        UIManager::showMessage("LoRa", msg);
    }));

    menu->addItem(MenuItem("LongFast Preset", []() {
        LoRaModule::setMeshtasticLongFast();
        UIManager::showMessage("LoRa", "LongFast preset set");
    }));

    menu->addItem(MenuItem("ShortFast Preset", []() {
        LoRaModule::setMeshtasticShortFast();
        UIManager::showMessage("LoRa", "ShortFast preset set");
    }));

    // Meshtastic Node submenu
    static MenuScreen* meshNodeMenu = new MenuScreen("Meshtastic Node", menu);

    meshNodeMenu->addItem(MenuItem("Join Default Channel", []() {
        LoRaModule::setDefaultChannel();
        UIManager::showMessage("Meshtastic", "Joined default channel");
    }));

    meshNodeMenu->addItem(MenuItem("Send Message", []() {
        String msg = UIManager::showTextInput("Enter message:", "");
        if (msg.length() > 0) {
            LoRaModule::sendMeshtasticText(msg);
            UIManager::showMessage("Meshtastic", "Message sent!");
        }
    }));

    meshNodeMenu->addItem(MenuItem("Send Hello", []() {
        LoRaModule::sendMeshtasticText("Hello from ShitBird!");
        UIManager::showMessage("Meshtastic", "Sent hello message");
    }));

    meshNodeMenu->addItem(MenuItem("Send Node Info", []() {
        LoRaModule::sendMeshtasticNodeInfo();
        UIManager::showMessage("Meshtastic", "Node info broadcast");
    }));

    meshNodeMenu->addItem(MenuItem("Send GPS Position", []() {
        #if ENABLE_GPS
        if (GPSModule::hasFix()) {
            LoRaModule::sendMeshtasticPosition(
                GPSModule::getLatitude(),
                GPSModule::getLongitude(),
                (int32_t)GPSModule::getAltitude()
            );
            UIManager::showMessage("Meshtastic", "Position sent!");
        } else {
            UIManager::showMessage("Meshtastic", "No GPS fix");
        }
        #else
        UIManager::showMessage("Meshtastic", "GPS disabled");
        #endif
    }));

    meshNodeMenu->addItem(MenuItem("Start Listening", []() {
        LoRaModule::setDefaultChannel();
        LoRaModule::startReceive();
        UIManager::showMessage("Meshtastic", "Listening on default channel");
    }));

    meshNodeMenu->addItem(MenuItem("Show My Node ID", []() {
        uint32_t id = LoRaModule::getNodeId();
        char buf[20];
        snprintf(buf, sizeof(buf), "ID: %08X", id);
        UIManager::showMessage("Meshtastic", buf);
    }));

    meshNodeMenu->addItem(MenuItem::back());
    menu->addItem(MenuItem("Meshtastic Node", meshNodeMenu));

    menu->addItem(MenuItem("< Back", nullptr));
    static_cast<MenuItem&>(menu->items.back()).type = MenuItemType::BACK;
}
