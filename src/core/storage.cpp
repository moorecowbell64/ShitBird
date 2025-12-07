/**
 * ShitBird Firmware - Storage Manager Implementation
 */

#include "storage.h"
#include "system.h"
#include <vector>
#include <time.h>

bool Storage::mounted = false;
SPIClass* Storage::spi = nullptr;

bool Storage::init() {
    Serial.println("[STORAGE] Initializing SD card...");

    // Use HSPI for SD card (shared with display and LoRa)
    spi = new SPIClass(HSPI);
    spi->begin(SD_SCLK_PIN, SD_MISO_PIN, SD_MOSI_PIN, SD_CS_PIN);

    // Initialize SD card
    if (!SD.begin(SD_CS_PIN, *spi, 25000000)) {
        Serial.println("[STORAGE] SD card mount failed!");
        mounted = false;
        g_systemState.sdMounted = false;
        return false;
    }

    uint8_t cardType = SD.cardType();
    if (cardType == CARD_NONE) {
        Serial.println("[STORAGE] No SD card detected!");
        mounted = false;
        g_systemState.sdMounted = false;
        return false;
    }

    String cardTypeStr;
    switch (cardType) {
        case CARD_MMC:  cardTypeStr = "MMC"; break;
        case CARD_SD:   cardTypeStr = "SD"; break;
        case CARD_SDHC: cardTypeStr = "SDHC"; break;
        default:        cardTypeStr = "Unknown"; break;
    }

    Serial.printf("[STORAGE] Card type: %s\n", cardTypeStr.c_str());
    Serial.printf("[STORAGE] Card size: %s\n", formatBytes(SD.cardSize()).c_str());
    Serial.printf("[STORAGE] Total: %s, Used: %s, Free: %s\n",
                  formatBytes(getTotalBytes()).c_str(),
                  formatBytes(getUsedBytes()).c_str(),
                  formatBytes(getFreeBytes()).c_str());

    mounted = true;
    g_systemState.sdMounted = true;

    // Create directory structure
    createDirectories();

    Serial.println("[STORAGE] SD card initialized");
    return true;
}

void Storage::deinit() {
    if (mounted) {
        SD.end();
        mounted = false;
        g_systemState.sdMounted = false;
    }
    if (spi) {
        spi->end();
        delete spi;
        spi = nullptr;
    }
}

bool Storage::isMounted() {
    return mounted;
}

void Storage::createDirectories() {
    const char* dirs[] = {
        PATH_LOGS,
        PATH_PCAP,
        PATH_PAYLOADS,
        PATH_IR_CODES,
        PATH_LORA,
        PATH_SETTINGS,
        PATH_THEMES
    };

    for (const char* dir : dirs) {
        if (!SD.exists(dir)) {
            SD.mkdir(dir);
            Serial.printf("[STORAGE] Created directory: %s\n", dir);
        }
    }
}

bool Storage::exists(const char* path) {
    if (!mounted) return false;
    return SD.exists(path);
}

bool Storage::mkdir(const char* path) {
    if (!mounted) return false;
    return SD.mkdir(path);
}

bool Storage::remove(const char* path) {
    if (!mounted) return false;
    return SD.remove(path);
}

bool Storage::rename(const char* oldPath, const char* newPath) {
    if (!mounted) return false;
    return SD.rename(oldPath, newPath);
}

File Storage::openDir(const char* path) {
    return SD.open(path);
}

std::vector<String> Storage::listFiles(const char* path, const char* extension) {
    std::vector<String> files;

    if (!mounted) return files;

    File dir = SD.open(path);
    if (!dir || !dir.isDirectory()) {
        return files;
    }

    File entry;
    while ((entry = dir.openNextFile())) {
        if (!entry.isDirectory()) {
            String name = entry.name();
            if (extension == nullptr ||
                name.endsWith(extension)) {
                files.push_back(name);
            }
        }
        entry.close();
    }
    dir.close();

    return files;
}

