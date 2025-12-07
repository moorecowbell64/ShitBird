/**
 * ShitBird Firmware - IR Module Implementation
 */

#include "ir_module.h"
#include "../../core/system.h"
#include "../../core/storage.h"
#include "../../ui/ui_manager.h"

// Static member initialization
IRsend* IRModule::irSend = nullptr;
IRrecv* IRModule::irRecv = nullptr;
decode_results IRModule::results;

bool IRModule::initialized = false;
bool IRModule::learning = false;
bool IRModule::tvbGoneRunning = false;
bool IRModule::bruteForcing = false;

IRCode IRModule::learnedCode;
bool IRModule::hasLearned = false;

std::vector<IRCategory> IRModule::categories;

uint16_t IRModule::tvbGoneIndex = 0;
uint16_t IRModule::bruteForceCode = 0;
IRProtocol IRModule::bruteForceProtocol = IRProtocol::NEC;

TaskHandle_t IRModule::tvbGoneTaskHandle = nullptr;
TaskHandle_t IRModule::bruteForceTaskHandle = nullptr;

void IRModule::init() {
    if (initialized) return;

    Serial.println("[IR] Initializing...");

    // Initialize IR transmitter
    irSend = new IRsend(IR_TX_PIN);
    irSend->begin();

    // Initialize IR receiver
    irRecv = new IRrecv(IR_RX_PIN, 1024, 50, true);

    // Load built-in codes
    loadBuiltInCodes();

    // Try to load codes from SD card
    loadCodesFromSD();

    initialized = true;
    Serial.println("[IR] Initialized");
}

void IRModule::update() {
    if (!initialized) return;

    // Check for received IR codes if learning
    if (learning && irRecv->decode(&results)) {
        // Convert results to IRCode
        learnedCode.name = "Learned_" + String(millis());
        learnedCode.bits = results.bits;
        learnedCode.code = results.value;
        learnedCode.frequency = 38000;

        // Determine protocol
        switch (results.decode_type) {
            case decode_type_t::NEC: learnedCode.protocol = IRProtocol::NEC; break;
            case decode_type_t::SONY: learnedCode.protocol = IRProtocol::SONY; break;
            case decode_type_t::RC5: learnedCode.protocol = IRProtocol::RC5; break;
            case decode_type_t::RC6: learnedCode.protocol = IRProtocol::RC6; break;
            case decode_type_t::SAMSUNG: learnedCode.protocol = IRProtocol::SAMSUNG; break;
            case decode_type_t::LG: learnedCode.protocol = IRProtocol::LG; break;
            case decode_type_t::PANASONIC: learnedCode.protocol = IRProtocol::PANASONIC; break;
            case decode_type_t::JVC: learnedCode.protocol = IRProtocol::JVC; break;
            case decode_type_t::SHARP: learnedCode.protocol = IRProtocol::SHARP; break;
            default:
                learnedCode.protocol = IRProtocol::RAW;
                // Store raw data
                learnedCode.rawData.clear();
                for (uint16_t i = 1; i < results.rawlen; i++) {
                    learnedCode.rawData.push_back(results.rawbuf[i] * kRawTick);
                }
                break;
        }

        hasLearned = true;

        Serial.printf("[IR] Learned: %s, code: 0x%llX, bits: %d\n",
                      protocolToString(learnedCode.protocol).c_str(),
                      learnedCode.code, learnedCode.bits);

        Storage::logf("ir", "Learned code: %s 0x%llX",
                      protocolToString(learnedCode.protocol).c_str(),
                      learnedCode.code);

        irRecv->resume();
    }
}

void IRModule::deinit() {
    if (!initialized) return;

    stopLearning();
    stopTVBGone();
    stopBruteForce();

    delete irSend;
    delete irRecv;
    irSend = nullptr;
    irRecv = nullptr;

    initialized = false;
}

// ============================================================================
// Transmission
// ============================================================================

void IRModule::sendCode(const IRCode& code) {
    if (!initialized) return;

    Serial.printf("[IR] Sending: %s, protocol: %s, code: 0x%llX\n",
                  code.name.c_str(),
                  protocolToString(code.protocol).c_str(),
                  code.code);

    switch (code.protocol) {
        case IRProtocol::NEC:
            sendNEC(code.code, code.bits);
            break;
        case IRProtocol::SONY:
            sendSony(code.code, code.bits);
            break;
        case IRProtocol::SAMSUNG:
            sendSamsung(code.code, code.bits);
            break;
        case IRProtocol::LG:
            sendLG(code.code, code.bits);
            break;
        case IRProtocol::RC5:
            sendRC5(code.code, code.bits);
            break;
        case IRProtocol::RC6:
            sendRC6(code.code, code.bits);
            break;
        case IRProtocol::RAW:
            if (!code.rawData.empty()) {
                sendRaw(code.rawData.data(), code.rawData.size(), code.frequency);
            }
            break;
        default:
            // Try NEC as fallback
            sendNEC(code.code, code.bits);
            break;
    }

    Storage::logf("ir", "Sent: %s", code.name.c_str());
}

