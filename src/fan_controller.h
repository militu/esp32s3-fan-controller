#pragma once

#include <Arduino.h>
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "config.h"
#include "task_manager.h"
#include "mutex_guard.h"

// Forward declarations
class TempSensor;

static constexpr EventBits_t FAN_EVENT_TEMP_UPDATED = (1 << 0);
static constexpr EventBits_t FAN_EVENT_NIGHT_MODE_CHANGED = (1 << 1);
static constexpr EventBits_t FAN_EVENT_CONTROL_MODE_CHANGED = (1 << 2);


/**
 * @brief Controls a PWM-driven fan with RPM monitoring and various operating modes
 * 
 * Features:
 * - PWM-based speed control
 * - RPM monitoring via tachometer
 * - Automatic temperature-based control
 * - Manual control mode
 * - Night mode with customizable quiet hours
 * - Stall detection and recovery
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
        float minTemp;           ///< Minimum temperature for auto mode
        float maxTemp;           ///< Maximum temperature for auto mode
        uint8_t minPWM;         ///< Minimum PWM duty cycle
        uint8_t maxPWM;         ///< Maximum PWM duty cycle
        uint8_t nightMaxPWM;    ///< Maximum PWM during night mode
        uint16_t minRPM;        ///< Minimum RPM before stall detection
        uint8_t nightStartHour; ///< Night mode start hour (0-23)
        uint8_t nightEndHour;   ///< Night mode end hour (0-23)
        bool testMode;          ///< Enable test mode simulation
    };

    // Constructor and destructor
    explicit FanController(TaskManager& taskManager);
    ~FanController();

    // Prevent copying
    FanController(const FanController&) = delete;
    FanController& operator=(const FanController&) = delete;

    // Initialization
    esp_err_t begin();

    // Control methods
    bool setPWMDutyCycle(int percentPWM);          ///< Set PWM duty cycle (0-100%)
    bool setControlMode(Mode mode);                ///< Change operating mode
    bool setTemperature(float temperature);        ///< Update temperature for auto mode
    bool attemptRecovery();                       ///< Try to recover from stall condition

    // Night mode configuration
    bool setNightMode(bool enabled);
    bool setNightSettings(uint8_t startHour, uint8_t endHour, uint8_t maxPWM);
    bool isNightModeActive() const;
    uint8_t getNightStartHour() const;
    uint8_t getNightEndHour() const;
    uint8_t getNightMaxPWM() const;

    // Status getters
    int getCurrentPWM() const;          ///< Get current PWM (0-100%)
    int getTargetPWM() const;           ///< Get target PWM (0-100%)
    uint16_t getMeasuredRPM() const;    ///< Get current fan RPM
    Status getStatus() const;           ///< Get current fan status
    Mode getControlMode() const;        ///< Get current operating mode
    String getStatusString() const;     ///< Get human-readable status
    const Config& getConfig() const { return config; }

    // Utility functions for PWM conversion
    static uint8_t convertPercentToPWM(int percent, uint8_t minPWM, uint8_t maxPWM) {
        return map(constrain(percent, 0, 100), 0, 100, minPWM, maxPWM);
    }

    static int convertPWMToPercent(uint8_t pwm, uint8_t minPWM, uint8_t maxPWM) {
        return map(pwm, minPWM, maxPWM, 0, 100);
    }

    static constexpr EventBits_t TEMP_UPDATED = FAN_EVENT_TEMP_UPDATED;
    static constexpr EventBits_t NIGHT_MODE_CHANGED = FAN_EVENT_NIGHT_MODE_CHANGED;
    static constexpr EventBits_t CONTROL_MODE_CHANGED = FAN_EVENT_CONTROL_MODE_CHANGED;

    void registerTempSensor(TempSensor* sensor);
    EventGroupHandle_t getEventGroup() const { return events; }


private:
    // Constants
    static constexpr uint32_t TASK_STACK_SIZE = 4096;
    static constexpr UBaseType_t TASK_PRIORITY = 2;
    static constexpr BaseType_t TASK_CORE = 1;
    static constexpr uint32_t MUTEX_TIMEOUT_MS = 1000;
    static constexpr uint8_t STALL_RETRY_COUNT = 3;

    // Core components
    TaskManager& taskManager;
    SemaphoreHandle_t mutex;
    volatile static uint32_t pulseCount;
    Config config;

    // State variables
    Mode mode;
    Status status;
    uint8_t targetPWM;        ///< Raw PWM value (0-255)
    uint8_t currentPWM;       ///< Raw PWM value (0-255)
    uint16_t measuredRPM;
    uint8_t stallCount;
    bool nightModeEnabled;
    bool initialized;

    // Task management
    static void fanTask(void* parameters);
    void processUpdate();

    // Hardware control
    bool setupPWM();
    bool setupTachometer();
    static void IRAM_ATTR handleTachInterrupt();

    // Internal helper methods
    void updateRPM();
    void updatePWM();
    uint8_t calculatePWMForTemperature(float temp);
    bool isStalled() const;
    void applyNightMode(uint8_t& pwm) const;
    bool isNightTime() const;
    uint8_t percentToRawPWM(int percent) const;
    int rawPWMToPercent(uint8_t raw) const;

    EventGroupHandle_t events;
    TempSensor* tempSensor;
    void processEvents();  // New method to handle events
    void setTemperatureInternal(float temperature);

};