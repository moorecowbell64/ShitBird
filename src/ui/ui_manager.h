/**
 * ShitBird Firmware - UI Manager
 * Handles menu navigation, screens, and user interaction
 */

#ifndef SHITBIRD_UI_MANAGER_H
#define SHITBIRD_UI_MANAGER_H

#include <Arduino.h>
#include <lvgl.h>
#include <vector>
#include <functional>
#include "config.h"

// Forward declarations
class MenuItem;
class MenuScreen;

// Menu item types
enum class MenuItemType {
    ACTION,         // Executes a function
    SUBMENU,        // Opens a submenu
    TOGGLE,         // On/Off toggle
    VALUE,          // Numeric value
    TEXT_INPUT,     // Text entry
    BACK            // Go back to parent menu
};

// Menu item callback
typedef std::function<void()> MenuCallback;
typedef std::function<bool()> ToggleGetter;
typedef std::function<void(bool)> ToggleSetter;

// Menu Item class
class MenuItem {
public:
    String label;
    String description;
    MenuItemType type;
    MenuCallback action;
    MenuScreen* submenu;
    ToggleGetter getToggle;
    ToggleSetter setToggle;
    const uint8_t* icon;  // 16x16 icon bitmap

    MenuItem(const String& label, MenuCallback action, const uint8_t* icon = nullptr);
    MenuItem(const String& label, MenuScreen* submenu, const uint8_t* icon = nullptr);
    MenuItem(const String& label, ToggleGetter getter, ToggleSetter setter, const uint8_t* icon = nullptr);

    static MenuItem back();
};

// Menu Screen class
class MenuScreen {
public:
    String title;
    std::vector<MenuItem> items;
    MenuScreen* parent;
    int selectedIndex;
    int scrollOffset;

    MenuScreen(const String& title, MenuScreen* parent = nullptr);

    void addItem(const MenuItem& item);
    void draw();
    void handleInput(uint8_t key);
    void selectNext();
    void selectPrev();
    void activateSelected();
};

// Main UI Manager
class UIManager {
public:
    static void init();
    static void update();

    // Screen management
    static void showMainMenu();
    static void showScreen(MenuScreen* screen);
    static void goBack();
    static MenuScreen* getCurrentScreen();

    // Special screens
    static bool showPinEntry();
    static void showLockScreen();
    static void showMessage(const String& title, const String& message, uint16_t duration = 2000);
    static void showProgress(const String& title, uint8_t percent);
    static void hideProgress();
    static bool showConfirm(const String& title, const String& message);
    static String showTextInput(const String& title, const String& defaultValue = "");

    // Status updates
    static void updateStatusBar();

    // LVGL integration
    static lv_obj_t* getMainScreen();

private:
    static MenuScreen* currentScreen;
    static MenuScreen* mainMenu;
    static std::vector<MenuScreen*> screenStack;

    static lv_obj_t* lvMainScreen;
    static lv_obj_t* lvStatusBar;
    static lv_obj_t* lvContentArea;
    static lv_obj_t* lvProgressBar;
    static lv_obj_t* lvMessageBox;

    static unsigned long lastInputTime;
    static bool screenSleeping;

    static void buildMainMenu();
    static void buildBLEMenu();
    static void buildWiFiMenu();
    static void buildIRMenu();
    static void buildLoRaMenu();
    static void buildBadUSBMenu();
    static void buildRFMenu();
    static void buildSettingsMenu();
    static void buildProfilesMenu();
    static void buildSecurityMenu();
    static void buildDisplayMenu();
    static void buildAboutMenu();

    static void checkScreenTimeout();
    static void handleKeyInput();
};

#endif // SHITBIRD_UI_MANAGER_H