void IRModule::sendNEC(uint64_t data, uint16_t bits) {
    if (!irSend) return;
    irSend->sendNEC(data, bits);
}

void IRModule::sendSony(uint64_t data, uint16_t bits) {
    if (!irSend) return;
    // Sony protocol often requires multiple sends
    for (int i = 0; i < 3; i++) {
        irSend->sendSony(data, bits);
        delay(40);
    }
}

void IRModule::sendSamsung(uint64_t data, uint16_t bits) {
    if (!irSend) return;
    irSend->sendSAMSUNG(data, bits);
}

void IRModule::sendLG(uint64_t data, uint16_t bits) {
    if (!irSend) return;
    irSend->sendLG(data, bits);
}

void IRModule::sendRC5(uint64_t data, uint16_t bits) {
    if (!irSend) return;
    irSend->sendRC5(data, bits);
}

void IRModule::sendRC6(uint64_t data, uint16_t bits) {
    if (!irSend) return;
    irSend->sendRC6(data, bits);
}

void IRModule::sendRaw(const uint16_t* data, uint16_t len, uint16_t freq) {
    if (!irSend) return;
    irSend->sendRaw(data, len, freq / 1000);  // IRsend expects kHz
}

// ============================================================================
// Learning
// ============================================================================

void IRModule::startLearning() {
    if (learning) return;

    Serial.println("[IR] Starting learning mode...");
    learning = true;
    hasLearned = false;
    g_systemState.currentMode = OperationMode::IR_RX;

    irRecv->enableIRIn();

    Storage::log("ir", "Learning mode started");
}

void IRModule::stopLearning() {
    if (!learning) return;

    Serial.println("[IR] Stopping learning mode");
    learning = false;

    irRecv->disableIRIn();

    if (g_systemState.currentMode == OperationMode::IR_RX) {
        g_systemState.currentMode = OperationMode::IDLE;
    }
}

bool IRModule::isLearning() {
    return learning;
}

bool IRModule::hasLearnedCode() {
    return hasLearned;
}

IRCode IRModule::getLearnedCode() {
    return learnedCode;
}

void IRModule::clearLearnedCode() {
    hasLearned = false;
    learnedCode = IRCode();
}

// ============================================================================
// TV-B-Gone
// ============================================================================

void IRModule::startTVBGone() {
    if (tvbGoneRunning) stopTVBGone();

    Serial.println("[IR] Starting TV-B-Gone...");
    tvbGoneRunning = true;
    tvbGoneIndex = 0;
    g_systemState.currentMode = OperationMode::IR_TX;

    xTaskCreatePinnedToCore(
        tvbGoneTask,
        "IR_TVBGone",
        4096,
        nullptr,
        1,
        &tvbGoneTaskHandle,
        0
    );

    Storage::log("ir", "TV-B-Gone started");
}

void IRModule::stopTVBGone() {
    if (!tvbGoneRunning) return;

    Serial.println("[IR] Stopping TV-B-Gone");
    tvbGoneRunning = false;

    if (tvbGoneTaskHandle) {
        vTaskDelete(tvbGoneTaskHandle);
        tvbGoneTaskHandle = nullptr;
    }

    if (g_systemState.currentMode == OperationMode::IR_TX) {
        g_systemState.currentMode = OperationMode::IDLE;
    }

    Storage::logf("ir", "TV-B-Gone stopped at index %d", tvbGoneIndex);
}

bool IRModule::isTVBGoneRunning() {
    return tvbGoneRunning;
}

uint16_t IRModule::getTVBGoneProgress() {
    return (tvbGoneIndex * 100) / TVPowerCodes::POWER_CODE_COUNT;
}

