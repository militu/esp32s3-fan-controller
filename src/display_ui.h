#ifndef DISPLAY_UI_H
#define DISPLAY_UI_H

/*******************************************************************************
 * Includes
 ******************************************************************************/
#include <Arduino.h>
#include "lvgl.h"
#include "fan_controller.h"

/*******************************************************************************
 * Class Definition
 ******************************************************************************/
/**
 * @brief Manages the LVGL-based user interface for the fan controller display
 * 
 * Handles creation, updates, and management of all UI elements including:
 * - Temperature display with color-coded arc
 * - Fan speed indicators (current and target)
 * - Operation mode display
 * - System status indicators (WiFi, MQTT, Night mode)
 */
class DisplayUI {
public:
    /**
     * @brief Constructor - initializes all UI elements to nullptr
     */
    DisplayUI();

    /**
     * @brief Initializes the UI if not already initialized
     */
    void begin();

    /**
     * @brief Updates all UI elements with current system state
     * 
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
     * @brief Returns pointer to the temperature arc widget
     * @return Pointer to the LVGL arc object
     */
    lv_obj_t* getArc();

private:
    // UI Creation Methods
    void createUI();
    void createMainScreen();
    void createLeftContainer();
    void createTemperatureArc();
    void createStatusIndicators();
    void createRightContainer();

    // Helper Methods for UI Creation
    lv_obj_t* createStatusLabel(lv_obj_t* parent, lv_align_t align, lv_coord_t x_ofs, 
                               lv_coord_t y_ofs, const char* text);
    lv_obj_t* createInfoLabel(lv_obj_t* parent, lv_align_t align, lv_coord_t x_ofs, 
                             lv_coord_t y_ofs, const char* text);

    // Update Helper Methods
    void updateTemperatureDisplay(float temp);
    void updateStatusIndicators(bool wifiConnected, bool mqttConnected, bool nightMode);
    void updateSpeedDisplay(int fanSpeed, int targetSpeed);
    void updateModeDisplay(FanController::Mode mode, bool nightMode);

    // Main UI Elements
    lv_obj_t* screen;            ///< Main screen container
    lv_obj_t* arc;              ///< Temperature display arc
    lv_obj_t* left_container;    ///< Left side container
    
    // Display Labels
    lv_obj_t* tempLabel;         ///< Temperature value display
    lv_obj_t* currentSpeedLabel; ///< Current fan speed display
    lv_obj_t* targetSpeedLabel;  ///< Target fan speed display
    lv_obj_t* modeLabel;         ///< Operation mode display

    // Status Indicators
    lv_obj_t* wifiLabel;         ///< WiFi connection status
    lv_obj_t* mqttLabel;         ///< MQTT connection status
    lv_obj_t* nightLabel;        ///< Night mode status

    bool initialized;            ///< Tracks whether UI has been initialized
};

#endif // DISPLAY_UI_H