/**
 * ShitBird Firmware - Web Server Implementation
 */

#include "web_server.h"
#include "../core/system.h"
#include "../core/storage.h"
#include "../modules/wifi/wifi_module.h"
#include "../modules/ble/ble_module.h"
#include "../modules/lora/lora_module.h"
#include "../modules/ir/ir_module.h"

// Static member initialization
AsyncWebServer* WebServer::server = nullptr;
AsyncWebSocket* WebServer::ws = nullptr;

bool WebServer::apActive = false;
bool WebServer::serverRunning = false;
bool WebServer::otaEnabled = true;
bool WebServer::connected = false;

String WebServer::authUsername = "admin";
String WebServer::authPassword = "shitbird";

void WebServer::init() {
    Serial.println("[WEB] Initializing web server...");

    server = new AsyncWebServer(WEB_SERVER_PORT);
    ws = new AsyncWebSocket("/ws");

    ws->onEvent(onWebSocketEvent);
    server->addHandler(ws);

    setupRoutes();

    Serial.println("[WEB] Web server initialized");
}

void WebServer::update() {
    if (!serverRunning) return;

    // Clean up disconnected WebSocket clients
    ws->cleanupClients();

    // Broadcast status periodically
    static unsigned long lastBroadcast = 0;
    if (millis() - lastBroadcast > 1000) {
        broadcastStatus();
        lastBroadcast = millis();
    }
}

void WebServer::stop() {
    if (serverRunning) {
        server->end();
        serverRunning = false;
    }
    stopAP();
}

// ============================================================================
// AP Mode
// ============================================================================

void WebServer::startAP(const char* ssid, const char* password) {
    Serial.printf("[WEB] Starting AP: %s\n", ssid);

    WiFi.mode(WIFI_AP);
    WiFi.softAP(ssid, password, WEB_AP_CHANNEL, WEB_AP_HIDDEN, WEB_MAX_CLIENTS);

    apActive = true;

    Serial.printf("[WEB] AP IP: %s\n", WiFi.softAPIP().toString().c_str());

    startServer();

    Storage::logf("web", "AP started: %s", ssid);
}

void WebServer::stopAP() {
    if (apActive) {
        WiFi.softAPdisconnect(true);
        apActive = false;
        Serial.println("[WEB] AP stopped");
    }
}

bool WebServer::isAPActive() {
    return apActive;
}

String WebServer::getAPIP() {
    return WiFi.softAPIP().toString();
}

// ============================================================================
// Station Mode
// ============================================================================

bool WebServer::connectToNetwork(const char* ssid, const char* password) {
    Serial.printf("[WEB] Connecting to: %s\n", ssid);

    WiFi.mode(WIFI_STA);
    WiFi.begin(ssid, password);

    int timeout = 30;
    while (WiFi.status() != WL_CONNECTED && timeout > 0) {
        delay(1000);
        timeout--;
        Serial.print(".");
    }
    Serial.println();

    if (WiFi.status() == WL_CONNECTED) {
        connected = true;
        Serial.printf("[WEB] Connected! IP: %s\n", WiFi.localIP().toString().c_str());
        startServer();
        return true;
    }

    Serial.println("[WEB] Connection failed");
    return false;
}

void WebServer::disconnect() {
    WiFi.disconnect();
    connected = false;
}

bool WebServer::isConnected() {
    return connected && WiFi.status() == WL_CONNECTED;
}

String WebServer::getStationIP() {
    return WiFi.localIP().toString();
}

// ============================================================================
// Server Control
// ============================================================================

void WebServer::startServer() {
    if (serverRunning) return;

    server->begin();
    serverRunning = true;

    Serial.println("[WEB] Server started on port 80");
    Storage::log("web", "Server started");
}

void WebServer::stopServer() {
    if (!serverRunning) return;

    server->end();
    serverRunning = false;
}

bool WebServer::isServerRunning() {
    return serverRunning;
}

// ============================================================================
// Authentication
// ============================================================================

void WebServer::setCredentials(const char* username, const char* password) {
    authUsername = username;
    authPassword = password;
}

bool WebServer::checkAuth(AsyncWebServerRequest* request) {
    if (!request->authenticate(authUsername.c_str(), authPassword.c_str())) {
        request->requestAuthentication();
        return false;
    }
    return true;
}

