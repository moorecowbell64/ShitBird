/**
 * ShitBird Firmware - GPS Module Implementation
 */

#include "gps_module.h"
#include "../../core/system.h"
#include "../../ui/ui_manager.h"

// Static member initialization
TinyGPSPlus GPSModule::gps;
HardwareSerial* GPSModule::gpsSerial = nullptr;
bool GPSModule::initialized = false;
GPSData GPSModule::lastData = {0};

void GPSModule::init() {
    if (initialized) return;
    
    Serial.println("[GPS] Initializing...");
    
    // Use Serial1 for GPS on T-Deck Plus
    gpsSerial = &Serial1;
    gpsSerial->begin(GPS_BAUD, SERIAL_8N1, GPS_RX_PIN, GPS_TX_PIN);
    
    initialized = true;
    Serial.printf("[GPS] Initialized on pins RX:%d TX:%d @ %d baud\n", 
                  GPS_RX_PIN, GPS_TX_PIN, GPS_BAUD);
}

void GPSModule::update() {
    if (!initialized) return;
    
    // Process any available GPS data
    while (gpsSerial->available() > 0) {
        char c = gpsSerial->read();
        gps.encode(c);
    }
    
    // Update last data if we have a valid location
    processGPS();
}

void GPSModule::deinit() {
    if (!initialized) return;
    
    gpsSerial->end();
    initialized = false;
    Serial.println("[GPS] Deinitialized");
}

void GPSModule::processGPS() {
    lastData.valid = gps.location.isValid();
    lastData.age = gps.location.age();
    
    if (gps.location.isValid()) {
        lastData.latitude = gps.location.lat();
        lastData.longitude = gps.location.lng();
    }
    
    if (gps.altitude.isValid()) {
        lastData.altitude = gps.altitude.meters();
    }
    
    if (gps.speed.isValid()) {
        lastData.speed = gps.speed.kmph();
    }
    
    if (gps.course.isValid()) {
        lastData.course = gps.course.deg();
    }
    
    if (gps.satellites.isValid()) {
        lastData.satellites = gps.satellites.value();
    }
    
    if (gps.hdop.isValid()) {
        lastData.hdop = gps.hdop.value();
    }
    
    if (gps.date.isValid()) {
        lastData.year = gps.date.year();
        lastData.month = gps.date.month();
        lastData.day = gps.date.day();
    }
    
    if (gps.time.isValid()) {
        lastData.hour = gps.time.hour();
        lastData.minute = gps.time.minute();
        lastData.second = gps.time.second();
    }
}

bool GPSModule::isInitialized() {
    return initialized;
}

bool GPSModule::hasFix() {
    return lastData.valid && lastData.age < 2000;
}

uint32_t GPSModule::getSatellites() {
    return lastData.satellites;
}

GPSData GPSModule::getData() {
    return lastData;
}

double GPSModule::getLatitude() {
    return lastData.latitude;
}

double GPSModule::getLongitude() {
    return lastData.longitude;
}

double GPSModule::getAltitude() {
    return lastData.altitude;
}

double GPSModule::getSpeed() {
    return lastData.speed;
}

double GPSModule::getCourse() {
    return lastData.course;
}

String GPSModule::getPositionString() {
    if (!hasFix()) {
        return "No GPS fix";
    }
    
    char buf[64];
    snprintf(buf, sizeof(buf), "%.6f, %.6f", lastData.latitude, lastData.longitude);
    return String(buf);
}

String GPSModule::getTimeString() {
    if (!gps.time.isValid()) {
        return "--:--:--";
    }
    
    char buf[16];
    snprintf(buf, sizeof(buf), "%02d:%02d:%02d", 
             lastData.hour, lastData.minute, lastData.second);
    return String(buf);
}

String GPSModule::getDateString() {
    if (!gps.date.isValid()) {
        return "--/--/----";
    }
    
    char buf[16];
    snprintf(buf, sizeof(buf), "%02d/%02d/%04d",
             lastData.month, lastData.day, lastData.year);
    return String(buf);
}

String GPSModule::getMaidenhead() {
    if (!hasFix()) {
        return "------";
    }
    return toMaidenhead(lastData.latitude, lastData.longitude);
}

String GPSModule::toMaidenhead(double lat, double lon) {
    // Convert lat/lon to Maidenhead grid locator (6 character)
    char grid[7];
    
    lon += 180.0;
    lat += 90.0;
    
    grid[0] = 'A' + (int)(lon / 20.0);
    grid[1] = 'A' + (int)(lat / 10.0);
    grid[2] = '0' + (int)(fmod(lon, 20.0) / 2.0);
    grid[3] = '0' + (int)(fmod(lat, 10.0));
    grid[4] = 'a' + (int)(fmod(lon, 2.0) * 12.0);
    grid[5] = 'a' + (int)(fmod(lat, 1.0) * 24.0);
    grid[6] = '\0';
    
    return String(grid);
}

void GPSModule::buildMenu(void* menuPtr) {
    MenuScreen* menu = static_cast<MenuScreen*>(menuPtr);
    
    menu->addItem(MenuItem("Show Position", []() {
        String pos = GPSModule::getPositionString();
        UIManager::showMessage("GPS Position", pos, 5000);
    }));
    
    menu->addItem(MenuItem("Show Altitude", []() {
        if (GPSModule::hasFix()) {
            String alt = String(GPSModule::getAltitude(), 1) + " m";
            UIManager::showMessage("Altitude", alt, 3000);
        } else {
            UIManager::showMessage("GPS", "No fix", 2000);
        }
    }));
    
    menu->addItem(MenuItem("Show Speed", []() {
        if (GPSModule::hasFix()) {
            String spd = String(GPSModule::getSpeed(), 1) + " km/h";
            UIManager::showMessage("Speed", spd, 3000);
        } else {
            UIManager::showMessage("GPS", "No fix", 2000);
        }
    }));
    
    menu->addItem(MenuItem("Show Grid Locator", []() {
        String grid = GPSModule::getMaidenhead();
        UIManager::showMessage("Grid Locator", grid, 3000);
    }));
    
    menu->addItem(MenuItem("Show Satellites", []() {
        String sats = String(GPSModule::getSatellites()) + " satellites";
        if (GPSModule::hasFix()) {
            sats += " (fix)";
        }
        UIManager::showMessage("GPS Status", sats, 3000);
    }));
    
    menu->addItem(MenuItem("Show Time (UTC)", []() {
        String time = GPSModule::getTimeString();
        String date = GPSModule::getDateString();
        UIManager::showMessage("GPS Time", date + "\n" + time, 3000);
    }));
    
    menu->addItem(MenuItem::back());
}
