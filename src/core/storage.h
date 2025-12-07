/**
 * ShitBird Firmware - Storage Manager (SD Card)
 */

#ifndef SHITBIRD_STORAGE_H
#define SHITBIRD_STORAGE_H

#include <Arduino.h>
#include <SD.h>
#include <FS.h>
#include <vector>
#include "config.h"

// File paths
#define PATH_ROOT           "/"
#define PATH_LOGS           "/logs"
#define PATH_PCAP           "/pcap"
#define PATH_PAYLOADS       "/payloads"
#define PATH_IR_CODES       "/ir_codes"
#define PATH_LORA           "/lora"
#define PATH_SETTINGS       "/settings"
#define PATH_THEMES         "/themes"

// PCAP file header magic
#define PCAP_MAGIC          0xA1B2C3D4
#define PCAP_VERSION_MAJOR  2
#define PCAP_VERSION_MINOR  4
#define PCAP_LINKTYPE_IEEE802_11  105
#define PCAP_LINKTYPE_BLUETOOTH   201

// PCAP file header structure
struct __attribute__((packed)) PcapFileHeader {
    uint32_t magic;
    uint16_t versionMajor;
    uint16_t versionMinor;
    int32_t  thiszone;
    uint32_t sigfigs;
    uint32_t snaplen;
    uint32_t network;
};

// PCAP packet header structure
struct __attribute__((packed)) PcapPacketHeader {
    uint32_t tsSec;
    uint32_t tsUsec;
    uint32_t inclLen;
    uint32_t origLen;
};

class Storage {
public:
    static bool init();
    static void deinit();
    static bool isMounted();

    // File operations
    static bool exists(const char* path);
    static bool mkdir(const char* path);
    static bool remove(const char* path);
    static bool rename(const char* oldPath, const char* newPath);

    // Directory operations
    static File openDir(const char* path);
    static std::vector<String> listFiles(const char* path, const char* extension = nullptr);

    // File read/write
    static String readFile(const char* path);
    static bool writeFile(const char* path, const char* content);
    static bool appendFile(const char* path, const char* content);
    static bool writeBytes(const char* path, const uint8_t* data, size_t len);
    static size_t readBytes(const char* path, uint8_t* buffer, size_t maxLen);

    // PCAP operations
    static bool createPcapFile(const char* path, uint32_t linkType = PCAP_LINKTYPE_IEEE802_11);
    static bool writePcapPacket(const char* path, const uint8_t* data, uint32_t len);
    static bool writePcapPacket(File& file, const uint8_t* data, uint32_t len);

    // Log operations
    static bool log(const char* category, const char* message);
    static bool logf(const char* category, const char* format, ...);
    static String getLogFilePath(const char* category);

    // Utility
    static uint64_t getTotalBytes();
    static uint64_t getUsedBytes();
    static uint64_t getFreeBytes();
    static String formatBytes(uint64_t bytes);

    // Encryption (for secure logs)
    static bool writeEncrypted(const char* path, const uint8_t* data, size_t len, const char* key);
    static size_t readEncrypted(const char* path, uint8_t* buffer, size_t maxLen, const char* key);

    // Secure wipe
    static bool secureWipe();
    static bool wipeDirectory(const char* path);

private:
    static bool mounted;
    static SPIClass* spi;

    static void createDirectories();
    static void rotateLogFiles(const char* category);
};

#endif // SHITBIRD_STORAGE_H
