#ifndef BOOT_SCREEN_H
#define BOOT_SCREEN_H

#include <Arduino.h>
#include "lvgl.h"
#include "mutex_guard.h"

/**
 * @brief Manages the boot screen UI displayed during system initialization
 * 
 * Features:
 * - Visual initialization status display
 * - Animated component status updates
 * - Thread-safe UI updates
 * - Component-specific status sections
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

    // Construction / Destruction
    BootScreen();
    ~BootScreen();

    /**
     * @brief Initialize display dimensions
     */
    void init(uint16_t width, uint16_t height) {
        displayWidth = width;
        displayHeight = height;
    }

    // Core functionality
    void begin();
    void updateStatus(const char* component, ComponentStatus status);
    void updateStatusWithDetail(const char* component, ComponentStatus status, const char* detail);

    // Screen access
    lv_obj_t* getScreen() { return screen; }

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
    
    bool initialized;              ///< Tracks UI initialization

    // UI Creation Methods
    void createUI();
    void createMainScreen();
    void createStatusSection(const char* title, uint16_t yOffset, 
                           lv_obj_t** statusLabel, lv_obj_t** detailLabel);
    const lv_font_t* selectDynamicFont(uint16_t width);
    const lv_font_t* selectDetailFont(uint16_t width);

    // Helper Methods
    lv_color_t getStatusColor(ComponentStatus status);
    const char* getStatusText(ComponentStatus status);
    void animateContainer(lv_obj_t* container, ComponentStatus status);

    // Synchronization
    SemaphoreHandle_t uiMutex;
};

#endif // BOOT_SCREEN_H