void IRModule::tvbGoneTask(void* param) {
    using namespace TVPowerCodes;

    Serial.printf("[IR] TV-B-Gone: %d codes to send\n", POWER_CODE_COUNT);

    while (tvbGoneRunning && tvbGoneIndex < POWER_CODE_COUNT) {
        const PowerCode& pc = COMMON_POWER_CODES[tvbGoneIndex];

        Serial.printf("[IR] Sending %s power (%d/%d)\n",
                      pc.brand, tvbGoneIndex + 1, POWER_CODE_COUNT);

        // Send the power code
        switch (pc.protocol) {
            case IRProtocol::NEC:
                irSend->sendNEC(pc.code, pc.bits);
                break;
            case IRProtocol::SONY:
                for (int i = 0; i < 3; i++) {
                    irSend->sendSony(pc.code, pc.bits);
                    vTaskDelay(pdMS_TO_TICKS(40));
                }
                break;
            case IRProtocol::SAMSUNG:
                irSend->sendSAMSUNG(pc.code, pc.bits);
                break;
            case IRProtocol::LG:
                irSend->sendLG(pc.code, pc.bits);
                break;
            case IRProtocol::PANASONIC:
                irSend->sendPanasonic(0x4004, pc.code);
                break;
            case IRProtocol::SHARP:
                irSend->sendSharpRaw(pc.code, pc.bits);
                break;
            case IRProtocol::RC5:
                irSend->sendRC5(pc.code, pc.bits);
                break;
            case IRProtocol::RC6:
                irSend->sendRC6(pc.code, pc.bits);
                break;
            case IRProtocol::JVC:
                irSend->sendJVC(pc.code, pc.bits);
                break;
            case IRProtocol::DENON:
                irSend->sendDenon(pc.code, pc.bits);
                break;
            case IRProtocol::SANYO:
                irSend->sendSanyoLC7461(pc.code, pc.bits);
                break;
            default:
                irSend->sendNEC(pc.code, pc.bits);
                break;
        }

        tvbGoneIndex++;
        vTaskDelay(pdMS_TO_TICKS(100));  // Delay between codes
    }

    Serial.println("[IR] TV-B-Gone complete");
    tvbGoneRunning = false;

    if (g_systemState.currentMode == OperationMode::IR_TX) {
        g_systemState.currentMode = OperationMode::IDLE;
    }

    vTaskDelete(nullptr);
}

// ============================================================================
// Quick Commands
// ============================================================================

void IRModule::sendTVPower() {
    // Send multiple common power codes
    sendNEC(TVPowerCodes::SAMSUNG_POWER, 32);
    delay(100);
    sendNEC(TVPowerCodes::LG_POWER, 32);
    delay(100);
    sendSony(TVPowerCodes::SONY_POWER, 12);
}

void IRModule::sendVolUp() {
    // Samsung volume up
    sendNEC(0xE0E0E01F, 32);
    delay(50);
    // LG volume up
    sendNEC(0x20DF40BF, 32);
}

void IRModule::sendVolDown() {
    // Samsung volume down
    sendNEC(0xE0E0D02F, 32);
    delay(50);
    // LG volume down
    sendNEC(0x20DFC03F, 32);
}

void IRModule::sendMute() {
    // Samsung mute
    sendNEC(0xE0E0F00F, 32);
    delay(50);
    // LG mute
    sendNEC(0x20DF906F, 32);
}

void IRModule::sendChannelUp() {
    sendNEC(0xE0E048B7, 32);  // Samsung
    delay(50);
    sendNEC(0x20DF00FF, 32);  // LG
}

void IRModule::sendChannelDown() {
    sendNEC(0xE0E008F7, 32);  // Samsung
    delay(50);
    sendNEC(0x20DF807F, 32);  // LG
}

void IRModule::sendACPower() {
    // Generic AC power toggle - most AC use proprietary protocols
    // This is a simplified implementation
    sendNEC(0x10AF8877, 32);  // Generic
}

void IRModule::sendACTempUp() {
    sendNEC(0x10AF708F, 32);
}

void IRModule::sendACTempDown() {
    sendNEC(0x10AFB04F, 32);
}

// ============================================================================
// Brute Force
// ============================================================================

void IRModule::startBruteForce(IRProtocol protocol, uint16_t startCode) {
    if (bruteForcing) stopBruteForce();

    Serial.printf("[IR] Starting brute force: %s from 0x%04X\n",
                  protocolToString(protocol).c_str(), startCode);

    bruteForcing = true;
    bruteForceProtocol = protocol;
    bruteForceCode = startCode;
    g_systemState.currentMode = OperationMode::IR_TX;

    xTaskCreatePinnedToCore(
        bruteForceTask,
        "IR_BruteForce",
        4096,
        nullptr,
        1,
        &bruteForceTaskHandle,
        0
    );

    Storage::logf("ir", "Brute force started: %s",
                  protocolToString(protocol).c_str());
}

