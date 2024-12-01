#ifndef DASHBAORD_SCREEN_H
#define DASHBAORD_SCREEN_H

/*******************************************************************************
 * Includes
 ******************************************************************************/
#include <Arduino.h>
#include "lvgl.h"
#include "fan_controller.h"
#include "fonts/icons.h"

#define MY_MOON_SYMBOL "\xEF\x86\x86"
#define MY_TOWER_BROADCAST "\xEF\x94\x99"


/*******************************************************************************
 * Class Definition
 ******************************************************************************/
/**
 * @brief Manages the LVGL-based user interface for the fan controller display
 */
class DashboardScreen {
public:
    /**
     * @brief Constructor - initializes all UI elements to nullptr
     */
    DashboardScreen();  // Just declaration, no implementation here
    ~DashboardScreen();

    /**
     * @brief Initializes display dimensions
     */
    void init(uint16_t width, uint16_t height) {
        displayWidth = width;
        displayHeight = height;
    }

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
     * @param nightModeEnabled Night mode status
     * @param nightModeActive Night mode status
     */
    void update(float temp, int fanSpeed, int targetSpeed, FanController::Mode mode,
                bool wifiConnected, bool mqttConnected, bool nightModeEnabled, bool nightModeActive);

    /**
     * @brief Returns pointer to the temperature arc widget
     * @return Pointer to the LVGL arc object
     */
    lv_obj_t* getArc();

    lv_obj_t* getScreen() {
        return screen;  // Return the main screen object
    }

    SemaphoreHandle_t getUIMutex() const { return uiMutex; }

private:
    // display dimensions
    uint16_t displayWidth;
    uint16_t displayHeight;

    // UI Creation Methods
    void createUI();
    void createMainScreen();
    void createLeftContainer(uint16_t width, uint16_t height, uint16_t margin);
    void createTemperatureArc(uint16_t size, uint16_t xPos);
    void createStatusIndicators();
    void createRightContainer(uint16_t width, uint16_t height, uint16_t xPos, uint16_t yPos);
    
    void createTopStatusBar(uint16_t height);
    void createDelimiter(uint16_t topOffset, uint16_t height);
    void createMainContent(uint16_t startY, uint16_t height);


    // Helper Methods for UI Creation
    lv_obj_t* createStatusLabel(lv_obj_t* parent, lv_align_t align, lv_coord_t x_ofs, 
                               lv_coord_t y_ofs, const char* text);
    lv_obj_t* createInfoLabel(lv_obj_t* parent, lv_align_t align, lv_coord_t x_ofs, 
                             lv_coord_t y_ofs, const char* text);

    // Update Helper Methods
    void updateTemperatureDisplay(float temp);
    void updateStatusIndicators(bool wifiConnected, bool mqttConnected, bool nightModeEnabled, bool nightModeActive);
    void updateSpeedDisplay(int fanSpeed, int targetSpeed);
    void updateModeDisplay(FanController::Mode mode);

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

    // animation
    lv_anim_t arcAnim;
    int currentArcValue;
    static void arcAnimCallback(void* var, int32_t value);

    bool animationInProgress;
    SemaphoreHandle_t uiMutex;

    struct StatusState {
        bool wifiConnected = false;
        bool mqttConnected = false;
        bool nightModeEnabled = false;
        bool nightModeActive = false;
    };
    StatusState lastStatus;

    // Define consistent colors as constants
    static constexpr uint32_t COLOR_SUCCESS = 0x00FF88;    // Cyan (from boot screen)
    static constexpr uint32_t COLOR_ERROR = 0xFF5555;      // Soft red (from boot screen)
    static constexpr uint32_t COLOR_WORKING = 0x00A5FF;    // Blue (from boot screen)
    static constexpr uint32_t COLOR_INACTIVE = 0x404060;   // Dark blue-gray
    static constexpr uint32_t COLOR_BORDER = 0x304060;     // Border color
    static constexpr uint32_t COLOR_BG_DARK = 0x1A1A2A;    // Dark background
    static constexpr uint32_t COLOR_BG_LIGHT = 0x202040;   // Lighter background for gradient
};

#endif // DASHBAORD_SCREEN_H