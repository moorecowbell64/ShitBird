/**
 * ShitBird Firmware - Main Configuration
 * Target: LilyGo T-Deck Plus (ESP32-S3)
 */

#ifndef SHITBIRD_CONFIG_H
#define SHITBIRD_CONFIG_H

// ============================================================================
// FIRMWARE INFO
// ============================================================================
#define FIRMWARE_NAME       "ShitBird"
#define FIRMWARE_VERSION    "1.0.0"
#define FIRMWARE_AUTHOR     "Custom Build"
#define FIRMWARE_CODENAME   "Woodpecker"

// ============================================================================
// HARDWARE CONFIGURATION - T-Deck Plus
// ============================================================================

// --- Display (ST7789) ---
// Note: TFT_WIDTH/HEIGHT defined in platformio.ini build flags for TFT_eSPI
#define SCREEN_WIDTH        320
#define SCREEN_HEIGHT       240
#define TFT_CS_PIN          12
#define TFT_DC_PIN          11
#define TFT_MOSI_PIN        41
#define TFT_MISO_PIN        38
#define TFT_SCLK_PIN        40
#define TFT_BL_PIN          42
#define TFT_RST_PIN         -1

// --- I2C Bus ---
#define I2C_SDA_PIN         18
#define I2C_SCL_PIN         8
#define I2C_FREQ            400000

// --- Keyboard (ESP32-C3) ---
#define KB_I2C_ADDR         0x55
#define KB_INT_PIN          46
#define KB_BACKLIGHT_CMD    0x01

// --- LoRa Radio (SX1262) ---
#define LORA_CS_PIN         9
#define LORA_BUSY_PIN       13
#define LORA_RST_PIN        17
#define LORA_DIO1_PIN       45
#define LORA_MOSI_PIN       41
#define LORA_MISO_PIN       38
#define LORA_SCLK_PIN       40

// LoRa Configuration (915MHz for US)
#define LORA_FREQUENCY      915.0   // MHz
#define LORA_BANDWIDTH      125.0   // kHz
#define LORA_SPREAD_FACTOR  7
#define LORA_CODING_RATE    5
#define LORA_SYNC_WORD      0x12
#define LORA_TX_POWER       22      // dBm (max for SX1262)

// --- SD Card ---
#define SD_CS_PIN           39
#define SD_MOSI_PIN         41
#define SD_MISO_PIN         38
#define SD_SCLK_PIN         40

// --- GPS (MIA-M10Q) ---
#define GPS_RX_PIN          44
#define GPS_TX_PIN          43
#define GPS_BAUD            9600

// --- Audio ---
#define I2S_WS_PIN          5
#define I2S_BCK_PIN         7
#define I2S_DOUT_PIN        6
#define ES7210_MCLK_PIN     48
#define ES7210_LRCK_PIN     21
#define ES7210_SCK_PIN      47
#define ES7210_DIN_PIN      14
#define ES7210_I2C_ADDR     0x40

// --- Power Management ---
#define POWER_ON_PIN        10
#define BAT_ADC_PIN         4
#define BOOT_PIN            0

// --- Trackball GPIO (T-Deck Plus uses dedicated GPIO, not keyboard I2C) ---
// Corrected mapping: UP=3 worked, RIGHT was triggering DOWN action
// So: GPIO2 (old DOWN) should be RIGHT, GPIO1 (old RIGHT) should be DOWN
#define TBOX_UP_PIN         3       // GPIO3 - UP (confirmed working)
#define TBOX_DOWN_PIN       15      // GPIO15 - try as DOWN
#define TBOX_LEFT_PIN       1       // GPIO1 - try as LEFT
#define TBOX_RIGHT_PIN      2       // GPIO2 - was triggering DOWN, so this is probably RIGHT

// --- IR (External - assign to available GPIO) ---
// NOTE: GPIO 1 and 2 conflict with trackball, use different pins for IR
#define IR_TX_PIN           43      // GPIO43 (TX) for IR transmitter - reassigned
#define IR_RX_PIN           44      // GPIO44 (RX) for IR receiver - reassigned

// --- Touch Controller (CST328) ---
#define TOUCH_I2C_ADDR      0x1A
#define TOUCH_INT_PIN       16

// ============================================================================
// FEATURE TOGGLES
// ============================================================================
// Module toggles
#define ENABLE_WIFI         1
#define ENABLE_BLE          1
#define ENABLE_LORA         1
#define ENABLE_GPS          0
#define ENABLE_IR           0
#define ENABLE_BADUSB       0
#define ENABLE_SD           0
#define ENABLE_AUDIO        0
#define ENABLE_OTA          0
#define ENABLE_WEB_SERVER   0

// ============================================================================
// WIFI ATTACK CONFIGURATION
// ============================================================================
#define WIFI_DEAUTH_REASON      1       // Unspecified reason
#define WIFI_DEAUTH_INTERVAL    100     // ms between packets
#define WIFI_BEACON_INTERVAL    100     // ms between beacons
#define WIFI_MAX_TARGETS        64      // Max APs/clients to track
#define WIFI_CHANNEL_HOP_TIME   200     // ms per channel
#define WIFI_PCAP_BUFFER_SIZE   8192    // Bytes for pcap buffer

// ============================================================================
// BLE ATTACK CONFIGURATION
// ============================================================================
#define BLE_SCAN_DURATION       10      // seconds
#define BLE_SPAM_INTERVAL       20      // ms between spam packets
#define BLE_MAX_DEVICES         100     // Max tracked devices

// ============================================================================
// SECURITY CONFIGURATION
// ============================================================================
#define SECURITY_PIN_LENGTH     6
#define SECURITY_MAX_ATTEMPTS   3
#define SECURITY_LOCKOUT_TIME   300     // seconds
#define ENCRYPT_SD_LOGS         1
#define AUTO_LOCK_TIMEOUT       300     // seconds (0 = disabled)

// ============================================================================
// WEB SERVER CONFIGURATION
// ============================================================================
#define WEB_SERVER_PORT         80
#define WEB_AP_SSID             "ShitBird-AP"
#define WEB_AP_PASS             "shitbird123"
#define WEB_AP_CHANNEL          6
#define WEB_AP_HIDDEN           0
#define WEB_MAX_CLIENTS         4

// ============================================================================
// LOGGING CONFIGURATION
// ============================================================================
#define LOG_TO_SERIAL           1
#define LOG_TO_SD               1
#define LOG_MAX_FILE_SIZE       10485760    // 10MB per log file
#define LOG_ROTATE_COUNT        5           // Number of log files to keep

// ============================================================================
// UI CONFIGURATION
// ============================================================================
#define UI_FPS                  30
#define UI_MENU_ITEMS_VISIBLE   6
#define UI_SCROLL_SPEED         3
#define UI_ANIMATION_ENABLED    1
#define UI_SPLASH_DURATION      2000    // ms

// ============================================================================
// DEFAULT THEME (can be changed in settings)
// ============================================================================
#define THEME_DEFAULT           "hacker"

// Hacker Theme Colors (Green on Black)
#define COLOR_BG_PRIMARY        0x0000  // Black
#define COLOR_BG_SECONDARY      0x0841  // Dark gray
#define COLOR_TEXT_PRIMARY      0x07E0  // Green
#define COLOR_TEXT_SECONDARY    0x03E0  // Dark green
#define COLOR_ACCENT            0x07FF  // Cyan
#define COLOR_WARNING           0xFBE0  // Yellow
#define COLOR_ERROR             0xF800  // Red
#define COLOR_SUCCESS           0x07E0  // Green

#endif // SHITBIRD_CONFIG_H
