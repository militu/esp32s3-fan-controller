/**
 * @file temp_sensor.cpp
 * @brief Implementation of the TempSensor class
 */

#include "temp_sensor.h"
#include "fan_controller.h"

/*******************************************************************************
 * Construction / Destruction
 ******************************************************************************/

TempSensor::TempSensor(TaskManager& tm)
    : taskManager(tm)
    , fanController(nullptr)
    , oneWire(Config::Temperature::SENSOR_PIN)
    , sensors(&oneWire)
    , mutex(xSemaphoreCreateMutex())
    , currentTemp(Config::Temperature::DEFAULT_VALUE)
    , smoothedTemp(Config::Temperature::DEFAULT_VALUE)
    , historyIndex(0)
    , lastReadSuccess(false)
    , lastReadTime(0)
    , consecutiveFailures(0)
    , initialized(false) 
{
    if (!mutex) {
        DEBUG_LOG_TEMP("TempSensor - Mutex creation failed!");
    }
    
    // Initialize temperature history buffer
    for (int i = 0; i < Config::Temperature::SMOOTH_SAMPLES; i++) {
        tempHistory[i] = Config::Temperature::DEFAULT_VALUE;
    }
}

TempSensor::~TempSensor() {
    if (mutex) {
        vSemaphoreDelete(mutex);
    }
}

/*******************************************************************************
 * Initialization
 ******************************************************************************/

esp_err_t TempSensor::begin() {
    DEBUG_LOG_TEMP("Temperature Sensor Starting...");

    if (!mutex) {
        return ESP_ERR_NO_MEM;
    }

    // Initialize the Dallas temperature sensor
    sensors.begin();
    sensors.setWaitForConversion(false);  // Enable async reading

    if (!sensors.getDeviceCount()) {
        Serial.println("No temperature sensors detected!");
        return ESP_ERR_NOT_FOUND;
    }

    // Create temperature monitoring task
    TaskManager::TaskConfig taskConfig("Temp", 
                                       Config::Temperature::Task::STACK_SIZE, 
                                       Config::Temperature::Task::TASK_PRIORITY, 
                                       Config::Temperature::Task::TASK_CORE);
    esp_err_t err = taskManager.createTask(taskConfig, tempTask, this);
    
    if (err != ESP_OK) {
        return err;
    }

    initialized = true;
    
    // Start first temperature conversion
    MutexGuard guard(mutex);
    if (!guard.isLocked()) {
        return ESP_ERR_TIMEOUT;
    }
    
    sensors.requestTemperatures();
    conversionRequested = true;
    conversionRequestTime = millis();

    DEBUG_LOG_TEMP("Temperature sensor initialized successfully");
    return ESP_OK;
}

void TempSensor::registerFanController(FanController* controller) {
    MutexGuard guard(mutex);
    if (guard.isLocked()) {
        fanController = controller;
    }
}

/*******************************************************************************
 * Temperature Reading and Processing
 ******************************************************************************/

void TempSensor::processReading() {
    if (!initialized) return;

    uint32_t currentTime = millis();

    // Start new conversion if none is in progress
    if (!conversionRequested) {
        MutexGuard guard(mutex);
        if (!guard.isLocked()) return;
        
        DEBUG_LOG_TEMP("Starting new temperature conversion");  // Add this
        sensors.requestTemperatures();
        conversionRequested = true;
        conversionRequestTime = millis();
        return;
    }

    // Wait for conversion to complete (750ms minimum)
    if (currentTime - conversionRequestTime < 750) {
        return;
    }

    MutexGuard guard(mutex);
    if (!guard.isLocked()) return;

    float tempC = sensors.getTempCByIndex(0);
    DEBUG_LOG_TEMP("Raw temperature reading: %.2f°C", tempC);  // Add this
    
    bool tempChanged = false;
    
    // Validate temperature reading
    if (tempC != DEVICE_DISCONNECTED_C && tempC != 85.0 && tempC > -55.0 && tempC < 125.0) {
        lastReadSuccess = true;
        consecutiveFailures = 0;
        tempChanged = (abs(tempC - currentTemp) > 0.1);
        currentTemp = tempC;
        updateSmoothing(tempC);  // Always update smoothing
    } else {
        consecutiveFailures++;
        lastReadSuccess = false;
        if (consecutiveFailures >= Config::Temperature::MAX_RETRIES) {
            currentTemp = Config::Temperature::DEFAULT_VALUE;
        }
    }

    lastReadTime = currentTime;
    conversionRequested = false;

    // Always notify of temperature update if successful, not just on changes
    if (lastReadSuccess && fanController) {
        EventGroupHandle_t events = fanController->getEventGroup();
        if (events) {
            DEBUG_LOG_TEMP("Notifying fan controller of temperature update");
            xEventGroupSetBits(events, FanController::TEMP_UPDATED);
        }
    }
}

