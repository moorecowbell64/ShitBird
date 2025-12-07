/**
 * ShitBird Firmware - UI Manager Implementation
 */

#include "ui_manager.h"
#include "../core/display.h"
#include "../core/keyboard.h"
#include "../core/system.h"
#include "../core/storage.h"
#include "config.h"

#if ENABLE_WIFI
#include "../modules/wifi/wifi_module.h"
#endif

#if ENABLE_BLE
#include "../modules/ble/ble_module.h"
#endif

#if ENABLE_LORA
#include "../modules/lora/lora_module.h"
#endif

// Static member initialization
MenuScreen* UIManager::currentScreen = nullptr;
MenuScreen* UIManager::mainMenu = nullptr;
std::vector<MenuScreen*> UIManager::screenStack;

lv_obj_t* UIManager::lvMainScreen = nullptr;
lv_obj_t* UIManager::lvStatusBar = nullptr;
lv_obj_t* UIManager::lvContentArea = nullptr;
lv_obj_t* UIManager::lvProgressBar = nullptr;
lv_obj_t* UIManager::lvMessageBox = nullptr;

unsigned long UIManager::lastInputTime = 0;
bool UIManager::screenSleeping = false;

// ============================================================================
// MenuItem Implementation
// ============================================================================

MenuItem::MenuItem(const String& label, MenuCallback action, const uint8_t* icon)
    : label(label), type(MenuItemType::ACTION), action(action), submenu(nullptr), icon(icon) {}

MenuItem::MenuItem(const String& label, MenuScreen* submenu, const uint8_t* icon)
    : label(label), type(MenuItemType::SUBMENU), submenu(submenu), icon(icon) {}

MenuItem::MenuItem(const String& label, ToggleGetter getter, ToggleSetter setter, const uint8_t* icon)
    : label(label), type(MenuItemType::TOGGLE), getToggle(getter), setToggle(setter), icon(icon) {}

MenuItem MenuItem::back() {
    MenuItem item("< Back", nullptr);
    item.type = MenuItemType::BACK;
    return item;
}

// ============================================================================
// MenuScreen Implementation
// ============================================================================

MenuScreen::MenuScreen(const String& title, MenuScreen* parent)
    : title(title), parent(parent), selectedIndex(0), scrollOffset(0) {}

void MenuScreen::addItem(const MenuItem& item) {
    items.push_back(item);
}

void MenuScreen::draw() {
    TFT_eSPI* tft = Display::getTFT();
    ThemeColors colors = g_systemState.getThemeColors();

    // Clear content area (below status bar)
    tft->fillRect(0, 22, SCREEN_WIDTH, SCREEN_HEIGHT - 22, colors.bgPrimary);

    // Draw title bar
    tft->fillRect(0, 22, SCREEN_WIDTH, 20, colors.bgSecondary);
    tft->setTextColor(colors.accent);
    tft->setTextSize(1);
    tft->setCursor(5, 28);
    tft->print(title);

    // Draw menu items
    int itemHeight = 24;
    int startY = 44;
    int visibleItems = (SCREEN_HEIGHT - startY) / itemHeight;

    // Adjust scroll offset if needed
    if (selectedIndex < scrollOffset) {
        scrollOffset = selectedIndex;
    } else if (selectedIndex >= scrollOffset + visibleItems) {
        scrollOffset = selectedIndex - visibleItems + 1;
    }

    for (int i = 0; i < visibleItems && (i + scrollOffset) < items.size(); i++) {
        int idx = i + scrollOffset;
        MenuItem& item = items[idx];
        int y = startY + i * itemHeight;

        // Selection highlight
        if (idx == selectedIndex) {
            tft->fillRect(0, y, SCREEN_WIDTH, itemHeight - 2, colors.bgSecondary);
            tft->drawRect(0, y, SCREEN_WIDTH, itemHeight - 2, colors.accent);
        }

        // Item text
        tft->setTextColor(idx == selectedIndex ? colors.accent : colors.textPrimary);
        tft->setCursor(10, y + 6);
        tft->print(item.label);

        // Toggle state or submenu indicator
        if (item.type == MenuItemType::TOGGLE && item.getToggle) {
            bool state = item.getToggle();
            tft->setTextColor(state ? colors.success : colors.textSecondary);
            tft->setCursor(SCREEN_WIDTH - 30, y + 6);
            tft->print(state ? "ON" : "OFF");
        } else if (item.type == MenuItemType::SUBMENU) {
            tft->setTextColor(colors.textSecondary);
            tft->setCursor(SCREEN_WIDTH - 15, y + 6);
            tft->print(">");
        }
    }

    // Scroll indicators
    if (scrollOffset > 0) {
        tft->setTextColor(colors.accent);
        tft->setCursor(SCREEN_WIDTH - 10, startY);
        tft->print("^");
    }
    if (scrollOffset + visibleItems < items.size()) {
        tft->setTextColor(colors.accent);
        tft->setCursor(SCREEN_WIDTH - 10, SCREEN_HEIGHT - 15);
        tft->print("v");
    }

    // Update status bar
    Display::drawStatusBar();
}

