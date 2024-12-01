#ifndef BOOT_SCREEN_H
#define BOOT_SCREEN_H

#include <Arduino.h>
#include "lvgl.h"
#include "mutex_guard.h"

/**
 * @brief Manages the boot screen UI displayed during system initialization
 */
class BootScreen {
public:
    /**
     * @brief Status of each major system component
     */
    enum class ComponentStatus {
        PENDING,    ///< Not yet initialized
        WORKING,    ///< Currently initializing
        SUCCESS,    ///< Successfully initialized
        FAILED      ///< Failed to initialize
    };

    /**
     * @brief Constructor /Destructor - initializes UI elements
     */
    BootScreen();
    ~BootScreen();

    /**
     * @brief Initialize display dimensions
     */
    void init(uint16_t width, uint16_t height) {
        displayWidth = width;
        displayHeight = height;
    }

    /**
     * @brief Creates and shows the boot screen
     */
    void begin();

    /**
     * @brief Update component status
     */
    void updateStatus(const char* component, ComponentStatus status);

    /**
     * @brief Update component status with additional detail text
     */
    void updateStatusWithDetail(const char* component, ComponentStatus status, const char* detail);

    lv_obj_t* getScreen() {
        return screen;  // Return the main screen object
    }

private:
    // Display dimensions
    uint16_t displayWidth;
    uint16_t displayHeight;

    // UI Elements
    lv_obj_t* screen;              ///< Main screen container
    lv_obj_t* titleLabel;          ///< Title text
    lv_obj_t* wifiLabel;           ///< WiFi status
    lv_obj_t* wifiDetailLabel;     ///< WiFi detail text
    lv_obj_t* ntpLabel;            ///< NTP status
    lv_obj_t* ntpDetailLabel;      ///< NTP detail text
    lv_obj_t* mqttLabel;           ///< MQTT status
    lv_obj_t* mqttDetailLabel;     ///< MQTT detail text
    
    bool initialized;              ///< Tracks whether UI has been initialized

    // UI Creation Methods
    void createUI();
    void createMainScreen();
    lv_color_t getStatusBgColor(ComponentStatus status);
    void createStatusSection(const char* title, uint16_t yOffset, 
                               lv_obj_t** statusLabel, lv_obj_t** detailLabel);
    const lv_font_t* selectDynamicFont(uint16_t width);
    const lv_font_t* selectDetailFont(uint16_t width);

    // Helper Methods
    const char* getStatusText(ComponentStatus status);
    lv_color_t getStatusColor(ComponentStatus status);

    SemaphoreHandle_t uiMutex;
    void animateContainer(lv_obj_t* container, ComponentStatus status);
};

#endif // BOOT_SCREEN_H