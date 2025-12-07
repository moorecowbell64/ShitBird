/**
 * ShitBird Firmware - Web Server
 * Remote control interface, file management, and OTA updates
 */

#ifndef SHITBIRD_WEB_SERVER_H
#define SHITBIRD_WEB_SERVER_H

#include <Arduino.h>
#include <WiFi.h>
#include <ESPAsyncWebServer.h>
#include <AsyncTCP.h>
#include <ArduinoJson.h>
#include <Update.h>
#include "config.h"

class WebServer {
public:
    static void init();
    static void update();
    static void stop();

    // AP Mode
    static void startAP(const char* ssid = WEB_AP_SSID,
                       const char* password = WEB_AP_PASS);
    static void stopAP();
    static bool isAPActive();
    static String getAPIP();

    // Station Mode (connect to existing network)
    static bool connectToNetwork(const char* ssid, const char* password);
    static void disconnect();
    static bool isConnected();
    static String getStationIP();

    // Server control
    static void startServer();
    static void stopServer();
    static bool isServerRunning();

    // OTA Updates
    static void enableOTA();
    static void disableOTA();
    static bool isOTAEnabled();

    // Authentication
    static void setCredentials(const char* username, const char* password);
    static bool checkAuth(AsyncWebServerRequest* request);

private:
    static AsyncWebServer* server;
    static AsyncWebSocket* ws;

    static bool apActive;
    static bool serverRunning;
    static bool otaEnabled;
    static bool connected;

    static String authUsername;
    static String authPassword;

    // Route handlers
    static void setupRoutes();

    // API endpoints
    static void handleRoot(AsyncWebServerRequest* request);
    static void handleStatus(AsyncWebServerRequest* request);
    static void handleWiFiScan(AsyncWebServerRequest* request);
    static void handleWiFiAction(AsyncWebServerRequest* request);
    static void handleBLEScan(AsyncWebServerRequest* request);
    static void handleBLEAction(AsyncWebServerRequest* request);
    static void handleLoRaAction(AsyncWebServerRequest* request);
    static void handleIRAction(AsyncWebServerRequest* request);
    static void handleSettings(AsyncWebServerRequest* request);
    static void handleProfiles(AsyncWebServerRequest* request);

    // File management
    static void handleFileList(AsyncWebServerRequest* request);
    static void handleFileDownload(AsyncWebServerRequest* request);
    static void handleFileUpload(AsyncWebServerRequest* request, String filename,
                                 size_t index, uint8_t* data, size_t len, bool final);
    static void handleFileDelete(AsyncWebServerRequest* request);

    // OTA handlers
    static void handleOTAUpdate(AsyncWebServerRequest* request);
    static void handleOTAUpload(AsyncWebServerRequest* request, String filename,
                                size_t index, uint8_t* data, size_t len, bool final);

    // WebSocket
    static void onWebSocketEvent(AsyncWebSocket* server, AsyncWebSocketClient* client,
                                 AwsEventType type, void* arg, uint8_t* data, size_t len);
    static void broadcastStatus();

    // Utility
    static String getContentType(const String& filename);
    static String generateToken();
};

// ============================================================================
// HTML Templates (stored in PROGMEM)
// ============================================================================

