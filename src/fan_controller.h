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
    struct Config {
        float minTemp;          ///< Minimum temperature for auto mode
        float maxTemp;          ///< Maximum temperature for auto mode
        int minSpeed;         ///< Minimum Speed duty cycle
        int maxSpeed;         ///< Maximum Speed duty cycle
        u_int8_t minPWM;         ///< Minimum PWM of fan 
        u_int8_t maxPWM;         ///< Maximum PWM of fan
        int nightMaxSpeed;    ///< Maximum Speed during night mode
        uint16_t minRPM;        ///< Minimum RPM before stall detection
        uint8_t nightStartHour; ///< Night mode start hour (0-23)
        uint8_t nightEndHour;   ///< Night mode end hour (0-23)
        bool testMode;          ///< Enable test mode simulation
    };

    /**
     * @brief Encapsulates target speed states and calculations
     */
    struct SpeedTarget {
        int requestedSpeed;   ///< Originally requested Speed value
        int effectiveSpeed;   ///< Actual Speed after applying limits

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
    bool setSpeedDutyCycle(int percentSpeed);       ///< Set Speed duty cycle (0-100%)
    bool setControlMode(Mode mode);             ///< Change operating mode
    bool setTemperature(float temperature);     ///< Update temperature for auto mode
    bool attemptRecovery();                     ///< Try to recover from stall condition

    // Night mode configuration
    bool setNightMode(bool enabled);
    bool setNightSettings(uint8_t startHour, uint8_t endHour, int maxPercent);
    bool isNightModeEnabled() const;
    uint8_t getNightStartHour() const;
    uint8_t getNightEndHour() const;
    uint8_t getNightMaxSpeed() const;
    bool isNightModeActive() const;

    // Status getters
    int getCurrentSpeed() const;          ///< Get current speed (PWM) (0-100%)
    int getTargetSpeed() const;           ///< Get target speed (PWM) (0-100%)
    uint16_t getMeasuredRPM() const;    ///< Get current fan RPM
    Status getStatus() const;           ///< Get current fan status
    Mode getControlMode() const;        ///< Get current operating mode
    String getStatusString() const;     ///< Get human-readable status
    const Config& getConfig() const { return config; }

    // Component registration
    void registerTempSensor(TempSensor* sensor);
    void registerNTPManager(NTPManager* manager);
    EventGroupHandle_t getEventGroup() const { return events; }

    // Utility functions for PWM conversion
    static uint8_t convertSpeedToPWM(int percent, uint8_t minPWM, uint8_t maxPWM) {
        return map(constrain(percent, 0, 100), 0, 100, minPWM, maxPWM);
    }

    static int convertPWMToSpeed(uint8_t pwm, uint8_t minPWM, uint8_t maxPWM) {
        return map(pwm, minPWM, maxPWM, 0, 100);
    }

    // Save and load preferences
    void saveSettings(ConfigPreference& configPref);
    void loadSettings(ConfigPreference& configPref);

private:
    // Constants
    static constexpr uint32_t TASK_STACK_SIZE = 4096;
    static constexpr UBaseType_t TASK_PRIORITY = 3;
    static constexpr BaseType_t TASK_CORE = 1;

    static constexpr uint32_t MUTEX_TIMEOUT_MS = 1000;
    static constexpr uint8_t STALL_RETRY_COUNT = 3;
    static constexpr uint32_t EVENT_CHECK_INTERVAL = 1000;

    // Core components
    TaskManager& taskManager;
    SemaphoreHandle_t mutex;
    EventGroupHandle_t events;
    TempSensor* tempSensor;
    NTPManager* ntpManager;
    Config config;
    
    // State tracking
    Mode mode;
    Status status;
    SpeedTarget target;
    int currentSpeed;
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
    void updateTargetSpeed(int requestedSpeed);
    int calculateSpeedForTemperature(float temp) const;
    void setTemperatureInternal(float temperature);
    bool validateNightSettings(uint8_t startHour, uint8_t endHour, int maxPercent) const;
    
    // Status helpers
    bool isStalled() const;
    bool isNightTime() const;
    bool isNightTimeRTC() const;

    // Utility methods
    uint8_t SpeedToRawPWM(int percent) const;
    int rawPWMToSpeed(uint8_t raw) const;

    // Config Preference
    ConfigPreference& configPreference;
};