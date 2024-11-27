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
    /**
     * Constructor takes references to all required system components
     */
    DisplayManager(TempSensor& ts, FanController& fc, WifiManager& wm, MqttManager& mm);


    /**
     * Initialize the display system with the provided driver
     * @param driver Pointer to display driver implementation
     * @return true if initialization successful
     */
    bool begin(DisplayDriver* displayDriver);
    
    /**
     * Main processing function, should be called in loop
     */
    void process();
    
private:
    // System component references
    TempSensor& tempSensor;
    FanController& fanController;
    WifiManager& wifiManager;
    MqttManager& mqttManager;
    
    // Display components
    DisplayDriver* driver;
    DisplayUI ui;
    
    // Update timing control
    lv_timer_t* updateTimer;
    uint32_t lastUpdate;
    static constexpr uint32_t UPDATE_INTERVAL = 16;  // 100ms refresh rate
    
    // State tracking
    bool initialized;
    
    // LVGL callbacks
    static void lvglTimerCallback(lv_timer_t* timer);
    static void flush_cb(lv_disp_drv_t* disp_drv, const lv_area_t* area, lv_color_t* color_p);
    
    // Internal update methods
    void updateDisplayValues();
};

#endif // DISPLAY_MANAGER_H