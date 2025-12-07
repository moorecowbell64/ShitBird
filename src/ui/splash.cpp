/**
 * ShitBird Firmware - Splash Screen Implementation
 */

#include "splash.h"
#include "../core/display.h"
#include "../core/system.h"
#include "config.h"

void Splash::show() {
    TFT_eSPI* tft = Display::getTFT();

    // Get theme colors
    ThemeColors colors = g_systemState.getThemeColors();

    // Clear screen
    tft->fillScreen(colors.bgPrimary);

    // Animate in the splash
    animateIn();

    // Hold for splash duration
    delay(UI_SPLASH_DURATION);
}

void Splash::hide() {
    TFT_eSPI* tft = Display::getTFT();

    // Fade out effect
    for (int i = 255; i >= 0; i -= 15) {
        Display::setBrightness(i);
        delay(20);
    }

    // Clear to black
    tft->fillScreen(TFT_BLACK);

    // Restore brightness (use 200 as default if settings not loaded)
    uint8_t brightness = g_systemState.settings.display.brightness;
    if (brightness == 0) brightness = 200;
    Display::setBrightness(brightness);
}

void Splash::animateIn() {
    TFT_eSPI* tft = Display::getTFT();
    ThemeColors colors = g_systemState.getThemeColors();

    // Start dark
    Display::setBrightness(0);

    // Draw logo centered
    int logoX = (SCREEN_WIDTH - LOGO_WIDTH) / 2;
    int logoY = (SCREEN_HEIGHT - LOGO_HEIGHT) / 2 - 30;

    drawWoodpecker(logoX, logoY);

    // Draw text below logo
    drawText();

    // Fade in
    for (int i = 0; i <= g_systemState.settings.display.brightness; i += 10) {
        Display::setBrightness(i);
        delay(30);
    }
}

void Splash::drawWoodpecker(int16_t x, int16_t y) {
    TFT_eSPI* tft = Display::getTFT();

    // Colors from the geometric bird logo
    uint16_t red = 0xF800;      // Bright red
    uint16_t yellow = 0xFFE0;   // Yellow
    uint16_t blue = 0x001F;     // Blue
    uint16_t white = 0xFFFF;    // White
    uint16_t black = 0x0000;    // Black

    // Scale factor for 64x64 area (original is ~200px, we scale to 64)
    // We'll draw the geometric rooster logo

    // === HEAD (red triangular crest) ===
    // Main head triangle pointing down-right
    tft->fillTriangle(
        x + 15, y + 5,      // top-left spike
        x + 40, y + 30,     // bottom point
        x + 25, y + 30,     // left side
        red
    );

    // Second spike
    tft->fillTriangle(
        x + 20, y + 0,      // top spike
        x + 35, y + 25,     // bottom
        x + 30, y + 10,     // mid
        red
    );

    // Third spike (tallest)
    tft->fillTriangle(
        x + 28, y + 0,
        x + 42, y + 20,
        x + 35, y + 5,
        red
    );

    // Main face area (red)
    tft->fillTriangle(
        x + 20, y + 15,     // top
        x + 45, y + 35,     // right
        x + 20, y + 35,     // bottom-left
        red
    );

    // Lower face triangle
    tft->fillTriangle(
        x + 20, y + 25,
        x + 42, y + 35,
        x + 20, y + 42,
        red
    );

    // === BEAK (yellow triangle) ===
    tft->fillTriangle(
        x + 42, y + 25,     // left point
        x + 58, y + 32,     // right tip
        x + 42, y + 38,     // bottom-left
        yellow
    );

    // === EYE ===
    // Outer white circle
    tft->fillCircle(x + 35, y + 28, 7, white);
    // Black ring
    tft->drawCircle(x + 35, y + 28, 7, black);
    tft->drawCircle(x + 35, y + 28, 6, black);
    // Blue pupil
    tft->fillCircle(x + 35, y + 28, 4, blue);

    // === BODY ===
    // Red body rectangle (left side)
    tft->fillRect(x + 15, y + 42, 18, 20, red);

    // Yellow body rectangle (right side)
    tft->fillRect(x + 33, y + 42, 12, 20, yellow);

    // Black outlines for body
    tft->drawRect(x + 15, y + 42, 18, 20, black);
    tft->drawRect(x + 33, y + 42, 12, 20, black);
    tft->drawFastHLine(x + 33, y + 52, 12, black);

    // === TAIL (blue triangles) ===
    // Upper tail feather
    tft->fillTriangle(
        x + 15, y + 45,     // right point
        x + 0, y + 42,      // top-left
        x + 5, y + 52,      // bottom
        blue
    );

    // Lower tail feather
    tft->fillTriangle(
        x + 15, y + 52,     // right
        x + 0, y + 55,      // left
        x + 8, y + 62,      // bottom
        blue
    );

    // Blue accent on body
    tft->fillTriangle(
        x + 20, y + 48,
        x + 30, y + 55,
        x + 20, y + 60,
        blue
    );

    // Black outlines for crispness
    tft->drawTriangle(x + 42, y + 25, x + 58, y + 32, x + 42, y + 38, black);
}

void Splash::drawText() {
    TFT_eSPI* tft = Display::getTFT();
    ThemeColors colors = g_systemState.getThemeColors();

    // Title - "ShitBird"
    tft->setTextColor(colors.textPrimary);
    tft->setTextSize(3);

    String title = FIRMWARE_NAME;
    int titleWidth = title.length() * 18;  // Approximate width
    int titleX = (SCREEN_WIDTH - titleWidth) / 2;
    int titleY = SCREEN_HEIGHT / 2 + 30;

    tft->setCursor(titleX, titleY);
    tft->print(title);

    // Version
    tft->setTextSize(1);
    tft->setTextColor(colors.textSecondary);

    String version = "v" + String(FIRMWARE_VERSION);
    int versionWidth = version.length() * 6;
    int versionX = (SCREEN_WIDTH - versionWidth) / 2;

    tft->setCursor(versionX, titleY + 30);
    tft->print(version);

    // Tagline
    tft->setTextColor(colors.accent);
    String tagline = "Penetration Testing Toolkit";
    int taglineWidth = tagline.length() * 6;
    int taglineX = (SCREEN_WIDTH - taglineWidth) / 2;

    tft->setCursor(taglineX, titleY + 45);
    tft->print(tagline);

    // Bottom text
    tft->setTextColor(colors.textSecondary);
    tft->setCursor(10, SCREEN_HEIGHT - 15);
    tft->print("For authorized testing only");
}

void Splash::drawLogo(int16_t x, int16_t y) {
    drawWoodpecker(x, y);
}