void MenuScreen::handleInput(uint8_t key) {
    switch (key) {
        case KEY_UP:
        case TRACKBALL_UP:
            selectPrev();
            break;

        case KEY_DOWN:
        case TRACKBALL_DOWN:
            selectNext();
            break;

        case KEY_ENTER:
        case TRACKBALL_CLICK:
            activateSelected();
            break;

        case KEY_ESC:
        case KEY_BACKSPACE:
            if (parent) {
                UIManager::goBack();
            }
            break;
    }
}

void MenuScreen::selectNext() {
    if (selectedIndex < items.size() - 1) {
        selectedIndex++;
        draw();
    }
}

void MenuScreen::selectPrev() {
    if (selectedIndex > 0) {
        selectedIndex--;
        draw();
    }
}

void MenuScreen::activateSelected() {
    if (selectedIndex >= items.size()) return;

    MenuItem& item = items[selectedIndex];

    switch (item.type) {
        case MenuItemType::ACTION:
            if (item.action) {
                item.action();
            }
            break;

        case MenuItemType::SUBMENU:
            if (item.submenu) {
                UIManager::showScreen(item.submenu);
            }
            break;

        case MenuItemType::TOGGLE:
            if (item.getToggle && item.setToggle) {
                item.setToggle(!item.getToggle());
                draw();
            }
            break;

        case MenuItemType::BACK:
            UIManager::goBack();
            break;

        default:
            break;
    }
}

// ============================================================================
// UIManager Implementation
// ============================================================================

void UIManager::init() {
    Serial.println("[UI] Initializing...");

    lastInputTime = millis();

    // Build menu structure
    buildMainMenu();

    Serial.println("[UI] Initialized");
}

void UIManager::update() {
    // NOTE: Not calling Display::update() / lv_timer_handler() because
    // we're using direct TFT drawing, not LVGL widgets.
    // LVGL would clear our manually drawn content.

    // Handle keyboard input
    handleKeyInput();

    // Check for screen timeout
    checkScreenTimeout();
}

void UIManager::handleKeyInput() {
    if (Keyboard::hasKey()) {
        KeyEvent event = Keyboard::getKey();

        // Wake screen if sleeping
        if (screenSleeping) {
            screenSleeping = false;
            Display::wake();
            lastInputTime = millis();
            return;
        }

        lastInputTime = millis();

        // Check for panic sequence
        if (g_systemState.settings.security.panicWipeEnabled) {
            // TODO: Track key sequence for panic wipe
        }

        // Pass to current screen
        if (currentScreen) {
            currentScreen->handleInput(event.key);
        }
    }

    // Handle trackball
    int8_t tbY = Keyboard::getTrackballY();
    if (tbY != 0 && currentScreen) {
        if (tbY < 0) {
            currentScreen->selectPrev();
        } else {
            currentScreen->selectNext();
        }
        lastInputTime = millis();
    }

    if (Keyboard::isTrackballClicked() && currentScreen) {
        currentScreen->activateSelected();
        lastInputTime = millis();
    }
}

