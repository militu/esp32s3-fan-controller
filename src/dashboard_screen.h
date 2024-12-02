#ifndef DASHBOARD_SCREEN_H
#define DASHBOARD_SCREEN_H

#include <Arduino.h>
#include "lvgl.h"
#include "fan_controller.h"
#include "fonts/icons.h"

/**
 * @brief Manages the main UI screen for system monitoring and control
 * 
 * Features:
 * - Temperature display with animated arc
 * - Status indicators for WiFi, MQTT, and night mode
 * - Fan speed and mode display
 * - Thread-safe UI updates
 * - Responsive layout
 */
class DashboardScreen {
public:
    DashboardScreen();
    ~DashboardScreen();

    // Screen initialization
    void init(uint16_t width, uint16_t height);
    void begin();

    // Core display interface
    void update(float temp, int fanSpeed, int targetSpeed, FanController::Mode mode,
               bool wifiConnected, bool mqttConnected, bool nightModeEnabled, bool nightModeActive);
    lv_obj_t* getArc();
    lv_obj_t* getScreen() { return screen; }
    SemaphoreHandle_t getUIMutex() const { return uiMutex; }

private:
    // Constants for FontAwesome icons
    static const char MY_MOON_SYMBOL[];
    static const char MY_TOWER_BROADCAST[];

    // Display dimensions
    uint16_t displayWidth;
    uint16_t displayHeight;

    // UI Elements - Main containers
    lv_obj_t* screen;
    lv_obj_t* arc;
    lv_obj_t* left_container;

    // UI Elements - Labels
    lv_obj_t* tempLabel;
    lv_obj_t* currentSpeedLabel;
    lv_obj_t* targetSpeedLabel;
    lv_obj_t* modeLabel;
    lv_obj_t* wifiLabel;
    lv_obj_t* mqttLabel;
    lv_obj_t* nightLabel;
    lv_meter_indicator_t* temperatureIndicator;
    lv_obj_t* speedMeter;
    lv_meter_indicator_t* targetSpeedIndicator;
    lv_meter_indicator_t* currentSpeedIndicator;
    lv_obj_t* speedLabel;
    bool speedAnimationInProgress;
    int currentTargetSpeed;

    // State tracking
    bool initialized;
    bool animationInProgress;
    int currentArcValue;
    SemaphoreHandle_t uiMutex;

    // Layout construction methods
    void createUI();
    void createMainScreen();
    void createTopStatusBar(uint16_t height);
    void createDelimiter(uint16_t topOffset, uint16_t height);
    void createMainContent(uint16_t startY, uint16_t height);
    void createLeftContainer(uint16_t width, uint16_t height, uint16_t xPos);
    void createTemperatureArc(uint16_t size, uint16_t xPos);
    void createRightContainer(uint16_t width, uint16_t height, uint16_t xPos, uint16_t yPos);
    void createStatusIndicators();
    void createSpeedMeter(uint16_t size, uint16_t xPos, uint16_t yPos);

    // UI Helper methods
    lv_obj_t* createStatusLabel(lv_obj_t* parent, lv_align_t align, 
                               lv_coord_t x_ofs, lv_coord_t y_ofs, const char* text);
    lv_obj_t* createInfoLabel(lv_obj_t* parent, lv_align_t align, 
                             lv_coord_t x_ofs, lv_coord_t y_ofs, const char* text);

    // Update methods
    void updateTemperatureDisplay(float temp);
    void updateStatusIndicators(bool wifiConnected, bool mqttConnected, 
                              bool nightModeEnabled, bool nightModeActive);
    void updateSpeedDisplay(int fanSpeed, int targetSpeed);
    void updateModeDisplay(FanController::Mode mode);

    // Animation handling
    lv_anim_t arcAnim;
    static void arcAnimCallback(void* var, int32_t value);

    // Status tracking
    struct StatusState {
        bool wifiConnected = false;
        bool mqttConnected = false;
        bool nightModeEnabled = false;
        bool nightModeActive = false;
    };
    StatusState lastStatus;
};

#endif // DASHBOARD_SCREEN_H