// ============================================================================
// Routes Setup
// ============================================================================

void WebServer::setupRoutes() {
    // Main page
    server->on("/", HTTP_GET, handleRoot);

    // API endpoints
    server->on("/api/status", HTTP_GET, handleStatus);
    server->on("/api/wifi/scan", HTTP_GET, handleWiFiScan);
    server->on("/api/wifi/action", HTTP_POST, handleWiFiAction);
    server->on("/api/ble/scan", HTTP_GET, handleBLEScan);
    server->on("/api/ble/action", HTTP_POST, handleBLEAction);
    server->on("/api/lora/action", HTTP_POST, handleLoRaAction);
    server->on("/api/ir/action", HTTP_POST, handleIRAction);
    server->on("/api/settings", HTTP_GET, handleSettings);
    server->on("/api/settings", HTTP_POST, handleSettings);
    server->on("/api/profiles", HTTP_GET, handleProfiles);

    // File management
    server->on("/api/files", HTTP_GET, handleFileList);
    server->on("/api/download", HTTP_GET, handleFileDownload);
    server->on("/api/delete", HTTP_DELETE, handleFileDelete);
    server->on("/api/upload", HTTP_POST, [](AsyncWebServerRequest* request) {
        request->send(200);
    }, handleFileUpload);

    // OTA update
    server->on("/api/ota", HTTP_POST, handleOTAUpdate, handleOTAUpload);

    // 404 handler
    server->onNotFound([](AsyncWebServerRequest* request) {
        request->send(404, "text/plain", "Not Found");
    });
}

// ============================================================================
// Route Handlers
// ============================================================================

void WebServer::handleRoot(AsyncWebServerRequest* request) {
    request->send_P(200, "text/html", INDEX_HTML);
}

void WebServer::handleStatus(AsyncWebServerRequest* request) {
    StaticJsonDocument<512> doc;

    doc["wifi"] = g_systemState.settings.wifi.enabled;
    doc["ble"] = g_systemState.settings.ble.enabled;
    doc["lora"] = g_systemState.settings.lora.enabled;
    doc["battery"] = g_systemState.batteryPercent;
    doc["mode"] = (int)g_systemState.currentMode;
    doc["locked"] = g_systemState.locked;
    doc["wifiPackets"] = g_systemState.packetsCapture;
    doc["bleDevices"] = g_systemState.bleDevicesFound;
    doc["deauths"] = g_systemState.deauthsSent;
    doc["beacons"] = g_systemState.beaconsSent;

    String response;
    serializeJson(doc, response);
    request->send(200, "application/json", response);
}

void WebServer::handleWiFiScan(AsyncWebServerRequest* request) {
    auto& aps = WiFiModule::getAccessPoints();

    StaticJsonDocument<4096> doc;
    JsonArray array = doc.to<JsonArray>();

    for (const auto& ap : aps) {
        JsonObject obj = array.createNestedObject();
        obj["ssid"] = ap.ssid;
        obj["bssid"] = ap.bssid;
        obj["rssi"] = ap.rssi;
        obj["channel"] = ap.channel;
        obj["encryption"] = WiFiModule::getEncryptionString(ap.encryption);
    }

    String response;
    serializeJson(doc, response);
    request->send(200, "application/json", response);
}

void WebServer::handleWiFiAction(AsyncWebServerRequest* request) {
    if (!request->hasParam("action", true)) {
        request->send(400, "text/plain", "Missing action");
        return;
    }

    String action = request->getParam("action", true)->value();

    if (action == "scan") {
        WiFiModule::startScan();
    } else if (action == "deauth") {
        String bssid = request->hasParam("bssid", true) ?
                       request->getParam("bssid", true)->value() : "";
        if (bssid.length() > 0) {
            WiFiModule::startDeauthFlood(bssid);
        }
    } else if (action == "beacon") {
        WiFiModule::startBeaconSpamRandom();
    } else if (action == "stop") {
        WiFiModule::stopScan();
        WiFiModule::stopDeauth();
        WiFiModule::stopBeaconSpam();
    }

    request->send(200, "text/plain", "OK");
}

