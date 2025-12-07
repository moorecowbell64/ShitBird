/**
 * ShitBird Firmware - Keyboard Driver Implementation
 */

#include "keyboard.h"
#include "system.h"

// Static member initialization
uint8_t Keyboard::lastKey = KEY_NONE;
bool Keyboard::keyAvailable = false;
KeyEvent Keyboard::currentEvent = {0};

bool Keyboard::shiftState = false;
bool Keyboard::ctrlState = false;
bool Keyboard::altState = false;
bool Keyboard::fnState = false;
bool Keyboard::symbolState = false;

int8_t Keyboard::trackballX = 0;
int8_t Keyboard::trackballY = 0;
bool Keyboard::trackballClick = false;

uint8_t Keyboard::backlight = 128;
String Keyboard::inputBuffer = "";

// Volatile variables for trackball GPIO interrupts
static volatile int8_t tb_delta_x = 0;
static volatile int8_t tb_delta_y = 0;
static volatile bool tb_clicked = false;

// Trackball interrupt handlers (must be in IRAM for ESP32)
void IRAM_ATTR onTrackballUp() {
    tb_delta_y--;
}

void IRAM_ATTR onTrackballDown() {
    tb_delta_y++;
}

void IRAM_ATTR onTrackballLeft() {
    tb_delta_x--;
}

void IRAM_ATTR onTrackballRight() {
    tb_delta_x++;
}

void IRAM_ATTR onTrackballClick() {
    tb_clicked = true;
}

void Keyboard::init() {
    Serial.println("[KEYBOARD] Initializing...");

    // I2C should already be initialized, but ensure it is
    Wire.begin(I2C_SDA_PIN, I2C_SCL_PIN, I2C_FREQ);

    // Set keyboard interrupt pin as input
    pinMode(KB_INT_PIN, INPUT_PULLUP);

    // Set keyboard backlight
    Wire.beginTransmission(KB_I2C_ADDR);
    Wire.write(0x01);  // Brightness command
    Wire.write(128);   // Medium brightness
    Wire.endTransmission();

    // Initialize trackball GPIO pins with interrupts
    Serial.println("[KEYBOARD] Setting up trackball GPIO...");

    pinMode(TBOX_UP_PIN, INPUT_PULLUP);
    pinMode(TBOX_DOWN_PIN, INPUT_PULLUP);
    pinMode(TBOX_LEFT_PIN, INPUT_PULLUP);
    pinMode(TBOX_RIGHT_PIN, INPUT_PULLUP);

    // Attach interrupts - FALLING edge since pins are pulled up
    attachInterrupt(digitalPinToInterrupt(TBOX_UP_PIN), onTrackballUp, FALLING);
    attachInterrupt(digitalPinToInterrupt(TBOX_DOWN_PIN), onTrackballDown, FALLING);
    attachInterrupt(digitalPinToInterrupt(TBOX_LEFT_PIN), onTrackballLeft, FALLING);
    attachInterrupt(digitalPinToInterrupt(TBOX_RIGHT_PIN), onTrackballRight, FALLING);

    // Trackball click is on BOOT_PIN (GPIO 0)
    pinMode(BOOT_PIN, INPUT_PULLUP);
    attachInterrupt(digitalPinToInterrupt(BOOT_PIN), onTrackballClick, FALLING);

    Serial.printf("[KEYBOARD] Trackball pins: UP=%d DOWN=%d LEFT=%d RIGHT=%d CLICK=%d\n",
                  TBOX_UP_PIN, TBOX_DOWN_PIN, TBOX_LEFT_PIN, TBOX_RIGHT_PIN, BOOT_PIN);

    Serial.println("[KEYBOARD] Initialized");
}

