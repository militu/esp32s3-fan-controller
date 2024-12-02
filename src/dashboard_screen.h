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

    // UI Elements - Labels
    lv_obj_t* tempLabel;
    lv_obj_t* modeLabel;
    lv_obj_t* wifiLabel;
    lv_obj_t* mqttLabel;
    lv_obj_t* nightLabel;
    lv_meter_indicator_t* temperatureIndicator;
    lv_obj_t* speedMeter;
    lv_obj_t* tempMeter;
    lv_obj_t* arcTempMeter;
    lv_meter_indicator_t* targetSpeedIndicator;
    lv_meter_indicator_t* currentSpeedIndicator;
    lv_obj_t* speedLabel;
    bool currentSpeedAnimationInProgress;
    bool targetSpeedAnimationInProgress;
    int targetSpeedValue;
    lv_obj_t* modeIndicator;
    int currentSpeedValue;

    // State tracking
    bool initialized;
    bool tempAnimationInProgress;
    int currentTempValue;
    SemaphoreHandle_t uiMutex;

    // Layout construction methods
    void createMainScreen();
    void createTopStatusBar(uint16_t height);
    void createMainContent(uint16_t startY, uint16_t height);
    void createSpeedMeter(uint16_t size, uint16_t xPosFromRight);
    void createTemperatureMeter(uint16_t size, uint16_t xPosFromLeft);

    // UI Helper methods
    lv_obj_t* createStatusLabel(lv_obj_t* parent, lv_align_t align, 
                               lv_coord_t x_ofs, lv_coord_t y_ofs, const char* text);

    // Update methods
    void updateTemperatureDisplay(float temp);
    void updateStatusIndicators(bool wifiConnected, bool mqttConnected, 
                              bool nightModeEnabled, bool nightModeActive);
    void updateSpeedDisplay(int fanSpeed, int targetSpeed);
    void updateModeDisplay(FanController::Mode mode);

    // Animation handling
    static void set_temp_value(void* obj, int32_t v) {
        lv_meter_set_indicator_end_value((lv_obj_t*)obj, ((DashboardScreen*)lv_obj_get_user_data(lv_obj_get_parent((lv_obj_t*)obj)))->temperatureIndicator, v);
    }

    static void set_current_speed_value(void* obj, int32_t v) {
        lv_meter_set_indicator_end_value((lv_obj_t*)obj, ((DashboardScreen*)lv_obj_get_user_data(lv_obj_get_parent((lv_obj_t*)obj)))->currentSpeedIndicator, v);
    }

    static void set_target_speed_value(void* obj, int32_t v) {
        lv_meter_set_indicator_end_value((lv_obj_t*)obj, ((DashboardScreen*)lv_obj_get_user_data(lv_obj_get_parent((lv_obj_t*)obj)))->targetSpeedIndicator, v);
    }

    // Status tracking
    struct StatusState {
        bool wifiConnected = false;
        bool mqttConnected = false;
        bool nightModeEnabled = false;
        bool nightModeActive = false;
    };
    StatusState lastStatus;

    // Utilities
    void addDebugPoint(lv_obj_t* parent, lv_coord_t x, lv_coord_t y, lv_color_t color);
    void addDebugPoints(lv_obj_t* meter_container, uint16_t size);

};

#endif // DASHBOARD_SCREEN_H