void UIManager::checkScreenTimeout() {
    uint16_t timeout = g_systemState.settings.display.sleepTimeout;
    if (timeout == 0) return;  // Disabled

    if (!screenSleeping && (millis() - lastInputTime > timeout * 1000)) {
        screenSleeping = true;
        Display::sleep();
    }
}

void UIManager::showMainMenu() {
    showScreen(mainMenu);
}

void UIManager::showScreen(MenuScreen* screen) {
    if (currentScreen) {
        screenStack.push_back(currentScreen);
    }
    currentScreen = screen;
    currentScreen->selectedIndex = 0;
    currentScreen->scrollOffset = 0;
    currentScreen->draw();
}

void UIManager::goBack() {
    if (!screenStack.empty()) {
        currentScreen = screenStack.back();
        screenStack.pop_back();
        currentScreen->draw();
    }
}

MenuScreen* UIManager::getCurrentScreen() {
    return currentScreen;
}

bool UIManager::showPinEntry() {
    TFT_eSPI* tft = Display::getTFT();
    ThemeColors colors = g_systemState.getThemeColors();

    String enteredPin = "";
    int attempts = 0;
    int maxAttempts = g_systemState.settings.security.maxAttempts;

    while (attempts < maxAttempts) {
        // Draw PIN entry screen
        tft->fillScreen(colors.bgPrimary);

        tft->setTextColor(colors.textPrimary);
        tft->setTextSize(2);
        tft->setCursor(80, 60);
        tft->print("Enter PIN");

        // Draw PIN dots
        tft->setTextSize(3);
        tft->setCursor(100, 100);
        for (int i = 0; i < SECURITY_PIN_LENGTH; i++) {
            if (i < enteredPin.length()) {
                tft->print("*");
            } else {
                tft->print("_");
            }
            tft->print(" ");
        }

        // Attempts remaining
        tft->setTextSize(1);
        tft->setTextColor(colors.warning);
        tft->setCursor(80, 160);
        tft->printf("Attempts: %d/%d", attempts + 1, maxAttempts);

        // Wait for input
        while (true) {
            Keyboard::update();

            if (Keyboard::hasKey()) {
                KeyEvent event = Keyboard::getKey();

                if (event.key >= '0' && event.key <= '9') {
                    if (enteredPin.length() < SECURITY_PIN_LENGTH) {
                        enteredPin += (char)event.key;

                        // Check if PIN is complete
                        if (enteredPin.length() == SECURITY_PIN_LENGTH) {
                            if (enteredPin == g_systemState.settings.security.pin) {
                                showMessage("Access Granted", "Welcome!", 1000);
                                return true;
                            } else {
                                attempts++;
                                enteredPin = "";
                                showMessage("Access Denied", "Incorrect PIN", 1000);
                                break;
                            }
                        }
                        break;
                    }
                } else if (event.key == KEY_BACKSPACE) {
                    if (enteredPin.length() > 0) {
                        enteredPin.remove(enteredPin.length() - 1);
                        break;
                    }
                }
            }

            delay(10);
        }
    }

    // Too many attempts - lockout
    showMessage("LOCKED", "Too many attempts", 3000);
    return false;
}