String Storage::readFile(const char* path) {
    if (!mounted) return "";

    File file = SD.open(path, FILE_READ);
    if (!file) {
        return "";
    }

    String content = file.readString();
    file.close();
    return content;
}

bool Storage::writeFile(const char* path, const char* content) {
    if (!mounted) return false;

    File file = SD.open(path, FILE_WRITE);
    if (!file) {
        return false;
    }

    size_t written = file.print(content);
    file.close();
    return written > 0;
}

bool Storage::appendFile(const char* path, const char* content) {
    if (!mounted) return false;

    File file = SD.open(path, FILE_APPEND);
    if (!file) {
        return false;
    }

    size_t written = file.print(content);
    file.close();
    return written > 0;
}

bool Storage::writeBytes(const char* path, const uint8_t* data, size_t len) {
    if (!mounted) return false;

    File file = SD.open(path, FILE_WRITE);
    if (!file) {
        return false;
    }

    size_t written = file.write(data, len);
    file.close();
    return written == len;
}

size_t Storage::readBytes(const char* path, uint8_t* buffer, size_t maxLen) {
    if (!mounted) return 0;

    File file = SD.open(path, FILE_READ);
    if (!file) {
        return 0;
    }

    size_t read = file.read(buffer, maxLen);
    file.close();
    return read;
}

bool Storage::createPcapFile(const char* path, uint32_t linkType) {
    if (!mounted) return false;

    File file = SD.open(path, FILE_WRITE);
    if (!file) {
        return false;
    }

    PcapFileHeader header = {
        .magic = PCAP_MAGIC,
        .versionMajor = PCAP_VERSION_MAJOR,
        .versionMinor = PCAP_VERSION_MINOR,
        .thiszone = 0,
        .sigfigs = 0,
        .snaplen = 65535,
        .network = linkType
    };

    size_t written = file.write((uint8_t*)&header, sizeof(header));
    file.close();

    return written == sizeof(header);
}

bool Storage::writePcapPacket(const char* path, const uint8_t* data, uint32_t len) {
    if (!mounted) return false;

    File file = SD.open(path, FILE_APPEND);
    if (!file) {
        return false;
    }

    bool result = writePcapPacket(file, data, len);
    file.close();
    return result;
}

bool Storage::writePcapPacket(File& file, const uint8_t* data, uint32_t len) {
    struct timeval tv;
    gettimeofday(&tv, nullptr);

    PcapPacketHeader pktHeader = {
        .tsSec = (uint32_t)tv.tv_sec,
        .tsUsec = (uint32_t)tv.tv_usec,
        .inclLen = len,
        .origLen = len
    };

    size_t written = file.write((uint8_t*)&pktHeader, sizeof(pktHeader));
    if (written != sizeof(pktHeader)) {
        return false;
    }

    written = file.write(data, len);
    return written == len;
}

bool Storage::log(const char* category, const char* message) {
    if (!mounted) return false;

    String path = getLogFilePath(category);

    // Check log rotation
    File file = SD.open(path, FILE_READ);
    if (file) {
        if (file.size() > LOG_MAX_FILE_SIZE) {
            file.close();
            rotateLogFiles(category);
        } else {
            file.close();
        }
    }

    // Get timestamp
    struct tm timeinfo;
    time_t now = time(nullptr);
    localtime_r(&now, &timeinfo);

    char timestamp[32];
    strftime(timestamp, sizeof(timestamp), "%Y-%m-%d %H:%M:%S", &timeinfo);

    // Format log entry
    char entry[512];
    snprintf(entry, sizeof(entry), "[%s] %s\n", timestamp, message);

    return appendFile(path.c_str(), entry);
}

bool Storage::logf(const char* category, const char* format, ...) {
    char message[256];
    va_list args;
    va_start(args, format);
    vsnprintf(message, sizeof(message), format, args);
    va_end(args);

    return log(category, message);
}

String Storage::getLogFilePath(const char* category) {
    char path[64];
    snprintf(path, sizeof(path), "%s/%s.log", PATH_LOGS, category);
    return String(path);
}

