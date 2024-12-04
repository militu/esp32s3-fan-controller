#ifndef DISPLAY_MANAGER_H
#define DISPLAY_MANAGER_H

/*******************************************************************************
 * Includes
 ******************************************************************************/
// Core includes
#include <Arduino.h>
#include "lvgl.h"

// Display components
#include "display_driver.h"
#include "dashboard_screen.h"
#include "boot_screen.h"
#include "debug_log.h"

// System components
#include "fan_controller.h"
#include "temp_sensor.h"
#include "wifi_manager.h"
#include "mqtt_manager.h"

/*******************************************************************************
 * Display Manager Class
 ******************************************************************************/
/**
 * Manages display initialization, updates, and interface with LVGL
 * Coordinates between system components and display UI
 */
class DisplayManager {
public:
    DisplayManager(TaskManager& tm, TempSensor& ts, FanController& fc, WifiManager& wm, MqttManager& mm);
    bool begin(DisplayDriver* displayDriver);
    enum class DisplayState {
        BOOT,
        DASHBOARD
    };

    void updateBootStatus(const char* component, BootScreen::ComponentStatus status);
    void updateBootStatusDetail(const char* component, 
                                BootScreen::ComponentStatus status,
                                const char* detail);
    void switchToDashboardUI();

    // WiFi status methods
    void showWifiInitializing();
    void showWifiConnecting(uint8_t attempt, uint8_t maxAttempts);
    void showWifiConnected(const char* ssid, const IPAddress& ip);
    void showWifiFailed(const char* reason);
    
    // NTP status methods
    void showNTPInitializing();
    void showNTPSyncing(uint8_t attempt, uint8_t maxAttempts);
    void showNTPSynced(const String& timeStr);
    void showNTPFailed(const char* reason);
    
    // MQTT status methods
    void showMQTTInitializing();
    void showMQTTConnecting(uint8_t attempt, uint8_t maxAttempts);
    void showMQTTConnected();
    void showMQTTFailed(const char* reason);

private:
    TaskManager& taskManager;
    TempSensor& tempSensor;
    FanController& fanController;
    WifiManager& wifiManager;
    MqttManager& mqttManager;

    DisplayDriver* driver;
    bool initialized;

    DisplayState currentState;
    DashboardScreen dashboardUI;
    BootScreen bootUI;

    // LVGL and uiUpdate task handling
    static void displayRenderTask(void* parameters);
    void processDisplayRender();
    static void displayUpdateTask(void* parameters);
    void processDisplayUpdates();

    static void flush_cb(lv_disp_drv_t* disp_drv, const lv_area_t* area, lv_color_t* color_p);
    void updateDashboardValues();

    struct DisplayUpdateCommand {
        enum class CommandType {
            UPDATE_DISPLAY,
        };

        CommandType type;
        float temperature;
        uint8_t currentSpeed;
        uint8_t targetSpeed;
        FanController::Mode controlMode;
        bool wifiConnected;
        bool mqttConnected;
        bool nightModeEnabled;
        bool nightModeActive;

        DisplayUpdateCommand() {} // Default constructor
        
        DisplayUpdateCommand(float temp, uint8_t current, uint8_t target, 
                 FanController::Mode mode, bool wifi, 
                 bool mqtt, bool nightEnabled, bool nightActive)
            : type(CommandType::UPDATE_DISPLAY)
            , temperature(temp)
            , currentSpeed(current)
            , targetSpeed(target)
            , controlMode(mode)
            , wifiConnected(wifi)
            , mqttConnected(mqtt)
            , nightModeEnabled(nightEnabled)
            , nightModeActive(nightActive) {}
    };

    QueueHandle_t DisplayUpdateCommandQueue;

    lv_obj_t* currentScreen;
    bool needsScreenTransition;

    void initializeBootScreen();

    void showComponentStatus(const char* component, 
                             BootScreen::ComponentStatus status,
                             const char* detail);
};

#endif // DISPLAY_MANAGER_H