void UIManager::showLockScreen() {
    TFT_eSPI* tft = Display::getTFT();
    ThemeColors colors = g_systemState.getThemeColors();

    static unsigned long lastDraw = 0;
    if (millis() - lastDraw < 1000) return;
    lastDraw = millis();

    tft->fillScreen(colors.bgPrimary);

    tft->setTextColor(colors.error);
    tft->setTextSize(2);
    tft->setCursor(100, 80);
    tft->print("LOCKED");

    tft->setTextColor(colors.textSecondary);
    tft->setTextSize(1);
    tft->setCursor(60, 130);
    tft->print("Press any key to unlock");

    // Wait for key press
    if (Keyboard::hasKey()) {
        Keyboard::getKey();  // Consume key
        if (showPinEntry()) {
            g_systemState.locked = false;
            showMainMenu();
        }
    }
}

void UIManager::showMessage(const String& title, const String& message, uint16_t duration) {
    TFT_eSPI* tft = Display::getTFT();
    ThemeColors colors = g_systemState.getThemeColors();

    // Draw message box
    int boxW = 200;
    int boxH = 80;
    int boxX = (SCREEN_WIDTH - boxW) / 2;
    int boxY = (SCREEN_HEIGHT - boxH) / 2;

    tft->fillRect(boxX, boxY, boxW, boxH, colors.bgSecondary);
    tft->drawRect(boxX, boxY, boxW, boxH, colors.accent);

    tft->setTextColor(colors.accent);
    tft->setTextSize(1);
    int titleX = boxX + (boxW - title.length() * 6) / 2;
    tft->setCursor(titleX, boxY + 15);
    tft->print(title);

    tft->setTextColor(colors.textPrimary);
    int msgX = boxX + (boxW - message.length() * 6) / 2;
    tft->setCursor(msgX, boxY + 40);
    tft->print(message);

    delay(duration);

    // Redraw current screen
    if (currentScreen) {
        currentScreen->draw();
    }
}

void UIManager::showProgress(const String& title, uint8_t percent) {
    TFT_eSPI* tft = Display::getTFT();
    ThemeColors colors = g_systemState.getThemeColors();

    int boxW = 220;
    int boxH = 60;
    int boxX = (SCREEN_WIDTH - boxW) / 2;
    int boxY = (SCREEN_HEIGHT - boxH) / 2;

    tft->fillRect(boxX, boxY, boxW, boxH, colors.bgSecondary);
    tft->drawRect(boxX, boxY, boxW, boxH, colors.accent);

    tft->setTextColor(colors.textPrimary);
    tft->setTextSize(1);
    tft->setCursor(boxX + 10, boxY + 10);
    tft->print(title);

    // Progress bar
    int barX = boxX + 10;
    int barY = boxY + 30;
    int barW = boxW - 20;
    int barH = 15;

    tft->drawRect(barX, barY, barW, barH, colors.textSecondary);
    int fillW = (percent * (barW - 2)) / 100;
    tft->fillRect(barX + 1, barY + 1, fillW, barH - 2, colors.accent);

    // Percentage text
    char pctStr[8];
    snprintf(pctStr, sizeof(pctStr), "%d%%", percent);
    tft->setCursor(boxX + boxW - 35, boxY + 10);
    tft->print(pctStr);
}

void UIManager::hideProgress() {
    if (currentScreen) {
        currentScreen->draw();
    }
}

bool UIManager::showConfirm(const String& title, const String& message) {
    TFT_eSPI* tft = Display::getTFT();
    ThemeColors colors = g_systemState.getThemeColors();

    int boxW = 240;
    int boxH = 100;
    int boxX = (SCREEN_WIDTH - boxW) / 2;
    int boxY = (SCREEN_HEIGHT - boxH) / 2;

    tft->fillRect(boxX, boxY, boxW, boxH, colors.bgSecondary);
    tft->drawRect(boxX, boxY, boxW, boxH, colors.accent);

    tft->setTextColor(colors.accent);
    tft->setTextSize(1);
    tft->setCursor(boxX + 10, boxY + 10);
    tft->print(title);

    tft->setTextColor(colors.textPrimary);
    tft->setCursor(boxX + 10, boxY + 35);
    tft->print(message);

    // Buttons
    tft->setTextColor(colors.success);
    tft->setCursor(boxX + 30, boxY + 70);
    tft->print("[ENTER] Yes");

    tft->setTextColor(colors.error);
    tft->setCursor(boxX + 140, boxY + 70);
    tft->print("[ESC] No");

    // Wait for input
    while (true) {
        Keyboard::update();

        if (Keyboard::hasKey()) {
            KeyEvent event = Keyboard::getKey();

            if (event.key == KEY_ENTER || Keyboard::isTrackballClicked()) {
                if (currentScreen) currentScreen->draw();
                return true;
            } else if (event.key == KEY_ESC || event.key == KEY_BACKSPACE) {
                if (currentScreen) currentScreen->draw();
                return false;
            }
        }

        delay(10);
    }
}