bool TempSensor::startConversion() {
    MutexGuard guard(mutex);
    if (!guard.isLocked()) return false;

    sensors.requestTemperatures();
    conversionRequested = true;
    conversionRequestTime = millis();
    return true;
}

bool TempSensor::readTemperature() {
    MutexGuard guard(mutex);
    if (!guard.isLocked()) return false;

    float tempC = sensors.getTempCByIndex(0);
    bool success = false;

    if (tempC != DEVICE_DISCONNECTED_C && tempC != 85.0 && tempC > -55.0 && tempC < 125.0) {
        lastReadSuccess = true;
        consecutiveFailures = 0;
        currentTemp = tempC;
        updateSmoothing(tempC);
        success = true;
    } else {
        consecutiveFailures++;
        lastReadSuccess = false;
        
        if (consecutiveFailures >= Config::Temperature::MAX_RETRIES) {
            currentTemp = Config::Temperature::DEFAULT_VALUE;
        }
    }

    lastReadTime = millis();
    return success;
}

void TempSensor::updateSmoothing(float newTemp) {
    tempHistory[historyIndex] = newTemp;
    historyIndex = (historyIndex + 1) % Config::Temperature::SMOOTH_SAMPLES;
    
    float sum = 0;
    for (int i = 0; i < Config::Temperature::SMOOTH_SAMPLES; i++) {
        sum += tempHistory[i];
    }
    smoothedTemp = sum / Config::Temperature::SMOOTH_SAMPLES;
}

/*******************************************************************************
 * Thread-safe Getters
 ******************************************************************************/

float TempSensor::getCurrentTemp() const {
    MutexGuard guard(mutex);
    if (!guard.isLocked()) return Config::Temperature::DEFAULT_VALUE;
    return currentTemp;
}

float TempSensor::getSmoothedTemp() const {
    MutexGuard guard(mutex);
    if (!guard.isLocked()) return Config::Temperature::DEFAULT_VALUE;
    return smoothedTemp;
}

bool TempSensor::isLastReadSuccess() const {
    MutexGuard guard(mutex);
    if (!guard.isLocked()) return false;
    return lastReadSuccess;
}

String TempSensor::getStatusString() const {
    MutexGuard guard(mutex);
    if (!guard.isLocked()) return "Mutex Error";
    
    if (lastReadSuccess) {
        return "OK";
    } else if (consecutiveFailures >= Config::Temperature::MAX_RETRIES) {
        return "Failed - Using Default";
    } else {
        return "Retrying";
    }
}

/*******************************************************************************
 * Task Implementation
 ******************************************************************************/

void TempSensor::tempTask(void* parameters) {
    TempSensor* temp = static_cast<TempSensor*>(parameters);
    TickType_t lastConversionStart = 0;
    bool conversionInProgress = false;
    
    while (true) {
        temp->taskManager.updateTaskRunTime("Temp");
        uint32_t currentTime = millis();
        
        if (!conversionInProgress) {
            // Start new conversion
            if (temp->startConversion()) {
                lastConversionStart = xTaskGetTickCount();
                conversionInProgress = true;
                vTaskDelay(pdMS_TO_TICKS(800)); // Slightly longer than conversion time
                continue;
            }
        } else {
            // Check if conversion is complete
            if ((xTaskGetTickCount() - lastConversionStart) >= pdMS_TO_TICKS(750)) {
                temp->processReading();
                conversionInProgress = false;
                // Wait remainder of the interval
                vTaskDelay(pdMS_TO_TICKS(Config::Temperature::READ_INTERVAL_MS - 800));
                continue;
            }
        }
        
        vTaskDelay(pdMS_TO_TICKS(100)); // Short delay if neither condition met
    }
}