void WebServer::handleBLEScan(AsyncWebServerRequest* request) {
    auto& devices = BLEModule::getDevices();

    StaticJsonDocument<4096> doc;
    JsonArray array = doc.to<JsonArray>();

    for (const auto& dev : devices) {
        JsonObject obj = array.createNestedObject();
        obj["address"] = dev.address;
        obj["name"] = dev.name;
        obj["rssi"] = dev.rssi;
        obj["type"] = dev.deviceType;
    }

    String response;
    serializeJson(doc, response);
    request->send(200, "application/json", response);
}

void WebServer::handleBLEAction(AsyncWebServerRequest* request) {
    if (!request->hasParam("action", true)) {
        request->send(400, "text/plain", "Missing action");
        return;
    }

    String action = request->getParam("action", true)->value();

    if (action == "scan") {
        BLEModule::startScan();
    } else if (action == "spam_apple") {
        BLEModule::startSpam(BLEAttackType::APPLE_SPAM);
    } else if (action == "spam_samsung") {
        BLEModule::startSpam(BLEAttackType::SAMSUNG_SPAM);
    } else if (action == "spam_all") {
        BLEModule::startSpam(BLEAttackType::ALL_SPAM);
    } else if (action == "stop") {
        BLEModule::stopScan();
        BLEModule::stopSpam();
    }

    request->send(200, "text/plain", "OK");
}

void WebServer::handleLoRaAction(AsyncWebServerRequest* request) {
    if (!request->hasParam("action", true)) {
        request->send(400, "text/plain", "Missing action");
        return;
    }

    String action = request->getParam("action", true)->value();

    if (action == "receive") {
        LoRaModule::startReceive();
    } else if (action == "meshtastic") {
        LoRaModule::startMeshtasticSniff();
    } else if (action == "stop") {
        LoRaModule::stopReceive();
        LoRaModule::stopMeshtasticSniff();
    }

    request->send(200, "text/plain", "OK");
}

void WebServer::handleIRAction(AsyncWebServerRequest* request) {
    if (!request->hasParam("action", true)) {
        request->send(400, "text/plain", "Missing action");
        return;
    }

    String action = request->getParam("action", true)->value();

    if (action == "tvbgone") {
        IRModule::startTVBGone();
    } else if (action == "learn") {
        IRModule::startLearning();
    } else if (action == "send") {
        if (IRModule::hasLearnedCode()) {
            IRModule::sendCode(IRModule::getLearnedCode());
        }
    } else if (action == "stop") {
        IRModule::stopTVBGone();
        IRModule::stopLearning();
    }

    request->send(200, "text/plain", "OK");
}

void WebServer::handleSettings(AsyncWebServerRequest* request) {
    if (request->method() == HTTP_GET) {
        StaticJsonDocument<1024> doc;

        doc["profile"] = (int)g_systemState.settings.activeProfile;
        doc["theme"] = (int)g_systemState.settings.display.theme;
        doc["brightness"] = g_systemState.settings.display.brightness;
        doc["wifiEnabled"] = g_systemState.settings.wifi.enabled;
        doc["bleEnabled"] = g_systemState.settings.ble.enabled;
        doc["loraEnabled"] = g_systemState.settings.lora.enabled;
        doc["loraFreq"] = g_systemState.settings.lora.frequency;
        doc["deviceName"] = g_systemState.settings.deviceName;

        String response;
        serializeJson(doc, response);
        request->send(200, "application/json", response);
    } else {
        // POST - update settings
        if (request->hasParam("profile", true)) {
            int profile = request->getParam("profile", true)->value().toInt();
            g_systemState.applyProfile((Profile)profile);
        }
        if (request->hasParam("theme", true)) {
            int theme = request->getParam("theme", true)->value().toInt();
            g_systemState.settings.display.theme = (Theme)theme;
        }
        if (request->hasParam("save", true)) {
            g_systemState.saveSettings();
        }

        request->send(200, "text/plain", "OK");
    }
}

void WebServer::handleProfiles(AsyncWebServerRequest* request) {
    StaticJsonDocument<512> doc;
    JsonArray array = doc.to<JsonArray>();

    array.add("Recon Only");
    array.add("WiFi Assessment");
    array.add("BLE Hunt");
    array.add("Physical Security");
    array.add("Stealth Mode");
    array.add("Full Assault");
    array.add("Custom");

    String response;
    serializeJson(doc, response);
    request->send(200, "application/json", response);
}

