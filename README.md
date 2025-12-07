# ShitBird Firmware

Custom firmware for LilyGo T-Deck Plus (ESP32-S3) with WiFi, BLE, and LoRa capabilities.

## Features

- **WiFi Tools**: Network scanning, monitor mode
- **BLE Tools**: Device scanning, advertising
- **LoRa Tools**: 915MHz SX1262 radio, Meshtastic node functionality
- **Settings**: Display brightness, keyboard backlight, system info

## Hardware

- **Board**: LilyGo T-Deck Plus
- **MCU**: ESP32-S3FN16R8
- **Display**: 2.8" IPS LCD (320x240, ST7789)
- **Radio**: Semtech SX1262 LoRa (915MHz)
- **Input**: QWERTY keyboard + trackball

## Building

Requires PlatformIO:

```bash
# Build
pio run

# Upload
pio run -t upload

# Monitor serial
pio device monitor
```

## Pin Configuration

See `include/config.h` for complete pin definitions.

## License

MIT License
