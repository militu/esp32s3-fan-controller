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

/**
 * @brief Temperature sensor management using Dallas Temperature sensors
 * 
 * Provides thread-safe temperature sensing with:
 * - Asynchronous temperature reading
 * - Temperature smoothing using rolling average
 * - Error detection and recovery
 * - Status monitoring and reporting
 */
class TempSensor {
public:
    /**
     * @brief Construct a new Temperature Sensor object
     * @param taskManager Reference to the task manager for FreeRTOS task handling
     */
    explicit TempSensor(TaskManager& taskManager);
    ~TempSensor();

    // Prevent copying
    TempSensor(const TempSensor&) = delete;
    TempSensor& operator=(const TempSensor&) = delete;

    /**
     * @brief Initialize the temperature sensor
     * @return ESP_OK on success, error code otherwise
     */
    esp_err_t begin();

    // Temperature reading methods - all thread-safe
    float getCurrentTemp() const;     
    float getSmoothedTemp() const;    
    bool isLastReadSuccess() const;    
    String getStatusString() const;    

    // Task and process handling
    static void tempTask(void* parameters);
    void processReading();
    void registerFanController(FanController* controller);

private:
    // Constants - will be moved to Config namespace
    static constexpr uint32_t TEMP_STACK_SIZE = 4096;
    static constexpr UBaseType_t TEMP_TASK_PRIORITY = 3;
    static constexpr BaseType_t TEMP_TASK_CORE = 1;

    // Hardware and system components
    TaskManager& taskManager;
    OneWire oneWire;
    DallasTemperature sensors;
    SemaphoreHandle_t mutex;
    FanController* fanController;

    // Temperature data
    float currentTemp;         
    float smoothedTemp;        
    float tempHistory[Config::Temperature::SMOOTH_SAMPLES];
    size_t historyIndex;      

    // Status tracking
    bool lastReadSuccess;      
    uint32_t lastReadTime;     
    uint8_t consecutiveFailures;
    bool initialized;         
    bool conversionRequested; 
    uint32_t conversionRequestTime;

    // Helper methods
    void updateSmoothing(float newTemp);
    bool startConversion();
    bool readTemperature();
};

#endif // TEMP_SENSOR_H