String UIManager::showTextInput(const String& title, const String& defaultValue) {
    TFT_eSPI* tft = Display::getTFT();
    ThemeColors colors = g_systemState.getThemeColors();

    Keyboard::setInputBuffer(defaultValue);

    while (true) {
        // Draw input box
        tft->fillScreen(colors.bgPrimary);

        tft->setTextColor(colors.accent);
        tft->setTextSize(1);
        tft->setCursor(10, 30);
        tft->print(title);

        // Input field
        String input = Keyboard::getInputBuffer();
        tft->fillRect(10, 50, SCREEN_WIDTH - 20, 25, colors.bgSecondary);
        tft->drawRect(10, 50, SCREEN_WIDTH - 20, 25, colors.accent);
        tft->setTextColor(colors.textPrimary);
        tft->setCursor(15, 58);
        tft->print(input);
        tft->print("_");  // Cursor

        // Instructions
        tft->setTextColor(colors.textSecondary);
        tft->setCursor(10, 90);
        tft->print("ENTER to confirm, ESC to cancel");

        // Handle input
        Keyboard::update();

        if (Keyboard::hasKey()) {
            KeyEvent event = Keyboard::getKey();

            if (event.key == KEY_ENTER) {
                return Keyboard::getInputBuffer();
            } else if (event.key == KEY_ESC) {
                return "";
            }
            // Other keys handled by Keyboard::update()
        }

        delay(50);
    }
}

void UIManager::updateStatusBar() {
    Display::drawStatusBar();
}

// Menu building functions will be implemented separately to keep file size manageable
void UIManager::buildMainMenu() {
    mainMenu = new MenuScreen("ShitBird v" + String(FIRMWARE_VERSION));

    // BLE Menu (top priority)
    static MenuScreen* bleMenu = new MenuScreen("BLE Tools", mainMenu);
#if ENABLE_BLE
    BLEModule::buildMenu(bleMenu);
#endif
    mainMenu->addItem(MenuItem("BLE Tools", bleMenu));

    // WiFi Menu
    static MenuScreen* wifiMenu = new MenuScreen("WiFi Tools", mainMenu);
#if ENABLE_WIFI
    WiFiModule::buildMenu(wifiMenu);
#endif
    mainMenu->addItem(MenuItem("WiFi Tools", wifiMenu));

    // IR Menu
    static MenuScreen* irMenu = new MenuScreen("IR Tools", mainMenu);
    buildIRMenu();
    mainMenu->addItem(MenuItem("IR Tools", irMenu));

    // LoRa Menu
    static MenuScreen* loraMenu = new MenuScreen("LoRa Tools", mainMenu);
#if ENABLE_LORA
    LoRaModule::buildMenu(loraMenu);
#endif
    mainMenu->addItem(MenuItem("LoRa Tools", loraMenu));

    // BadUSB Menu
    static MenuScreen* badUSBMenu = new MenuScreen("BadUSB", mainMenu);
    buildBadUSBMenu();
    mainMenu->addItem(MenuItem("BadUSB", badUSBMenu));

    // RF Menu
    static MenuScreen* rfMenu = new MenuScreen("RF Tools", mainMenu);
    buildRFMenu();
    mainMenu->addItem(MenuItem("RF Tools", rfMenu));

    // Settings
    static MenuScreen* settingsMenu = new MenuScreen("Settings", mainMenu);
    buildSettingsMenu();
    mainMenu->addItem(MenuItem("Settings", settingsMenu));

    // About
    mainMenu->addItem(MenuItem("About", []() {
        UIManager::showMessage(FIRMWARE_NAME, "v" + String(FIRMWARE_VERSION), 3000);
    }));
}