const char INDEX_HTML[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>ShitBird Control Panel</title>
    <style>
        :root {
            --bg-primary: #0a0a0a;
            --bg-secondary: #1a1a1a;
            --text-primary: #00ff00;
            --text-secondary: #00aa00;
            --accent: #00ffff;
            --warning: #ffaa00;
            --error: #ff0000;
        }
        * { box-sizing: border-box; margin: 0; padding: 0; }
        body {
            font-family: 'Courier New', monospace;
            background: var(--bg-primary);
            color: var(--text-primary);
            min-height: 100vh;
        }
        .header {
            background: var(--bg-secondary);
            padding: 1rem;
            border-bottom: 2px solid var(--text-primary);
            display: flex;
            justify-content: space-between;
            align-items: center;
        }
        .header h1 { color: var(--accent); font-size: 1.5rem; }
        .status-bar {
            display: flex;
            gap: 1rem;
            font-size: 0.8rem;
        }
        .status-item { color: var(--text-secondary); }
        .status-item.active { color: var(--text-primary); }
        .container { padding: 1rem; max-width: 1200px; margin: 0 auto; }
        .grid { display: grid; grid-template-columns: repeat(auto-fit, minmax(300px, 1fr)); gap: 1rem; }
        .card {
            background: var(--bg-secondary);
            border: 1px solid var(--text-secondary);
            border-radius: 4px;
            padding: 1rem;
        }
        .card h2 {
            color: var(--accent);
            font-size: 1rem;
            margin-bottom: 1rem;
            border-bottom: 1px solid var(--text-secondary);
            padding-bottom: 0.5rem;
        }
        .btn {
            background: var(--bg-primary);
            color: var(--text-primary);
            border: 1px solid var(--text-primary);
            padding: 0.5rem 1rem;
            cursor: pointer;
            font-family: inherit;
            margin: 0.25rem;
            transition: all 0.2s;
        }
        .btn:hover {
            background: var(--text-primary);
            color: var(--bg-primary);
        }
        .btn.danger { border-color: var(--error); color: var(--error); }
        .btn.danger:hover { background: var(--error); color: white; }
        .btn.warning { border-color: var(--warning); color: var(--warning); }
        .list {
            max-height: 200px;
            overflow-y: auto;
            border: 1px solid var(--text-secondary);
            margin: 0.5rem 0;
        }
        .list-item {
            padding: 0.5rem;
            border-bottom: 1px solid var(--bg-primary);
            cursor: pointer;
        }
        .list-item:hover { background: var(--bg-primary); }
        .list-item.selected { background: var(--text-secondary); color: var(--bg-primary); }
        input, select {
            background: var(--bg-primary);
            color: var(--text-primary);
            border: 1px solid var(--text-secondary);
            padding: 0.5rem;
            font-family: inherit;
            width: 100%;
            margin: 0.25rem 0;
        }
        .log {
            background: var(--bg-primary);
            border: 1px solid var(--text-secondary);
            padding: 0.5rem;
            height: 150px;
            overflow-y: auto;
            font-size: 0.8rem;
            white-space: pre-wrap;
        }
        .progress {
            background: var(--bg-primary);
            border: 1px solid var(--text-secondary);
            height: 20px;
            margin: 0.5rem 0;
        }
        .progress-bar {
            background: var(--text-primary);
            height: 100%;
            width: 0%;
            transition: width 0.3s;
        }
    </style>
</head>
<body>
    <div class="header">
        <h1>// ShitBird Control Panel</h1>
        <div class="status-bar">
            <span class="status-item" id="wifi-status">WiFi: --</span>
            <span class="status-item" id="ble-status">BLE: --</span>
            <span class="status-item" id="lora-status">LoRa: --</span>
            <span class="status-item" id="battery-status">BAT: --%</span>
        </div>
    </div>

    <div class="container">
        <div class="grid">
            <!-- WiFi Module -->
            <div class="card">
                <h2>> WiFi Module</h2>
                <div class="btn-group">
                    <button class="btn" onclick="wifiScan()">Scan</button>
                    <button class="btn danger" onclick="wifiDeauth()">Deauth</button>
                    <button class="btn" onclick="wifiBeacon()">Beacon Spam</button>
                    <button class="btn danger" onclick="wifiStop()">Stop</button>
                </div>
                <div class="list" id="wifi-list"></div>
                <div>Packets: <span id="wifi-packets">0</span></div>
            </div>

            <!-- BLE Module -->
            <div class="card">
                <h2>> BLE Module</h2>
                <div class="btn-group">
                    <button class="btn" onclick="bleScan()">Scan</button>
                    <button class="btn warning" onclick="bleSpam('apple')">Apple Spam</button>
                    <button class="btn warning" onclick="bleSpam('samsung')">Samsung Spam</button>
                    <button class="btn warning" onclick="bleSpam('all')">Spam All</button>
                    <button class="btn danger" onclick="bleStop()">Stop</button>
                </div>
                <div class="list" id="ble-list"></div>
                <div>Devices: <span id="ble-devices">0</span></div>
            </div>

            <!-- LoRa Module -->
            <div class="card">
                <h2>> LoRa Module</h2>
                <div class="btn-group">
                    <button class="btn" onclick="loraReceive()">Receive</button>
                    <button class="btn" onclick="loraMeshtastic()">Meshtastic</button>
                    <button class="btn" onclick="loraFreqScan()">Freq Scan</button>
                    <button class="btn danger" onclick="loraStop()">Stop</button>
                </div>
                <div>Freq: <span id="lora-freq">915.0</span> MHz</div>
                <div>Packets: <span id="lora-packets">0</span></div>
                <div>Nodes: <span id="mesh-nodes">0</span></div>
            </div>

            <!-- IR Module -->
            <div class="card">
                <h2>> IR Module</h2>
                <div class="btn-group">
                    <button class="btn" onclick="irTVBGone()">TV-B-Gone</button>
                    <button class="btn" onclick="irLearn()">Learn</button>
                    <button class="btn" onclick="irSend()">Send Learned</button>
                    <button class="btn danger" onclick="irStop()">Stop</button>
                </div>
                <div>Status: <span id="ir-status">Idle</span></div>
            </div>

            <!-- Files -->
            <div class="card">
                <h2>> File Manager</h2>
                <select id="file-dir" onchange="loadFiles()">
                    <option value="/logs">Logs</option>
                    <option value="/pcap">PCAP Files</option>
                    <option value="/payloads">Payloads</option>
                    <option value="/ir_codes">IR Codes</option>
                </select>
                <div class="list" id="file-list"></div>
                <div class="btn-group">
                    <button class="btn" onclick="downloadFile()">Download</button>
                    <button class="btn danger" onclick="deleteFile()">Delete</button>
                </div>
                <input type="file" id="file-upload" style="display:none" onchange="uploadFile()">
                <button class="btn" onclick="document.getElementById('file-upload').click()">Upload</button>
            </div>

            <!-- Settings -->
            <div class="card">
                <h2>> Settings</h2>
                <label>Profile:</label>
                <select id="profile" onchange="setProfile()">
                    <option value="0">Recon Only</option>
                    <option value="1">WiFi Assessment</option>
                    <option value="2">BLE Hunt</option>
                    <option value="3">Physical Security</option>
                    <option value="4">Stealth Mode</option>
                    <option value="5">Full Assault</option>
                </select>
                <label>Theme:</label>
                <select id="theme" onchange="setTheme()">
                    <option value="0">Hacker</option>
                    <option value="1">Cyberpunk</option>
                    <option value="2">Stealth</option>
                    <option value="3">Retro</option>
                    <option value="4">Blood</option>
                    <option value="5">Ocean</option>
                </select>
                <button class="btn" onclick="saveSettings()">Save Settings</button>
            </div>

            <!-- OTA Update -->
            <div class="card">
                <h2>> Firmware Update</h2>
                <input type="file" id="ota-file" accept=".bin">
                <div class="progress"><div class="progress-bar" id="ota-progress"></div></div>
                <button class="btn warning" onclick="uploadFirmware()">Update Firmware</button>
                <div id="ota-status"></div>
            </div>

            <!-- Console -->
            <div class="card" style="grid-column: span 2;">
                <h2>> Console</h2>
                <div class="log" id="console"></div>
                <input type="text" id="cmd-input" placeholder="Enter command..." onkeypress="if(event.key==='Enter')sendCommand()">
            </div>
        </div>
    </div>

    <script>
        let ws;
        let selectedFile = null;

        function connect() {
            ws = new WebSocket('ws://' + window.location.host + '/ws');
            ws.onopen = () => log('Connected to ShitBird');
            ws.onclose = () => { log('Disconnected'); setTimeout(connect, 3000); };
            ws.onmessage = (e) => handleMessage(JSON.parse(e.data));
        }

        function handleMessage(msg) {
            if (msg.type === 'status') {
                document.getElementById('wifi-status').textContent = 'WiFi: ' + (msg.wifi ? 'ON' : 'OFF');
                document.getElementById('ble-status').textContent = 'BLE: ' + (msg.ble ? 'ON' : 'OFF');
                document.getElementById('lora-status').textContent = 'LoRa: ' + (msg.lora ? 'ON' : 'OFF');
                document.getElementById('battery-status').textContent = 'BAT: ' + msg.battery + '%';
                document.getElementById('wifi-packets').textContent = msg.wifiPackets || 0;
                document.getElementById('ble-devices').textContent = msg.bleDevices || 0;
                document.getElementById('lora-packets').textContent = msg.loraPackets || 0;
                document.getElementById('mesh-nodes').textContent = msg.meshNodes || 0;
            } else if (msg.type === 'wifi_list') {
                updateList('wifi-list', msg.data, 'ssid');
            } else if (msg.type === 'ble_list') {
                updateList('ble-list', msg.data, 'name');
            } else if (msg.type === 'log') {
                log(msg.message);
            }
        }

        function updateList(id, items, labelKey) {
            const list = document.getElementById(id);
            list.innerHTML = items.map((item, i) =>
                `<div class="list-item" data-index="${i}">${item[labelKey] || item.address || 'Unknown'} (${item.rssi}dBm)</div>`
            ).join('');
        }

        function log(msg) {
            const console = document.getElementById('console');
            console.textContent += new Date().toLocaleTimeString() + ' ' + msg + '\n';
            console.scrollTop = console.scrollHeight;
        }

        function send(action, params = {}) {
            if (ws && ws.readyState === WebSocket.OPEN) {
                ws.send(JSON.stringify({ action, ...params }));
            }
        }

        // WiFi functions
        function wifiScan() { send('wifi_scan'); log('Starting WiFi scan...'); }
        function wifiDeauth() { send('wifi_deauth'); log('Starting deauth...'); }
        function wifiBeacon() { send('wifi_beacon'); log('Starting beacon spam...'); }
        function wifiStop() { send('wifi_stop'); log('Stopping WiFi attacks'); }

        // BLE functions
        function bleScan() { send('ble_scan'); log('Starting BLE scan...'); }
        function bleSpam(type) { send('ble_spam', { type }); log('Starting ' + type + ' spam...'); }
        function bleStop() { send('ble_stop'); log('Stopping BLE attacks'); }

        // LoRa functions
        function loraReceive() { send('lora_receive'); log('Starting LoRa receive...'); }
        function loraMeshtastic() { send('lora_meshtastic'); log('Starting Meshtastic sniff...'); }
        function loraFreqScan() { send('lora_freq_scan'); log('Starting frequency scan...'); }
        function loraStop() { send('lora_stop'); log('Stopping LoRa'); }

        // IR functions
        function irTVBGone() { send('ir_tvbgone'); log('Starting TV-B-Gone...'); }
        function irLearn() { send('ir_learn'); log('Learning IR code...'); }
        function irSend() { send('ir_send'); log('Sending IR code...'); }
        function irStop() { send('ir_stop'); log('Stopping IR'); }

        // File functions
        function loadFiles() {
            const dir = document.getElementById('file-dir').value;
            fetch('/api/files?dir=' + dir).then(r => r.json()).then(files => {
                const list = document.getElementById('file-list');
                list.innerHTML = files.map(f =>
                    `<div class="list-item" onclick="selectFile('${f}')">${f}</div>`
                ).join('');
            });
        }

        function selectFile(name) {
            selectedFile = name;
            document.querySelectorAll('#file-list .list-item').forEach(el => el.classList.remove('selected'));
            event.target.classList.add('selected');
        }

        function downloadFile() {
            if (selectedFile) {
                const dir = document.getElementById('file-dir').value;
                window.location.href = '/api/download?path=' + dir + '/' + selectedFile;
            }
        }

        function deleteFile() {
            if (selectedFile && confirm('Delete ' + selectedFile + '?')) {
                const dir = document.getElementById('file-dir').value;
                fetch('/api/delete?path=' + dir + '/' + selectedFile, { method: 'DELETE' })
                    .then(() => { loadFiles(); log('Deleted: ' + selectedFile); });
            }
        }

        function uploadFile() {
            const file = document.getElementById('file-upload').files[0];
            if (file) {
                const dir = document.getElementById('file-dir').value;
                const form = new FormData();
                form.append('file', file);
                form.append('path', dir);
                fetch('/api/upload', { method: 'POST', body: form })
                    .then(() => { loadFiles(); log('Uploaded: ' + file.name); });
            }
        }

        // Settings
        function setProfile() { send('set_profile', { profile: parseInt(document.getElementById('profile').value) }); }
        function setTheme() { send('set_theme', { theme: parseInt(document.getElementById('theme').value) }); }
        function saveSettings() { send('save_settings'); log('Settings saved'); }

        // OTA
        function uploadFirmware() {
            const file = document.getElementById('ota-file').files[0];
            if (!file) return;
            if (!confirm('Update firmware? Device will restart.')) return;

            const form = new FormData();
            form.append('firmware', file);

            const xhr = new XMLHttpRequest();
            xhr.open('POST', '/api/ota');
            xhr.upload.onprogress = (e) => {
                const pct = (e.loaded / e.total * 100).toFixed(0);
                document.getElementById('ota-progress').style.width = pct + '%';
                document.getElementById('ota-status').textContent = 'Uploading: ' + pct + '%';
            };
            xhr.onload = () => {
                document.getElementById('ota-status').textContent = 'Update complete. Restarting...';
                setTimeout(() => location.reload(), 5000);
            };
            xhr.send(form);
        }

        function sendCommand() {
            const input = document.getElementById('cmd-input');
            if (input.value) {
                send('command', { cmd: input.value });
                log('> ' + input.value);
                input.value = '';
            }
        }

        // Initialize
        connect();
        loadFiles();
        setInterval(() => send('get_status'), 2000);
    </script>
</body>
</html>
)rawliteral";

#endif // SHITBIRD_WEB_SERVER_H
