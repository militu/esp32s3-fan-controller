#ifndef DISPLAY_UI_H
#define DISPLAY_UI_H

/*******************************************************************************
 * Includes
 ******************************************************************************/
// Core includes
#include <Arduino.h>
#include "lvgl.h"

// System includes
#include "fan_controller.h"

/*******************************************************************************
 * DisplayUI Class
 ******************************************************************************/
/**
 * Manages the LVGL-based user interface for the fan controller display
 * Handles creation and updating of all UI elements
 */
class DisplayUI {
public:
    /**
     * Constructor - initializes UI elements to null
     */
    DisplayUI();

    /**
     * Initialize and create all UI elements
     */
    void begin();

    /**
     * Update all UI elements with current system state
     * @param temp Current temperature reading
     * @param fanSpeed Current fan speed percentage
     * @param targetSpeed Target fan speed percentage
     * @param mode Current fan control mode (Auto/Manual)
     * @param wifiConnected WiFi connection status
     * @param mqttConnected MQTT connection status
     * @param nightMode Night mode status
     */
    void update(float temp, int fanSpeed, int targetSpeed, FanController::Mode mode,
                bool wifiConnected, bool mqttConnected, bool nightMode);

    /**
     * Get pointer to the temperature arc widget
     * @return Pointer to the LVGL arc object
     */
    lv_obj_t* getArc();

private:
    /**
     * Create and configure all UI elements
     */
    void createUI();

    // Main container elements
    lv_obj_t* screen;          // Main screen container
    lv_obj_t* arc;            // Temperature display arc

    // Labels for displaying system status
    lv_obj_t* tempLabel;       // Temperature value
    lv_obj_t* currentSpeedLabel; // Current fan speed
    lv_obj_t* targetSpeedLabel;  // Target fan speed
    lv_obj_t* modeLabel;        // Operation mode
    
    // Status indicator labels
    lv_obj_t* wifiLabel;       // WiFi connection status
    lv_obj_t* mqttLabel;       // MQTT connection status
    lv_obj_t* nightLabel;      // Night mode status
    
    // State tracking
    bool initialized;          // UI initialization state
};

#endif // DISPLAY_UI_H