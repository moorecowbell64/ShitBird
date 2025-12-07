/**
 * ShitBird Firmware - Display Driver Implementation
 */

#include "display.h"

// Static member initialization
TFT_eSPI Display::tft = TFT_eSPI();
lv_disp_drv_t Display::disp_drv;
lv_disp_draw_buf_t Display::draw_buf;
lv_color_t* Display::buf1 = nullptr;
lv_color_t* Display::buf2 = nullptr;
lv_disp_t* Display::disp = nullptr;
uint8_t Display::currentBrightness = 200;
bool Display::sleeping = false;

void Display::lvglFlushCb(lv_disp_drv_t* drv, const lv_area_t* area, lv_color_t* color_p) {
    uint32_t w = (area->x2 - area->x1 + 1);
    uint32_t h = (area->y2 - area->y1 + 1);

    tft.startWrite();
    tft.setAddrWindow(area->x1, area->y1, w, h);
    tft.pushPixels((uint16_t*)color_p, w * h);
    tft.endWrite();

    lv_disp_flush_ready(drv);
}

void Display::init() {
    Serial.println("[DISPLAY] Initializing...");

    // Initialize backlight pin
    pinMode(TFT_BL_PIN, OUTPUT);
    ledcSetup(0, 5000, 8);
    ledcAttachPin(TFT_BL_PIN, 0);
    ledcWrite(0, 0);  // Start dark

    // Initialize TFT
    tft.init();
    tft.setRotation(1);  // Landscape mode
    tft.fillScreen(TFT_BLACK);

    // Turn on backlight
    setBrightness(currentBrightness);

    // Initialize LVGL
    lv_init();

    // Allocate display buffers in PSRAM if available
    size_t bufSize = SCREEN_WIDTH * 40;
    if (psramFound()) {
        buf1 = (lv_color_t*)ps_malloc(bufSize * sizeof(lv_color_t));
        buf2 = (lv_color_t*)ps_malloc(bufSize * sizeof(lv_color_t));
        Serial.println("[DISPLAY] Using PSRAM for display buffers");
    } else {
        buf1 = (lv_color_t*)malloc(bufSize * sizeof(lv_color_t));
        buf2 = nullptr;
        Serial.println("[DISPLAY] Using heap for display buffers");
    }

    if (!buf1) {
        Serial.println("[DISPLAY] ERROR: Failed to allocate display buffer!");
        return;
    }

    lv_disp_draw_buf_init(&draw_buf, buf1, buf2, bufSize);

    // Initialize display driver
    lv_disp_drv_init(&disp_drv);
    disp_drv.hor_res = SCREEN_WIDTH;   // 320
    disp_drv.ver_res = SCREEN_HEIGHT;  // 240
    disp_drv.flush_cb = lvglFlushCb;
    disp_drv.draw_buf = &draw_buf;

    disp = lv_disp_drv_register(&disp_drv);

    Serial.println("[DISPLAY] Initialized successfully");
}

void Display::setBrightness(uint8_t brightness) {
    currentBrightness = brightness;
    ledcWrite(0, brightness);
}

void Display::sleep() {
    if (!sleeping) {
        sleeping = true;
        ledcWrite(0, 0);
        // TODO: Put display controller in sleep mode
    }
}

void Display::wake() {
    if (sleeping) {
        sleeping = false;
        ledcWrite(0, currentBrightness);
        // TODO: Wake display controller
    }
}

void Display::clear() {
    tft.fillScreen(TFT_BLACK);
}

void Display::update() {
    lv_timer_handler();
}

void Display::fillScreen(uint16_t color) {
    tft.fillScreen(color);
}

void Display::drawText(int16_t x, int16_t y, const char* text, uint16_t color, uint8_t size) {
    tft.setTextColor(color);
    tft.setTextSize(size);
    tft.setCursor(x, y);
    tft.print(text);
}