void Storage::rotateLogFiles(const char* category) {
    char oldPath[64], newPath[64];

    // Delete oldest log
    snprintf(oldPath, sizeof(oldPath), "%s/%s.%d.log", PATH_LOGS, category, LOG_ROTATE_COUNT - 1);
    if (SD.exists(oldPath)) {
        SD.remove(oldPath);
    }

    // Rotate existing logs
    for (int i = LOG_ROTATE_COUNT - 2; i >= 0; i--) {
        if (i == 0) {
            snprintf(oldPath, sizeof(oldPath), "%s/%s.log", PATH_LOGS, category);
        } else {
            snprintf(oldPath, sizeof(oldPath), "%s/%s.%d.log", PATH_LOGS, category, i);
        }
        snprintf(newPath, sizeof(newPath), "%s/%s.%d.log", PATH_LOGS, category, i + 1);

        if (SD.exists(oldPath)) {
            SD.rename(oldPath, newPath);
        }
    }
}

uint64_t Storage::getTotalBytes() {
    if (!mounted) return 0;
    return SD.totalBytes();
}

uint64_t Storage::getUsedBytes() {
    if (!mounted) return 0;
    return SD.usedBytes();
}

uint64_t Storage::getFreeBytes() {
    return getTotalBytes() - getUsedBytes();
}

String Storage::formatBytes(uint64_t bytes) {
    if (bytes < 1024) {
        return String(bytes) + " B";
    } else if (bytes < 1048576) {
        return String(bytes / 1024.0, 1) + " KB";
    } else if (bytes < 1073741824) {
        return String(bytes / 1048576.0, 1) + " MB";
    } else {
        return String(bytes / 1073741824.0, 1) + " GB";
    }
}

bool Storage::writeEncrypted(const char* path, const uint8_t* data, size_t len, const char* key) {
    // Simple XOR encryption for now
    // TODO: Implement AES-256 encryption
    uint8_t* encrypted = new uint8_t[len];
    size_t keyLen = strlen(key);

    for (size_t i = 0; i < len; i++) {
        encrypted[i] = data[i] ^ key[i % keyLen];
    }

    bool result = writeBytes(path, encrypted, len);
    delete[] encrypted;
    return result;
}

size_t Storage::readEncrypted(const char* path, uint8_t* buffer, size_t maxLen, const char* key) {
    size_t len = readBytes(path, buffer, maxLen);
    if (len == 0) return 0;

    size_t keyLen = strlen(key);
    for (size_t i = 0; i < len; i++) {
        buffer[i] = buffer[i] ^ key[i % keyLen];
    }

    return len;
}

bool Storage::secureWipe() {
    if (!mounted) return false;

    Serial.println("[STORAGE] SECURE WIPE INITIATED!");

    // Wipe sensitive directories
    wipeDirectory(PATH_LOGS);
    wipeDirectory(PATH_PCAP);
    wipeDirectory(PATH_PAYLOADS);
    wipeDirectory(PATH_SETTINGS);

    Serial.println("[STORAGE] Secure wipe complete");
    return true;
}

bool Storage::wipeDirectory(const char* path) {
    if (!mounted) return false;

    File dir = SD.open(path);
    if (!dir || !dir.isDirectory()) {
        return false;
    }

    File entry;
    while ((entry = dir.openNextFile())) {
        String filePath = String(path) + "/" + entry.name();

        if (entry.isDirectory()) {
            wipeDirectory(filePath.c_str());
            SD.rmdir(filePath.c_str());
        } else {
            // Overwrite with zeros before deletion
            size_t size = entry.size();
            entry.close();

            if (size > 0) {
                uint8_t zeros[512] = {0};
                File f = SD.open(filePath, FILE_WRITE);
                if (f) {
                    for (size_t i = 0; i < size; i += sizeof(zeros)) {
                        size_t toWrite = min(sizeof(zeros), size - i);
                        f.write(zeros, toWrite);
                    }
                    f.close();
                }
            }
            SD.remove(filePath.c_str());
        }
    }
    dir.close();

    return true;
}