void IRModule::stopBruteForce() {
    if (!bruteForcing) return;

    Serial.println("[IR] Stopping brute force");
    bruteForcing = false;

    if (bruteForceTaskHandle) {
        vTaskDelete(bruteForceTaskHandle);
        bruteForceTaskHandle = nullptr;
    }

    if (g_systemState.currentMode == OperationMode::IR_TX) {
        g_systemState.currentMode = OperationMode::IDLE;
    }

    Storage::logf("ir", "Brute force stopped at 0x%04X", bruteForceCode);
}

bool IRModule::isBruteForcing() {
    return bruteForcing;
}

void IRModule::bruteForceTask(void* param) {
    Serial.println("[IR] Brute force task started");

    while (bruteForcing && bruteForceCode < 0xFFFF) {
        switch (bruteForceProtocol) {
            case IRProtocol::NEC:
                irSend->sendNEC(bruteForceCode, 16);
                break;
            case IRProtocol::SONY:
                irSend->sendSony(bruteForceCode, 12);
                break;
            case IRProtocol::RC5:
                irSend->sendRC5(bruteForceCode, 12);
                break;
            default:
                irSend->sendNEC(bruteForceCode, 16);
                break;
        }

        bruteForceCode++;

        // Print progress every 256 codes
        if ((bruteForceCode & 0xFF) == 0) {
            Serial.printf("[IR] Brute force progress: 0x%04X\n", bruteForceCode);
        }

        vTaskDelay(pdMS_TO_TICKS(50));  // 50ms between codes
    }

    Serial.println("[IR] Brute force complete");
    bruteForcing = false;

    if (g_systemState.currentMode == OperationMode::IR_TX) {
        g_systemState.currentMode = OperationMode::IDLE;
    }

    vTaskDelete(nullptr);
}

// ============================================================================
// Database Management
// ============================================================================

void IRModule::loadBuiltInCodes() {
    // TV Remote category
    IRCategory tvCategory;
    tvCategory.name = "TV Remote";

    IRCode code;

    code.name = "Samsung Power";
    code.protocol = IRProtocol::NEC;
    code.code = TVPowerCodes::SAMSUNG_POWER;
    code.bits = 32;
    tvCategory.codes.push_back(code);

    code.name = "LG Power";
    code.protocol = IRProtocol::NEC;
    code.code = TVPowerCodes::LG_POWER;
    code.bits = 32;
    tvCategory.codes.push_back(code);

    code.name = "Sony Power";
    code.protocol = IRProtocol::SONY;
    code.code = TVPowerCodes::SONY_POWER;
    code.bits = 12;
    tvCategory.codes.push_back(code);

    categories.push_back(tvCategory);

    // Quick Actions category
    IRCategory quickCategory;
    quickCategory.name = "Quick Actions";

    code.name = "Volume Up";
    code.protocol = IRProtocol::NEC;
    code.code = 0xE0E0E01F;
    code.bits = 32;
    quickCategory.codes.push_back(code);

    code.name = "Volume Down";
    code.protocol = IRProtocol::NEC;
    code.code = 0xE0E0D02F;
    code.bits = 32;
    quickCategory.codes.push_back(code);

    code.name = "Mute";
    code.protocol = IRProtocol::NEC;
    code.code = 0xE0E0F00F;
    code.bits = 32;
    quickCategory.codes.push_back(code);

    categories.push_back(quickCategory);

    Serial.printf("[IR] Loaded %d built-in categories\n", categories.size());
}

bool IRModule::loadCodesFromSD() {
    if (!Storage::isMounted()) return false;

    auto files = Storage::listFiles(PATH_IR_CODES, ".json");

    for (const String& filename : files) {
        String path = String(PATH_IR_CODES) + "/" + filename;
        IRCategory category;

        if (parseIRFile(path, category)) {
            categories.push_back(category);
            Serial.printf("[IR] Loaded category: %s\n", category.name.c_str());
        }
    }

    return true;
}

bool IRModule::parseIRFile(const String& path, IRCategory& category) {
    String content = Storage::readFile(path.c_str());
    if (content.isEmpty()) return false;

    // Simple JSON parsing (would use ArduinoJson in production)
    // For now, just extract the category name from filename
    int lastSlash = path.lastIndexOf('/');
    int lastDot = path.lastIndexOf('.');
    category.name = path.substring(lastSlash + 1, lastDot);

    // TODO: Parse JSON format for codes
    // Expected format:
    // {
    //   "name": "Category Name",
    //   "codes": [
    //     {"name": "Power", "protocol": "NEC", "code": "0xE0E040BF", "bits": 32},
    //     ...
    //   ]
    // }

    return true;
}