// ============================================================================
// File Management
// ============================================================================

void WebServer::handleFileList(AsyncWebServerRequest* request) {
    String dir = request->hasParam("dir") ?
                 request->getParam("dir")->value() : "/";

    auto files = Storage::listFiles(dir.c_str());

    StaticJsonDocument<2048> doc;
    JsonArray array = doc.to<JsonArray>();

    for (const String& file : files) {
        array.add(file);
    }

    String response;
    serializeJson(doc, response);
    request->send(200, "application/json", response);
}

void WebServer::handleFileDownload(AsyncWebServerRequest* request) {
    if (!request->hasParam("path")) {
        request->send(400, "text/plain", "Missing path");
        return;
    }

    String path = request->getParam("path")->value();

    if (!Storage::exists(path.c_str())) {
        request->send(404, "text/plain", "File not found");
        return;
    }

    request->send(SD, path, getContentType(path), true);
}

void WebServer::handleFileUpload(AsyncWebServerRequest* request, String filename,
                                  size_t index, uint8_t* data, size_t len, bool final) {
    static File uploadFile;

    if (index == 0) {
        String path = "/";
        if (request->hasParam("path", true)) {
            path = request->getParam("path", true)->value();
        }
        String fullPath = path + "/" + filename;
        uploadFile = SD.open(fullPath, FILE_WRITE);
        Serial.printf("[WEB] Upload start: %s\n", fullPath.c_str());
    }

    if (uploadFile) {
        uploadFile.write(data, len);
    }

    if (final) {
        if (uploadFile) {
            uploadFile.close();
        }
        Serial.printf("[WEB] Upload complete: %s\n", filename.c_str());
    }
}

void WebServer::handleFileDelete(AsyncWebServerRequest* request) {
    if (!request->hasParam("path")) {
        request->send(400, "text/plain", "Missing path");
        return;
    }

    String path = request->getParam("path")->value();

    if (Storage::remove(path.c_str())) {
        request->send(200, "text/plain", "OK");
    } else {
        request->send(500, "text/plain", "Delete failed");
    }
}

// ============================================================================
// OTA Update
// ============================================================================

void WebServer::enableOTA() {
    otaEnabled = true;
}

void WebServer::disableOTA() {
    otaEnabled = false;
}

bool WebServer::isOTAEnabled() {
    return otaEnabled;
}

void WebServer::handleOTAUpdate(AsyncWebServerRequest* request) {
    if (Update.hasError()) {
        request->send(500, "text/plain", "Update failed");
    } else {
        request->send(200, "text/plain", "Update successful. Rebooting...");
        delay(1000);
        ESP.restart();
    }
}

void WebServer::handleOTAUpload(AsyncWebServerRequest* request, String filename,
                                 size_t index, uint8_t* data, size_t len, bool final) {
    if (!otaEnabled) {
        return;
    }

    if (index == 0) {
        Serial.printf("[WEB] OTA Update start: %s\n", filename.c_str());
        if (!Update.begin(UPDATE_SIZE_UNKNOWN)) {
            Update.printError(Serial);
        }
    }

    if (Update.write(data, len) != len) {
        Update.printError(Serial);
    }

    if (final) {
        if (Update.end(true)) {
            Serial.printf("[WEB] OTA Update complete: %u bytes\n", index + len);
            Storage::log("web", "OTA update successful");
        } else {
            Update.printError(Serial);
        }
    }
}

// ============================================================================
// WebSocket
// ============================================================================