void UIManager::buildBLEMenu() {
    // Will be populated by BLE module
}

void UIManager::buildWiFiMenu() {
    // WiFi menu is now built directly in buildMainMenu()
}

void UIManager::buildIRMenu() {
    // Will be populated by IR module
}

void UIManager::buildLoRaMenu() {
    // Will be populated by LoRa module
}

void UIManager::buildBadUSBMenu() {
    // Will be populated by BadUSB module
}

void UIManager::buildRFMenu() {
    // Will be populated by RF module
}

void UIManager::buildSettingsMenu() {
    // Get the settings menu from buildMainMenu
    static MenuScreen* settingsMenu = nullptr;
    for (auto& item : mainMenu->items) {
        if (item.label == "Settings" && item.submenu) {
            settingsMenu = item.submenu;
            break;
        }
    }
    if (!settingsMenu) return;

    // Display Settings submenu
    static MenuScreen* displayMenu = new MenuScreen("Display", settingsMenu);
    displayMenu->addItem(MenuItem("Brightness +", []() {
        uint8_t brightness = g_systemState.settings.display.brightness;
        if (brightness < 255) {
            brightness = min(255, brightness + 25);
            g_systemState.settings.display.brightness = brightness;
            Display::setBrightness(brightness);
        }
    }));
    displayMenu->addItem(MenuItem("Brightness -", []() {
        uint8_t brightness = g_systemState.settings.display.brightness;
        if (brightness > 25) {
            brightness = max(25, brightness - 25);
            g_systemState.settings.display.brightness = brightness;
            Display::setBrightness(brightness);
        }
    }));
    displayMenu->addItem(MenuItem::back());
    settingsMenu->addItem(MenuItem("Display", displayMenu));

    // Keyboard Settings
    static MenuScreen* kbMenu = new MenuScreen("Keyboard", settingsMenu);
    kbMenu->addItem(MenuItem("Backlight +", []() {
        uint8_t bl = Keyboard::getBacklight();
        if (bl < 255) {
            bl = min(255, bl + 25);
            Keyboard::setBacklight(bl);
        }
    }));
    kbMenu->addItem(MenuItem("Backlight -", []() {
        uint8_t bl = Keyboard::getBacklight();
        if (bl > 0) {
            bl = max(0, bl - 25);
            Keyboard::setBacklight(bl);
        }
    }));
    kbMenu->addItem(MenuItem::back());
    settingsMenu->addItem(MenuItem("Keyboard", kbMenu));

    // System Info
    settingsMenu->addItem(MenuItem("System Info", []() {
        String info = "Heap: " + String(ESP.getFreeHeap() / 1024) + "KB\n";
        info += "PSRAM: " + String(ESP.getFreePsram() / 1024) + "KB";
        UIManager::showMessage("System", info, 3000);
    }));

    // Reboot
    settingsMenu->addItem(MenuItem("Reboot", []() {
        if (UIManager::showConfirm("Reboot", "Restart device?")) {
            ESP.restart();
        }
    }));

    settingsMenu->addItem(MenuItem::back());
}

void UIManager::buildProfilesMenu() {
    // Will be implemented in settings module
}

void UIManager::buildSecurityMenu() {
    // Will be implemented in security module
}

void UIManager::buildDisplayMenu() {
    // Will be implemented in display settings
}

void UIManager::buildAboutMenu() {
    // Simple about screen
}
