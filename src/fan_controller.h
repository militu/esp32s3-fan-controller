#pragma once

#include <Arduino.h>
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "config.h"
#include "task_manager.h"
#include "mutex_guard.h"
#include "ntp_manager.h"
#include "debug_log.h"
#include "config_preference.h"

// Forward declarations
class TempSensor;

/**
 * @brief Controls a PWM-driven fan with RPM monitoring and various operating modes
 * 
 * Features:
 * - PWM-based speed control with automatic temperature-based and manual modes
 * - RPM monitoring via tachometer input
 * - Night mode with configurable quiet hours and speed limits
 * - Stall detection and recovery
 * - Event-driven updates for temperature and mode changes
 */
class FanController {
public:
    /**
     * @brief Operating modes for the fan controller
     */
    enum class Mode {
        AUTO,       ///< Temperature-based automatic control
        MANUAL,     ///< Manual PWM control
        ERROR       ///< Error state
    };

    /**
     * @brief Current status of the fan
     */
    enum class Status {
        OK,         ///< Normal operation
        SHUTOFF,    ///< Fan stopped due to stall
        ERROR       ///< General error condition
    };

    /**
     * @brief Configuration parameters for fan operation
     */
    struct FanConfig {
        float minTriggerTemp;          ///< Minimum trigger temperature for auto mode
        float maxTriggerTemp;          ///< Maximum trigger temperature for auto mode
        uint8_t minSpeed;       ///< Minimum speed percentage
        uint8_t maxSpeed;       ///< Maximum speed percentage
        uint8_t minPWM;        ///< Minimum PWM value
        uint8_t maxPWM;        ///< Maximum PWM value
        uint8_t nightMaxSpeed; ///< Maximum speed during night mode
        uint16_t minRPM;      ///< Minimum RPM before stall detection
        uint8_t nightStartHour; ///< Night mode start hour (0-23)
        uint8_t nightEndHour;   ///< Night mode end hour (0-23)
        bool testMode;          ///< Enable test mode simulation
    };

    /**
     * @brief Encapsulates target speed states and calculations
     */
    struct SpeedTarget {
        uint8_t requestedSpeed;   ///< Originally requested speed value
        uint8_t effectiveSpeed;   ///< Actual speed after applying limits

        SpeedTarget() : requestedSpeed(0), effectiveSpeed(0) {}
    };

    // Event flags for FreeRTOS event group
    static constexpr EventBits_t TEMP_UPDATED = (1 << 0);
    static constexpr EventBits_t NIGHT_MODE_CHANGED = (1 << 1);
    static constexpr EventBits_t CONTROL_MODE_CHANGED = (1 << 2);

    // Constructor and destructor
    explicit FanController(TaskManager& taskManager, ConfigPreference& config);
    ~FanController();

    // Prevent copying
    FanController(const FanController&) = delete;
    FanController& operator=(const FanController&) = delete;

    // Initialization
    esp_err_t begin();

    // Core control methods
    bool setSpeedDutyCycle(uint8_t percentSpeed);
    bool setControlMode(Mode mode);
    bool setTemperature(float temperature);
    bool attemptRecovery();

    // Night mode configuration
    bool setNightMode(bool enabled);
    bool setNightSettings(uint8_t startHour, uint8_t endHour, uint8_t maxPercent);
    bool isNightModeEnabled() const;
    uint8_t getNightStartHour() const;
    uint8_t getNightEndHour() const;
    uint8_t getNightMaxSpeed() const;
    bool isNightModeActive() const;

    // Status getters
    uint8_t getCurrentSpeed() const;        ///< Get current speed percentage (0-100)
    uint8_t getTargetSpeed() const;         ///< Get target speed percentage (0-100)
    uint16_t getMeasuredRPM() const;        ///< Get current fan RPM
    Status getStatus() const;               ///< Get current fan status
    Mode getControlMode() const;            ///< Get current operating mode
    String getStatusString() const;         ///< Get human-readable status
    const FanConfig& getConfig() const { return config; }

    // Component registration
    void registerTempSensor(TempSensor* sensor);
    void registerNTPManager(NTPManager* manager);
    EventGroupHandle_t getEventGroup() const { return events; }

    // Settings management
    void saveSettings(ConfigPreference& configPref);
    void loadSettings(ConfigPreference& configPref);

private:
    // Core components
    TaskManager& taskManager;
    SemaphoreHandle_t mutex;
    EventGroupHandle_t events;
    TempSensor* tempSensor;
    NTPManager* ntpManager;
    FanConfig config;
    ConfigPreference& configPreference;
    
    // State tracking
    Mode mode;
    Status status;
    SpeedTarget target;
    uint8_t currentSpeed;
    uint16_t measuredRPM;
    uint8_t stallCount;
    bool nightModeEnabled;
    bool initialized;
    volatile static uint32_t pulseCount;

    // Task management
    static void fanTask(void* parameters);
    void processUpdate();
    void processEvents();

    // Hardware control
    bool setupPWM();
    bool setupTachometer();
    static void IRAM_ATTR handleTachInterrupt();
    void updateRPM();
    
    // Speed control helpers
    void updateTargetSpeed(uint8_t requestedSpeed);
    uint8_t calculateSpeedForTemperature(float temp) const;
    void setTemperatureInternal(float temperature);
    bool validateNightSettings(uint8_t startHour, uint8_t endHour, uint8_t maxPercent) const;
    
    // Status helpers
    bool isStalled() const;
    bool isNightTime() const;
    bool isNightTimeRTC() const;

    // Utility methods
    uint8_t SpeedToRawPWM(uint8_t percent) const;
    uint8_t rawPWMToSpeed(uint8_t raw) const;
};