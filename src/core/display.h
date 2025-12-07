/**
 * ShitBird Firmware - Display Driver
 */

#ifndef SHITBIRD_DISPLAY_H
#define SHITBIRD_DISPLAY_H

#include <Arduino.h>
#include <TFT_eSPI.h>
#include <lvgl.h>
#include "config.h"
#include "system.h"

// Display buffer size (use SCREEN dimensions which are post-rotation)
#define DISP_BUF_SIZE (SCREEN_WIDTH * 40)

class Display {
public:
    static void init();
    static void setBrightness(uint8_t brightness);
    static void sleep();
    static void wake();
    static void clear();
    static void update();

    // Direct drawing methods (for splash, etc.)
    static void fillScreen(uint16_t color);
    static void drawText(int16_t x, int16_t y, const char* text, uint16_t color, uint8_t size = 1);
    static void drawRect(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t color);
    static void fillRect(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t color);
    static void drawLine(int16_t x0, int16_t y0, int16_t x1, int16_t y1, uint16_t color);
    static void drawPixel(int16_t x, int16_t y, uint16_t color);
    static void drawBitmap(int16_t x, int16_t y, const uint8_t* bitmap, int16_t w, int16_t h, uint16_t color);
    static void drawXBitmap(int16_t x, int16_t y, const uint8_t* bitmap, int16_t w, int16_t h, uint16_t color);

    // Status bar
    static void drawStatusBar();

    // Get TFT instance for advanced usage
    static TFT_eSPI* getTFT() { return &tft; }
    static lv_disp_t* getLVDisplay() { return disp; }

private:
    static TFT_eSPI tft;
    static lv_disp_drv_t disp_drv;
    static lv_disp_draw_buf_t draw_buf;
    static lv_color_t* buf1;
    static lv_color_t* buf2;
    static lv_disp_t* disp;
    static uint8_t currentBrightness;
    static bool sleeping;

    static void lvglFlushCb(lv_disp_drv_t* drv, const lv_area_t* area, lv_color_t* color_p);
};

#endif // SHITBIRD_DISPLAY_H
