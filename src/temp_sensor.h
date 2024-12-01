/**
 * @file temp_sensor.h
 * @brief Temperature sensor management class using Dallas Temperature sensors
 * 
 * Provides thread-safe temperature sensing capabilities with:
 * - Async temperature reading
 * - Temperature smoothing
 * - Error detection and recovery
 * - Status monitoring
 */

#ifndef TEMP_SENSOR_H
#define TEMP_SENSOR_H

#include <Arduino.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "config.h"
#include "task_manager.h"
#include "mutex_guard.h"
#include "debug_log.h"

// Forward declarations
class FanController;

class TempSensor {
public:
    /**
     * @brief Construct a new Temperature Sensor object
     * @param taskManager Reference to the task manager for FreeRTOS task handling
     */
    explicit TempSensor(TaskManager& taskManager);
    ~TempSensor();

    // Prevent copying to avoid mutex management issues
    TempSensor(const TempSensor&) = delete;
    TempSensor& operator=(const TempSensor&) = delete;

    /**
     * @brief Initialize the temperature sensor
     * @return ESP_OK on success, error code otherwise
     */
    esp_err_t begin();

    // Temperature reading methods - all thread-safe
    float getCurrentTemp() const;      ///< Get the latest raw temperature reading
    float getSmoothedTemp() const;     ///< Get the smoothed temperature value
    bool isLastReadSuccess() const;    ///< Check if the last reading was successful
    String getStatusString() const;    ///< Get the current sensor status as a string

    // Task handling methods
    static void tempTask(void* parameters);  ///< FreeRTOS task function
    void processReading();                   ///< Process a temperature reading
    void registerFanController(FanController* controller);

private:
    // Hardware and system components
    TaskManager& taskManager;
    OneWire oneWire;
    DallasTemperature sensors;
    SemaphoreHandle_t mutex;
    FanController* fanController;

    // Temperature data
    float currentTemp;         ///< Current raw temperature reading
    float smoothedTemp;        ///< Smoothed temperature value
    float tempHistory[TEMP_SMOOTH_SAMPLES];  ///< History buffer for smoothing
    size_t historyIndex;      ///< Current position in history buffer

    // Status tracking
    bool lastReadSuccess;      ///< Flag indicating last read success
    uint32_t lastReadTime;     ///< Timestamp of last reading
    uint8_t consecutiveFailures;  ///< Count of consecutive failed readings
    bool initialized;          ///< Sensor initialization status
    bool conversionRequested;  ///< Flag for pending temperature conversion
    uint32_t conversionRequestTime;  ///< Timestamp of last conversion request

    // Helper methods
    void updateSmoothing(float newTemp);  ///< Update smoothing buffer
    bool startConversion();               ///< Start temperature conversion
    bool readTemperature();               ///< Read temperature from sensor

    // Task configuration constants
    static constexpr uint32_t TEMP_STACK_SIZE = 4096;
    static constexpr UBaseType_t TEMP_TASK_PRIORITY = 3;
    static constexpr BaseType_t TEMP_TASK_CORE = 1;
};

#endif // TEMP_SENSOR_H