/**
 * ShitBird Firmware - Keyboard Driver
 * Handles the ESP32-C3 based keyboard on T-Deck Plus
 */

#ifndef SHITBIRD_KEYBOARD_H
#define SHITBIRD_KEYBOARD_H

#include <Arduino.h>
#include <Wire.h>
#include "config.h"

// Keyboard I2C commands
#define KB_CMD_BRIGHTNESS   0x01
#define KB_CMD_ALT_BRIGHT   0x02

// Special key codes
#define KEY_NONE            0x00
#define KEY_BACKSPACE       0x08
#define KEY_TAB             0x09
#define KEY_ENTER           0x0D
#define KEY_SHIFT           0x80
#define KEY_CTRL            0x81
#define KEY_ALT             0x82
#define KEY_FN              0x83
#define KEY_SYMBOL          0x84
#define KEY_SPACE           0x20
#define KEY_UP              0x85
#define KEY_DOWN            0x86
#define KEY_LEFT            0x87
#define KEY_RIGHT           0x88
#define KEY_ESC             0x1B

// Trackball directions (reported via keyboard)
#define TRACKBALL_UP        0xE0
#define TRACKBALL_DOWN      0xE1
#define TRACKBALL_LEFT      0xE2
#define TRACKBALL_RIGHT     0xE3
#define TRACKBALL_CLICK     0xE4

struct KeyEvent {
    uint8_t key;
    bool pressed;
    bool shift;
    bool ctrl;
    bool alt;
    bool fn;
    bool symbol;
};

class Keyboard {
public:
    static void init();
    static void update();

    // Key state
    static bool hasKey();
    static KeyEvent getKey();
    static char getChar();  // Returns ASCII character or 0

    // Trackball
    static int8_t getTrackballX();
    static int8_t getTrackballY();
    static bool isTrackballClicked();
    static void resetTrackball();

    // Backlight
    static void setBacklight(uint8_t brightness);
    static uint8_t getBacklight();

    // Modifiers state
    static bool isShiftPressed();
    static bool isCtrlPressed();
    static bool isAltPressed();
    static bool isFnPressed();
    static bool isSymbolPressed();

    // Input buffer for text entry
    static String getInputBuffer();
    static void clearInputBuffer();
    static void setInputBuffer(const String& str);

private:
    static uint8_t lastKey;
    static bool keyAvailable;
    static KeyEvent currentEvent;

    static bool shiftState;
    static bool ctrlState;
    static bool altState;
    static bool fnState;
    static bool symbolState;

    static int8_t trackballX;
    static int8_t trackballY;
    static bool trackballClick;

    static uint8_t backlight;
    static String inputBuffer;

    static uint8_t readKey();
    static char keyToChar(uint8_t key);
};

#endif // SHITBIRD_KEYBOARD_H