bool IRModule::saveCodeToSD(const IRCode& code, const String& category) {
    if (!Storage::isMounted()) return false;

    // Find or create category file
    String path = String(PATH_IR_CODES) + "/" + category + ".json";

    // Simple format for now
    String json = "{\"name\":\"" + code.name + "\",";
    json += "\"protocol\":\"" + protocolToString(code.protocol) + "\",";
    json += "\"code\":\"0x" + String((uint32_t)code.code, HEX) + "\",";
    json += "\"bits\":" + String(code.bits) + "}\n";

    return Storage::appendFile(path.c_str(), json.c_str());
}

std::vector<IRCategory>& IRModule::getCategories() {
    return categories;
}

IRCode* IRModule::findCode(const String& category, const String& name) {
    for (auto& cat : categories) {
        if (cat.name == category) {
            for (auto& code : cat.codes) {
                if (code.name == name) {
                    return &code;
                }
            }
        }
    }
    return nullptr;
}

// ============================================================================
// Utility
// ============================================================================

String IRModule::protocolToString(IRProtocol protocol) {
    switch (protocol) {
        case IRProtocol::NEC: return "NEC";
        case IRProtocol::SONY: return "Sony";
        case IRProtocol::RC5: return "RC5";
        case IRProtocol::RC6: return "RC6";
        case IRProtocol::SAMSUNG: return "Samsung";
        case IRProtocol::LG: return "LG";
        case IRProtocol::PANASONIC: return "Panasonic";
        case IRProtocol::JVC: return "JVC";
        case IRProtocol::SHARP: return "Sharp";
        case IRProtocol::DENON: return "Denon";
        case IRProtocol::SANYO: return "Sanyo";
        case IRProtocol::RAW: return "Raw";
        default: return "Unknown";
    }
}

IRProtocol IRModule::stringToProtocol(const String& str) {
    if (str == "NEC") return IRProtocol::NEC;
    if (str == "Sony") return IRProtocol::SONY;
    if (str == "RC5") return IRProtocol::RC5;
    if (str == "RC6") return IRProtocol::RC6;
    if (str == "Samsung") return IRProtocol::SAMSUNG;
    if (str == "LG") return IRProtocol::LG;
    if (str == "Panasonic") return IRProtocol::PANASONIC;
    if (str == "JVC") return IRProtocol::JVC;
    if (str == "Sharp") return IRProtocol::SHARP;
    if (str == "Raw") return IRProtocol::RAW;
    return IRProtocol::UNKNOWN;
}

String IRModule::codeToString(const IRCode& code) {
    return code.name + " [" + protocolToString(code.protocol) +
           "] 0x" + String((uint32_t)code.code, HEX);
}

// ============================================================================
// Menu Integration
// ============================================================================

void IRModule::buildMenu(void* menuPtr) {
    MenuScreen* menu = static_cast<MenuScreen*>(menuPtr);

    menu->addItem(MenuItem("TV-B-Gone", []() {
        IRModule::startTVBGone();
        UIManager::showMessage("IR", "TV-B-Gone started");
    }));

    menu->addItem(MenuItem("Stop TV-B-Gone", []() {
        IRModule::stopTVBGone();
    }));

    menu->addItem(MenuItem("Learn Code", []() {
        IRModule::startLearning();
        UIManager::showMessage("IR", "Point remote at device...");
    }));

    menu->addItem(MenuItem("Stop Learning", []() {
        IRModule::stopLearning();
        if (IRModule::hasLearnedCode()) {
            UIManager::showMessage("IR", "Code learned!");
        }
    }));

    menu->addItem(MenuItem("Send Learned", []() {
        if (IRModule::hasLearnedCode()) {
            IRModule::sendCode(IRModule::getLearnedCode());
            UIManager::showMessage("IR", "Code sent");
        } else {
            UIManager::showMessage("IR", "No code learned");
        }
    }));

    menu->addItem(MenuItem("TV Power", []() {
        IRModule::sendTVPower();
        UIManager::showMessage("IR", "Power sent");
    }));

    menu->addItem(MenuItem("Volume Up", []() {
        IRModule::sendVolUp();
    }));

    menu->addItem(MenuItem("Volume Down", []() {
        IRModule::sendVolDown();
    }));

    menu->addItem(MenuItem("Mute", []() {
        IRModule::sendMute();
    }));

    menu->addItem(MenuItem("Brute Force NEC", []() {
        IRModule::startBruteForce(IRProtocol::NEC);
        UIManager::showMessage("IR", "Brute force started");
    }));

    menu->addItem(MenuItem("Stop Brute Force", []() {
        IRModule::stopBruteForce();
    }));

    menu->addItem(MenuItem("< Back", nullptr));
    static_cast<MenuItem&>(menu->items.back()).type = MenuItemType::BACK;
}
