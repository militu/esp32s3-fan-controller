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
#include "display_ui.h"

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

private:
    TaskManager& taskManager;
    TempSensor& tempSensor;
    FanController& fanController;
    WifiManager& wifiManager;
    MqttManager& mqttManager;
    
    DisplayDriver* driver;
    DisplayUI ui;
    bool initialized;
    
    // LVGL task handling
    static void lvglTask(void* parameters);
    void processLVGL();
    static void flush_cb(lv_disp_drv_t* disp_drv, const lv_area_t* area, lv_color_t* color_p);
    void updateDisplayValues();

    // Constants
    static constexpr uint32_t LVGL_STACK_SIZE = 4096;
    static constexpr UBaseType_t LVGL_TASK_PRIORITY = 1;  // Higher than other tasks
    static constexpr BaseType_t LVGL_TASK_CORE = 1;
    static constexpr uint32_t UPDATE_INTERVAL = 10;  // 20Hz updates

    struct UICommand {
        enum class CommandType {
            UPDATE_DISPLAY,  // Matches your current updateDisplayValues use case
            // We can add more specific command types as needed
        };
        
        CommandType type;
        float temperature;
        uint8_t currentSpeed;
        uint8_t targetSpeed;
        FanController::Mode controlMode;
        bool wifiConnected;
        bool mqttConnected;
        bool nightMode;
        
        UICommand() {} // Default constructor
        
        // Constructor that matches your current update pattern
        UICommand(float temp, uint8_t current, uint8_t target, 
                 FanController::Mode mode, bool wifi, 
                 bool mqtt, bool night)
            : type(CommandType::UPDATE_DISPLAY)
            , temperature(temp)
            , currentSpeed(current)
            , targetSpeed(target)
            , controlMode(mode)
            , wifiConnected(wifi)
            , mqttConnected(mqtt)
            , nightMode(night) {}
    };

    QueueHandle_t uiCommandQueue;
    static const uint8_t QUEUE_SIZE = 5;  // Adjust based on update frequency

};

#endif // DISPLAY_MANAGER_H