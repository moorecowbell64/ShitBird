/**
 * ShitBird Firmware - IR Module
 * Infrared transmission, reception, learning, and TV-B-Gone functionality
 */

#ifndef SHITBIRD_IR_MODULE_H
#define SHITBIRD_IR_MODULE_H

#include <Arduino.h>
#include <IRremoteESP8266.h>
#include <IRsend.h>
#include <IRrecv.h>
#include <IRutils.h>
#include <vector>
#include "config.h"

// IR Protocol Types
enum class IRProtocol {
    UNKNOWN,
    NEC,
    SONY,
    RC5,
    RC6,
    SAMSUNG,
    LG,
    PANASONIC,
    JVC,
    SHARP,
    DENON,
    SANYO,
    MITSUBISHI,
    AIWA,
    COOLIX,
    DAIKIN,
    KELVINATOR,
    RAW
};

// Stored IR Code
struct IRCode {
    String name;
    IRProtocol protocol;
    uint64_t code;
    uint16_t bits;
    uint16_t address;  // For protocols that use address
    std::vector<uint16_t> rawData;  // For raw/unknown protocols
    uint16_t frequency;  // Carrier frequency (typically 38kHz)
};

// IR Code Category
struct IRCategory {
    String name;
    std::vector<IRCode> codes;
};

// TV-B-Gone Database Entry
struct TVBGoneCode {
    uint16_t frequency;
    uint8_t nPairs;
    uint8_t nBits;
    const uint16_t* times;
    const uint8_t* codes;
};

class IRModule {
public:
    static void init();
    static void update();
    static void deinit();

    // Transmission
    static void sendCode(const IRCode& code);
    static void sendNEC(uint64_t data, uint16_t bits = 32);
    static void sendSony(uint64_t data, uint16_t bits = 12);
    static void sendSamsung(uint64_t data, uint16_t bits = 32);
    static void sendLG(uint64_t data, uint16_t bits = 28);
    static void sendRC5(uint64_t data, uint16_t bits = 12);
    static void sendRC6(uint64_t data, uint16_t bits = 20);
    static void sendRaw(const uint16_t* data, uint16_t len, uint16_t freq = 38000);

    // Reception / Learning
    static void startLearning();
    static void stopLearning();
    static bool isLearning();
    static bool hasLearnedCode();
    static IRCode getLearnedCode();
    static void clearLearnedCode();

    // TV-B-Gone
    static void startTVBGone();
    static void stopTVBGone();
    static bool isTVBGoneRunning();
    static uint16_t getTVBGoneProgress();

    // Code Database
    static bool loadCodesFromSD();
    static bool saveCodeToSD(const IRCode& code, const String& category);
    static std::vector<IRCategory>& getCategories();
    static IRCode* findCode(const String& category, const String& name);

    // Built-in codes
    static void sendTVPower();      // Universal TV power toggle
    static void sendVolUp();
    static void sendVolDown();
    static void sendMute();
    static void sendChannelUp();
    static void sendChannelDown();

    // AC/HVAC
    static void sendACPower();
    static void sendACTempUp();
    static void sendACTempDown();

    // Brute force
    static void startBruteForce(IRProtocol protocol, uint16_t startCode = 0);
    static void stopBruteForce();
    static bool isBruteForcing();

    // Utility
    static String protocolToString(IRProtocol protocol);
    static IRProtocol stringToProtocol(const String& str);
    static String codeToString(const IRCode& code);

    // Menu integration
    static void buildMenu(void* menuScreen);

private:
    static IRsend* irSend;
    static IRrecv* irRecv;
    static decode_results results;

    static bool initialized;
    static bool learning;
    static bool tvbGoneRunning;
    static bool bruteForcing;

    static IRCode learnedCode;
    static bool hasLearned;

    static std::vector<IRCategory> categories;

    static uint16_t tvbGoneIndex;
    static uint16_t bruteForceCode;
    static IRProtocol bruteForceProtocol;

    static TaskHandle_t tvbGoneTaskHandle;
    static TaskHandle_t bruteForceTaskHandle;

    // Tasks
    static void tvbGoneTask(void* param);
    static void bruteForceTask(void* param);

    // Database loading
    static void loadBuiltInCodes();
    static bool parseIRFile(const String& path, IRCategory& category);
};

// ============================================================================
// TV-B-Gone Power Codes Database (Subset - full database in SD card)
// Common TV power codes for various manufacturers
// ============================================================================

namespace TVPowerCodes {
    // NEC Protocol (most common)
    const uint64_t SAMSUNG_POWER = 0xE0E040BF;
    const uint64_t LG_POWER = 0x20DF10EF;
    const uint64_t SONY_POWER = 0xA90;  // 12-bit Sony
    const uint64_t VIZIO_POWER = 0x20DF10EF;
    const uint64_t TCL_POWER = 0x807F02FD;
    const uint64_t HISENSE_POWER = 0x20DF10EF;
    const uint64_t SHARP_POWER = 0x41B67E81;
    const uint64_t PHILIPS_POWER = 0x0C;  // RC5
    const uint64_t PANASONIC_POWER = 0x400401007C7D;

    // Array of common power codes for TV-B-Gone
    struct PowerCode {
        IRProtocol protocol;
        uint64_t code;
        uint16_t bits;
        const char* brand;
    };

    const PowerCode COMMON_POWER_CODES[] = {
        {IRProtocol::NEC, 0xE0E040BF, 32, "Samsung"},
        {IRProtocol::NEC, 0x20DF10EF, 32, "LG"},
        {IRProtocol::SONY, 0xA90, 12, "Sony"},
        {IRProtocol::SONY, 0x290, 15, "Sony 15-bit"},
        {IRProtocol::SONY, 0x00290, 20, "Sony 20-bit"},
        {IRProtocol::NEC, 0x807F02FD, 32, "TCL/Roku"},
        {IRProtocol::NEC, 0x40BF10EF, 32, "Vizio"},
        {IRProtocol::PANASONIC, 0x400401007C7D, 48, "Panasonic"},
        {IRProtocol::SHARP, 0x41B67E81, 32, "Sharp"},
        {IRProtocol::RC5, 0x0C, 12, "Philips"},
        {IRProtocol::RC6, 0x0C, 20, "Philips RC6"},
        {IRProtocol::NEC, 0xF708FB04, 32, "Toshiba"},
        {IRProtocol::NEC, 0xB4B40CF3, 32, "Insignia"},
        {IRProtocol::NEC, 0x00FF00FF, 32, "Generic 1"},
        {IRProtocol::NEC, 0xFFB04F, 32, "Generic 2"},
        {IRProtocol::SAMSUNG, 0xE0E040BF, 32, "Samsung Alt"},
        {IRProtocol::LG, 0x20DF10EF, 28, "LG Alt"},
        {IRProtocol::JVC, 0xC0E8, 16, "JVC"},
        {IRProtocol::DENON, 0x2A4C0280, 32, "Denon"},
        {IRProtocol::SANYO, 0x1C1C, 16, "Sanyo"},
    };

    const int POWER_CODE_COUNT = sizeof(COMMON_POWER_CODES) / sizeof(COMMON_POWER_CODES[0]);
}

// AC/HVAC Common Codes
namespace ACCodes {
    // Carrier frequencies often 38kHz
    // These are simplified - real AC codes are much longer

    struct ACCode {
        const char* brand;
        IRProtocol protocol;
        uint64_t powerOn;
        uint64_t powerOff;
    };

    // Most AC units use proprietary protocols with long codes
    // These are placeholders - real implementation would use IRremoteESP8266's AC classes
}

#endif // SHITBIRD_IR_MODULE_H