void Keyboard::update() {
    // First, check for trackball GPIO movement and click (from interrupts)
    noInterrupts();
    int8_t dx = tb_delta_x;
    int8_t dy = tb_delta_y;
    bool clicked = tb_clicked;
    tb_delta_x = 0;
    tb_delta_y = 0;
    tb_clicked = false;
    interrupts();

    if (dx != 0 || dy != 0) {
        trackballX = dx;
        trackballY = dy;
        Serial.printf("[TB] dx=%d dy=%d\n", dx, dy);
        g_systemState.recordActivity();
    }

    if (clicked) {
        trackballClick = true;
        Serial.println("[TB] CLICK");
        g_systemState.recordActivity();
    }

    // Check for keyboard key press via I2C
    uint8_t key = readKey();

    // Only process if we got a non-zero key that's different from last
    if (key == KEY_NONE || key == 0) {
        lastKey = 0;
        return;
    }

    // Debounce - don't repeat the same key too fast
    if (key == lastKey) {
        return;
    }
    lastKey = key;

    // Debug: print all key codes
    Serial.printf("[KB] 0x%02X\n", key);

    // Regular key press
    keyAvailable = true;
    currentEvent.key = key;
    currentEvent.pressed = true;

    // Record activity
    g_systemState.recordActivity();
}

uint8_t Keyboard::readKey() {
    // Read one byte from keyboard controller
    uint8_t bytesRead = Wire.requestFrom((uint8_t)KB_I2C_ADDR, (uint8_t)1);

    if (bytesRead > 0 && Wire.available()) {
        return Wire.read();
    }

    return KEY_NONE;
}

bool Keyboard::hasKey() {
    return keyAvailable;
}

KeyEvent Keyboard::getKey() {
    keyAvailable = false;
    return currentEvent;
}

char Keyboard::getChar() {
    if (!keyAvailable) return 0;

    keyAvailable = false;
    return keyToChar(currentEvent.key);
}

char Keyboard::keyToChar(uint8_t key) {
    // Basic QWERTY mapping
    // The ESP32-C3 keyboard controller handles most of the mapping
    // but we may need to apply shift/symbol modifiers

    if (key >= 'a' && key <= 'z') {
        if (shiftState) {
            return key - 32;  // Uppercase
        }
        return key;
    }

    if (key >= '0' && key <= '9') {
        if (symbolState) {
            // Number row symbols
            const char symbols[] = ")!@#$%^&*(";
            return symbols[key - '0'];
        }
        return key;
    }

    // Special characters with shift
    if (shiftState) {
        switch (key) {
            case '.': return '>';
            case ',': return '<';
            case '/': return '?';
            case ';': return ':';
            case '\'': return '"';
            case '[': return '{';
            case ']': return '}';
            case '-': return '_';
            case '=': return '+';
            case '\\': return '|';
            case '`': return '~';
        }
    }

    // Pass through printable characters
    if (key >= 0x20 && key <= 0x7E) {
        return key;
    }

    // Control characters
    switch (key) {
        case KEY_ENTER: return '\n';
        case KEY_TAB: return '\t';
        case KEY_SPACE: return ' ';
        case KEY_BACKSPACE: return '\b';
    }

    return 0;  // Non-printable
}

int8_t Keyboard::getTrackballX() {
    int8_t x = trackballX;
    trackballX = 0;
    return x;
}

int8_t Keyboard::getTrackballY() {
    int8_t y = trackballY;
    trackballY = 0;
    return y;
}

bool Keyboard::isTrackballClicked() {
    bool clicked = trackballClick;
    trackballClick = false;
    return clicked;
}

void Keyboard::resetTrackball() {
    trackballX = 0;
    trackballY = 0;
    trackballClick = false;
}

void Keyboard::setBacklight(uint8_t brightness) {
    backlight = brightness;

    Wire.beginTransmission(KB_I2C_ADDR);
    Wire.write(KB_CMD_BRIGHTNESS);
    Wire.write(brightness);
    Wire.endTransmission();
}

uint8_t Keyboard::getBacklight() {
    return backlight;
}

bool Keyboard::isShiftPressed() { return shiftState; }
bool Keyboard::isCtrlPressed() { return ctrlState; }
bool Keyboard::isAltPressed() { return altState; }
bool Keyboard::isFnPressed() { return fnState; }
bool Keyboard::isSymbolPressed() { return symbolState; }

String Keyboard::getInputBuffer() { return inputBuffer; }
void Keyboard::clearInputBuffer() { inputBuffer = ""; }
void Keyboard::setInputBuffer(const String& str) { inputBuffer = str; }
