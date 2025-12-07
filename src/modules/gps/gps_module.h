/**
 * ShitBird Firmware - GPS Module
 * GPS position tracking and display
 */

#ifndef SHITBIRD_GPS_MODULE_H
#define SHITBIRD_GPS_MODULE_H

#include <Arduino.h>
#include <TinyGPSPlus.h>
#include "config.h"

struct GPSData {
    double latitude;
    double longitude;
    double altitude;
    double speed;       // km/h
    double course;      // degrees
    uint32_t satellites;
    uint32_t hdop;
    bool valid;
    uint32_t age;       // ms since last update
    
    // Date/Time
    uint16_t year;
    uint8_t month;
    uint8_t day;
    uint8_t hour;
    uint8_t minute;
    uint8_t second;
};

class GPSModule {
public:
    static void init();
    static void update();
    static void deinit();
    
    // Status
    static bool isInitialized();
    static bool hasFix();
    static uint32_t getSatellites();
    
    // Position data
    static GPSData getData();
    static double getLatitude();
    static double getLongitude();
    static double getAltitude();
    static double getSpeed();
    static double getCourse();
    
    // Formatting
    static String getPositionString();
    static String getTimeString();
    static String getDateString();
    static String getMaidenhead();  // Grid locator
    
    // Menu integration
    static void buildMenu(void* menuScreen);

private:
    static TinyGPSPlus gps;
    static HardwareSerial* gpsSerial;
    static bool initialized;
    static GPSData lastData;
    
    static void processGPS();
    static String toMaidenhead(double lat, double lon);
};

#endif // SHITBIRD_GPS_MODULE_H