void Display::drawRect(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t color) {
    tft.drawRect(x, y, w, h, color);
}

void Display::fillRect(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t color) {
    tft.fillRect(x, y, w, h, color);
}

void Display::drawLine(int16_t x0, int16_t y0, int16_t x1, int16_t y1, uint16_t color) {
    tft.drawLine(x0, y0, x1, y1, color);
}

void Display::drawPixel(int16_t x, int16_t y, uint16_t color) {
    tft.drawPixel(x, y, color);
}

void Display::drawBitmap(int16_t x, int16_t y, const uint8_t* bitmap, int16_t w, int16_t h, uint16_t color) {
    tft.drawBitmap(x, y, bitmap, w, h, color);
}

void Display::drawXBitmap(int16_t x, int16_t y, const uint8_t* bitmap, int16_t w, int16_t h, uint16_t color) {
    tft.drawXBitmap(x, y, bitmap, w, h, color);
}

void Display::drawStatusBar() {
    ThemeColors colors = g_systemState.getThemeColors();

    // Draw status bar background
    tft.fillRect(0, 0, SCREEN_WIDTH, 20, colors.bgSecondary);

    // Battery indicator
    char batStr[8];
    snprintf(batStr, sizeof(batStr), "%d%%", g_systemState.batteryPercent);
    tft.setTextColor(colors.textPrimary);
    tft.setTextSize(1);
    tft.setCursor(SCREEN_WIDTH - 35, 6);
    tft.print(batStr);

    // Battery icon
    int batX = SCREEN_WIDTH - 50;
    tft.drawRect(batX, 5, 12, 8, colors.textPrimary);
    tft.fillRect(batX + 12, 7, 2, 4, colors.textPrimary);
    int fillWidth = (g_systemState.batteryPercent * 10) / 100;
    uint16_t batColor = g_systemState.batteryPercent > 20 ? colors.success : colors.error;
    tft.fillRect(batX + 1, 6, fillWidth, 6, batColor);

    // WiFi status
    if (g_systemState.settings.wifi.enabled) {
        tft.setTextColor(g_systemState.wifiConnected ? colors.success : colors.textSecondary);
        tft.setCursor(5, 6);
        tft.print("WiFi");
    }

    // BLE status
    if (g_systemState.settings.ble.enabled) {
        tft.setTextColor(g_systemState.bleConnected ? colors.success : colors.textSecondary);
        tft.setCursor(40, 6);
        tft.print("BLE");
    }

    // LoRa status
    if (g_systemState.settings.lora.enabled) {
        tft.setTextColor(g_systemState.loraActive ? colors.success : colors.textSecondary);
        tft.setCursor(70, 6);
        tft.print("LoRa");
    }

    // GPS status
    if (g_systemState.gpsFixed) {
        tft.setTextColor(colors.success);
        tft.setCursor(110, 6);
        tft.print("GPS");
    }

    // SD card status
    if (g_systemState.sdMounted) {
        tft.setTextColor(colors.success);
        tft.setCursor(145, 6);
        tft.print("SD");
    }

    // Current mode indicator
    const char* modeStr = "";
    switch (g_systemState.currentMode) {
        case OperationMode::WIFI_SCAN:   modeStr = "[SCAN]"; break;
        case OperationMode::WIFI_ATTACK: modeStr = "[ATK]"; break;
        case OperationMode::BLE_SCAN:    modeStr = "[BLE]"; break;
        case OperationMode::BLE_ATTACK:  modeStr = "[BLE-A]"; break;
        case OperationMode::LORA_SCAN:   modeStr = "[LoRa]"; break;
        case OperationMode::IR_TX:       modeStr = "[IR]"; break;
        case OperationMode::BADUSB:      modeStr = "[USB]"; break;
        default: break;
    }
    if (strlen(modeStr) > 0) {
        tft.setTextColor(colors.accent);
        tft.setCursor(170, 6);
        tft.print(modeStr);
    }
}