void WebServer::onWebSocketEvent(AsyncWebSocket* server, AsyncWebSocketClient* client,
                                  AwsEventType type, void* arg, uint8_t* data, size_t len) {
    switch (type) {
        case WS_EVT_CONNECT:
            Serial.printf("[WEB] WebSocket client connected: %u\n", client->id());
            break;

        case WS_EVT_DISCONNECT:
            Serial.printf("[WEB] WebSocket client disconnected: %u\n", client->id());
            break;

        case WS_EVT_DATA: {
            AwsFrameInfo* info = (AwsFrameInfo*)arg;
            if (info->final && info->index == 0 && info->len == len && info->opcode == WS_TEXT) {
                data[len] = 0;  // Null terminate

                StaticJsonDocument<512> doc;
                deserializeJson(doc, (char*)data);

                String action = doc["action"] | "";

                if (action == "wifi_scan") {
                    WiFiModule::startScan();
                } else if (action == "wifi_deauth") {
                    auto selected = WiFiModule::getSelectedAPs();
                    if (!selected.empty()) {
                        WiFiModule::startDeauthFlood(selected[0]->bssid);
                    }
                } else if (action == "wifi_beacon") {
                    WiFiModule::startBeaconSpamRandom();
                } else if (action == "wifi_stop") {
                    WiFiModule::stopScan();
                    WiFiModule::stopDeauth();
                    WiFiModule::stopBeaconSpam();
                } else if (action == "ble_scan") {
                    BLEModule::startScan();
                } else if (action == "ble_spam") {
                    String type = doc["type"] | "all";
                    if (type == "apple") BLEModule::startSpam(BLEAttackType::APPLE_SPAM);
                    else if (type == "samsung") BLEModule::startSpam(BLEAttackType::SAMSUNG_SPAM);
                    else BLEModule::startSpam(BLEAttackType::ALL_SPAM);
                } else if (action == "ble_stop") {
                    BLEModule::stopScan();
                    BLEModule::stopSpam();
                } else if (action == "lora_receive") {
                    LoRaModule::startReceive();
                } else if (action == "lora_meshtastic") {
                    LoRaModule::startMeshtasticSniff();
                } else if (action == "lora_stop") {
                    LoRaModule::stopReceive();
                } else if (action == "ir_tvbgone") {
                    IRModule::startTVBGone();
                } else if (action == "ir_learn") {
                    IRModule::startLearning();
                } else if (action == "ir_send") {
                    if (IRModule::hasLearnedCode()) {
                        IRModule::sendCode(IRModule::getLearnedCode());
                    }
                } else if (action == "ir_stop") {
                    IRModule::stopTVBGone();
                    IRModule::stopLearning();
                } else if (action == "set_profile") {
                    int profile = doc["profile"] | 0;
                    g_systemState.applyProfile((Profile)profile);
                } else if (action == "set_theme") {
                    int theme = doc["theme"] | 0;
                    g_systemState.settings.display.theme = (Theme)theme;
                } else if (action == "save_settings") {
                    g_systemState.saveSettings();
                } else if (action == "get_status") {
                    // Status will be broadcast
                }
            }
            break;
        }

        default:
            break;
    }
}

void WebServer::broadcastStatus() {
    if (ws->count() == 0) return;

    StaticJsonDocument<512> doc;
    doc["type"] = "status";
    doc["wifi"] = g_systemState.settings.wifi.enabled;
    doc["ble"] = g_systemState.settings.ble.enabled;
    doc["lora"] = g_systemState.settings.lora.enabled;
    doc["battery"] = g_systemState.batteryPercent;
    doc["wifiPackets"] = g_systemState.packetsCapture;
    doc["bleDevices"] = g_systemState.bleDevicesFound;
    doc["loraPackets"] = LoRaModule::getPacketHistory().size();
    doc["meshNodes"] = LoRaModule::getMeshtasticNodes().size();

    String message;
    serializeJson(doc, message);
    ws->textAll(message);
}

// ============================================================================
// Utility
// ============================================================================

String WebServer::getContentType(const String& filename) {
    if (filename.endsWith(".html")) return "text/html";
    if (filename.endsWith(".css")) return "text/css";
    if (filename.endsWith(".js")) return "application/javascript";
    if (filename.endsWith(".json")) return "application/json";
    if (filename.endsWith(".png")) return "image/png";
    if (filename.endsWith(".jpg")) return "image/jpeg";
    if (filename.endsWith(".ico")) return "image/x-icon";
    if (filename.endsWith(".txt")) return "text/plain";
    if (filename.endsWith(".pcap")) return "application/octet-stream";
    if (filename.endsWith(".log")) return "text/plain";
    return "application/octet-stream";
}

String WebServer::generateToken() {
    String token = "";
    for (int i = 0; i < 32; i++) {
        token += String(random(0, 16), HEX);
    }
    return token;
}
