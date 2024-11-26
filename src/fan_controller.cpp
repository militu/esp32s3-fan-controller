/**
 * @file fan_controller.cpp
 * @brief Implementation of the FanController class for PWM-based fan control
 */

#define DEBUG_LOG(msg, ...) if (DEBUG_FAN) { Serial.printf(msg "\n", ##__VA_ARGS__); }

#include "fan_controller.h"
#include "temp_sensor.h"

// Static member initialization
volatile uint32_t FanController::pulseCount = 0;

//-----------------------------------------------------------------------------
// Constructor & Destructor
//-----------------------------------------------------------------------------

FanController::FanController(TaskManager& tm)
    : taskManager(tm)
    , mutex(nullptr)
    , events(nullptr)
    , tempSensor(nullptr)
    , ntpManager(nullptr)
    , config{
        .minTemp = DEFAULT_MIN_TEMP,
        .maxTemp = DEFAULT_MAX_TEMP,
        .minSpeed = FAN_MIN_SPEED,
        .maxSpeed = FAN_MAX_SPEED,
        .minPWM = FAN_MIN_PWM,
        .maxPWM = FAN_MAX_PWM,
        .nightMaxSpeed = NIGHT_MODE_MAX_SPEED,
        .minRPM = RPM_MINIMUM,
        .nightStartHour = NIGHT_MODE_START,
        .nightEndHour = NIGHT_MODE_END,
        .testMode = true     // Enable test mode for Wokwi
    }
    , mode(Mode::AUTO)
    , status(Status::OK)
    , currentSpeed(0)
    , measuredRPM(0)
    , stallCount(0)
    , nightModeEnabled(false)
    , initialized(false) {
    
    mutex = xSemaphoreCreateMutex();
    events = xEventGroupCreate();
    
    if (!mutex || !events) {
        DEBUG_LOG("FanController - Resource creation failed!");
    }
}

FanController::~FanController() {
    if (mutex) vSemaphoreDelete(mutex);
    if (events) vEventGroupDelete(events);
}

//-----------------------------------------------------------------------------
// Initialization
//-----------------------------------------------------------------------------

esp_err_t FanController::begin() {
    if (!mutex || !events) return ESP_ERR_NO_MEM;

    MutexGuard guard(mutex);
    if (!guard.isLocked()) return ESP_ERR_TIMEOUT;

    if (!setupPWM() || !setupTachometer()) {
        return ESP_FAIL;
    }

    // Start with minimum speed
    target.requestedSpeed = config.minSpeed;
    target.effectiveSpeed = config.minSpeed;
    currentSpeed = config.minSpeed;
    ledcWrite(PWM_CHANNEL, SpeedToRawPWM(currentSpeed));

    TaskManager::TaskConfig taskConfig("Fan", TASK_STACK_SIZE, TASK_PRIORITY, TASK_CORE);
    esp_err_t err = taskManager.createTask(taskConfig, fanTask, this);
    
    if (err != ESP_OK) return err;

    initialized = true;
    
    if (events) {
        xEventGroupSetBits(events, TEMP_UPDATED);
    }
    
    return ESP_OK;
}

//-----------------------------------------------------------------------------
// Speed Control Methods
//-----------------------------------------------------------------------------

void FanController::updateTargetSpeed(int requestedSpeed) {
    target.requestedSpeed = requestedSpeed;
    
    if (nightModeEnabled && isNightTime()) {
        // Use raw PWM values for comparison to avoid conversion rounding
        target.effectiveSpeed = min(requestedSpeed, config.nightMaxSpeed);
    } else {
        target.effectiveSpeed = target.requestedSpeed;
    }
    
    if (status == Status::OK && currentSpeed != target.effectiveSpeed) {
        currentSpeed = target.effectiveSpeed;
        ledcWrite(PWM_CHANNEL, SpeedToRawPWM(currentSpeed));
    }
}

bool FanController::setSpeedDutyCycle(int percentSpeed) {
    if (!initialized || mode != Mode::MANUAL) return false;

    MutexGuard guard(mutex);
    if (!guard.isLocked()) return false;

    updateTargetSpeed(percentSpeed);
    return true;
}

void FanController::setTemperatureInternal(float temperature) {
    int speed = calculateSpeedForTemperature(temperature);
    DEBUG_LOG("Temperature %.2f -> speed %d", temperature, speed);
    updateTargetSpeed(speed);
}

bool FanController::setTemperature(float temperature) {
    if (!initialized || mode != Mode::AUTO) {
        DEBUG_LOG("setTemperature rejected - initialized: %d, mode: %d", 
                 initialized, (int)mode);
        return false;
    }

    MutexGuard guard(mutex);
    if (!guard.isLocked()) {
        DEBUG_LOG("setTemperature failed to acquire mutex");
        return false;
    }

    setTemperatureInternal(temperature);
    return true;
}

bool FanController::setControlMode(Mode newMode) {
    MutexGuard guard(mutex);
    if (!guard.isLocked()) return false;
    
    // Reject invalid mode transitions
    if (newMode == Mode::ERROR) return false;
    
    // Update mode and trigger event
    mode = newMode;
    
    // Re-evaluate speed when changing modes since
    // AUTO and MANUAL modes have different speed determination logic
    if (mode == Mode::AUTO && tempSensor && tempSensor->isLastReadSuccess()) {
        setTemperatureInternal(tempSensor->getSmoothedTemp());
    }
    
    if (events) {
        xEventGroupSetBits(events, CONTROL_MODE_CHANGED);
    }
    
    DEBUG_LOG("Control mode changed to: %s", 
              mode == Mode::AUTO ? "Auto" : "Manual");
    return true;
}

//-----------------------------------------------------------------------------
// Night Mode Implementation
//-----------------------------------------------------------------------------

bool FanController::setNightMode(bool enabled) {
    MutexGuard guard(mutex);
    if (!guard.isLocked()) return false;

    DEBUG_LOG("Night mode %s", enabled ? "enabled" : "disabled");
    nightModeEnabled = enabled;
    
    // Re-evaluate speed with new night mode state
    updateTargetSpeed(target.requestedSpeed);
    
    if (events) {
        xEventGroupSetBits(events, NIGHT_MODE_CHANGED);
    }
    return true;
}

bool FanController::validateNightSettings(uint8_t startHour, uint8_t endHour, int maxPercent) const {
    // Validate hours
    if (startHour > 23 || endHour > 23) return false;
    
    // Validate percentage range
    if (maxPercent < 0 || maxPercent > 100) return false;
    
    return true;
}

bool FanController::setNightSettings(uint8_t startHour, uint8_t endHour, int maxPercent) {
    MutexGuard guard(mutex);
    if (!guard.isLocked()) return false;

    // Validate input parameters
    if (!validateNightSettings(startHour, endHour, maxPercent)) return false;

    // Store previous settings to detect changes
    uint8_t prevStart = config.nightStartHour;
    uint8_t prevEnd = config.nightEndHour;
    uint8_t prevMaxSpeed = config.nightMaxSpeed;

    // Update configuration
    config.nightStartHour = startHour;
    config.nightEndHour = endHour;
    config.nightMaxSpeed = maxPercent;
    
    DEBUG_LOG("Night settings updated - Start: %d, End: %d, MaxSpeed: %d%%", 
            startHour, endHour, maxPercent);

    // Only trigger update if settings actually changed
    if (prevStart != config.nightStartHour || 
        prevEnd != config.nightEndHour || 
        prevMaxSpeed != config.nightMaxSpeed) {
        
        // Always recalculate from original requested speed
        updateTargetSpeed(target.requestedSpeed);
    }

    return true;
}

bool FanController::isNightTime() const {
    // First check if we have valid NTP time
    if (ntpManager && ntpManager->isTimeSynchronized()) {
        int currentHour = ntpManager->getCurrentHour();
        if (currentHour < 0) {
            return isNightTimeRTC();
        }

        if (config.nightStartHour < config.nightEndHour) {
            return currentHour >= config.nightStartHour && 
                   currentHour < config.nightEndHour;
        } else {
            return currentHour >= config.nightStartHour || 
                   currentHour < config.nightEndHour;
}
    }
    
    // Fallback to RTC if NTP is not available
    return isNightTimeRTC();
}

bool FanController::isNightTimeRTC() const {
    time_t now;
    time(&now);
    struct tm *timeinfo = localtime(&now);
    int currentHour = timeinfo->tm_hour;
    
    bool isNight;
    if (config.nightStartHour < config.nightEndHour) {
        // Simple case: night period within same day
        isNight = currentHour >= config.nightStartHour && 
                  currentHour < config.nightEndHour;
    } else {
        // Complex case: night period crosses midnight
        isNight = currentHour >= config.nightStartHour || 
                  currentHour < config.nightEndHour;
    }
    
    DEBUG_LOG("Time check - Hour: %d, Start: %d, End: %d, Is night: %d", 
              currentHour, config.nightStartHour, config.nightEndHour, isNight);
    
    return isNight;
}

bool FanController::isNightModeActive() const {
    MutexGuard guard(mutex);
    if (!guard.isLocked()) return false;
    
    // Night mode is active only if enabled AND currently within night hours
    return nightModeEnabled && isNightTime();
}

uint8_t FanController::getNightStartHour() const {
    MutexGuard guard(mutex);
    if (!guard.isLocked()) return 0;
    return config.nightStartHour;
}

uint8_t FanController::getNightEndHour() const {
    MutexGuard guard(mutex);
    if (!guard.isLocked()) return 0;
    return config.nightEndHour;
}

uint8_t FanController::getNightMaxSpeed() const {
    MutexGuard guard(mutex);
    if (!guard.isLocked()) return 0;
    return config.nightMaxSpeed; 
}

//-----------------------------------------------------------------------------
// Core Operation Methods
//-----------------------------------------------------------------------------

void FanController::processUpdate() {
    if (!initialized) return;

    MutexGuard guard(mutex);
    if (!guard.isLocked()) return;

    updateRPM();

    // Handle stall detection
    if (currentSpeed > config.minSpeed && measuredRPM < config.minRPM) {
        stallCount++;
        if (stallCount >= STALL_RETRY_COUNT) {
            status = Status::SHUTOFF;
            currentSpeed = 0;
            ledcWrite(PWM_CHANNEL, 0);
            return;
        }
    } else {
        stallCount = 0;
        status = Status::OK;
    }

    // Re-evaluate target speed based on current conditions
    updateTargetSpeed(target.requestedSpeed);
}

void FanController::processEvents() {
    if (!initialized || !events || !tempSensor) return;

    EventBits_t bits = xEventGroupWaitBits(
        events,
        TEMP_UPDATED | NIGHT_MODE_CHANGED | CONTROL_MODE_CHANGED,
        pdTRUE,  // Clear bits after reading
        pdFALSE, // Don't wait for all bits
        0        // Don't block
    );

    if (bits == 0) return;

    MutexGuard guard(mutex);
    if (!guard.isLocked()) return;

    DEBUG_LOG("Processing fan events: 0x%lx", (unsigned long)bits);

    // Handle temperature updates in auto mode
    if (mode == Mode::AUTO && tempSensor->isLastReadSuccess()) {
        float temp = tempSensor->getSmoothedTemp();
        DEBUG_LOG("Updating temperature to %.2f°C", temp);
        setTemperatureInternal(temp);
    }
}

void FanController::fanTask(void* parameters) {
    FanController* fan = static_cast<FanController*>(parameters);
    TickType_t lastRPMUpdate = xTaskGetTickCount();
    TickType_t lastEventCheck = xTaskGetTickCount();
    
    while (true) {
        fan->taskManager.updateTaskRunTime("Fan");
        TickType_t now = xTaskGetTickCount();
        
        // Process events more frequently than RPM updates
        if ((now - lastEventCheck) >= pdMS_TO_TICKS(EVENT_CHECK_INTERVAL)) {
            fan->processEvents();
            lastEventCheck = now;
        }
        
        // Update RPM and process fan control
        if ((now - lastRPMUpdate) >= pdMS_TO_TICKS(RPM_UPDATE_INTERVAL)) {
            fan->processUpdate();
            lastRPMUpdate = now;
        }
        
        vTaskDelay(pdMS_TO_TICKS(500));
    }
}

//-----------------------------------------------------------------------------
// Hardware Control Methods
//-----------------------------------------------------------------------------

bool FanController::setupPWM() {
    ledcSetup(PWM_CHANNEL, PWM_FREQUENCY, PWM_RESOLUTION);
    ledcAttachPin(FAN_PWM_PIN, PWM_CHANNEL);
    return true;
}

bool FanController::setupTachometer() {
    pinMode(FAN_TACH_PIN, INPUT_PULLUP);
    attachInterrupt(digitalPinToInterrupt(FAN_TACH_PIN), handleTachInterrupt, FALLING);
    return true;
}

void IRAM_ATTR FanController::handleTachInterrupt() {
    pulseCount++;
}

void FanController::updateRPM() {
    if (config.testMode) {
        // Simulate RPM in test mode
        measuredRPM = map(currentSpeed, 
                         config.minSpeed, config.maxSpeed, 
                         500, 2000);  // Simulate range 500-2000 RPM
        DEBUG_LOG("Test Mode - Simulated RPM: %d for speed: %d", 
                 measuredRPM, currentSpeed);
        return;
    }

    // Calculate actual RPM from pulse count
    uint32_t pulses = pulseCount;
    pulseCount = 0;
    measuredRPM = (pulses * 60) / (RPM_UPDATE_INTERVAL / 1000.0) / FAN_PULSES_PER_REV;
}

//-----------------------------------------------------------------------------
// Status & Recovery Methods
//-----------------------------------------------------------------------------

bool FanController::attemptRecovery() {
    MutexGuard guard(mutex);
    if (!guard.isLocked()) return false;

    if (status != Status::SHUTOFF) return false;

    // Reset stall detection and restart at minimum speed
    stallCount = 0;
    status = Status::OK;
    updateTargetSpeed(config.minSpeed);
    
    return true;
}

bool FanController::isStalled() const {
    MutexGuard guard(mutex);
    if (!guard.isLocked()) return true;  // Assume stalled if we can't check
    return status == Status::SHUTOFF;
}

//-----------------------------------------------------------------------------
// Utility Methods
//-----------------------------------------------------------------------------

int FanController::calculateSpeedForTemperature(float temp) const {
    temp = constrain(temp, config.minTemp, config.maxTemp);
    float ratio = (temp - config.minTemp) / (config.maxTemp - config.minTemp);
    return config.minSpeed + (ratio * (config.maxSpeed - config.minSpeed));
}

uint8_t FanController::SpeedToRawPWM(int percent) const {
    return map(constrain(percent, 0, 100), 0, 100, config.minPWM, config.maxPWM);
}

int FanController::rawPWMToSpeed(uint8_t raw) const {
    return map(raw, config.minPWM, config.maxPWM, 0, 100);
}

//-----------------------------------------------------------------------------
// Protected Getters
//-----------------------------------------------------------------------------

int FanController::getCurrentSpeed() const {
    MutexGuard guard(mutex);
    if (!guard.isLocked()) return 0;
    return currentSpeed;
}

int FanController::getTargetSpeed() const {
    MutexGuard guard(mutex);
    if (!guard.isLocked()) return 0;
    return target.effectiveSpeed;
}

uint16_t FanController::getMeasuredRPM() const {
    MutexGuard guard(mutex);
    if (!guard.isLocked()) return 0;
    return measuredRPM;
}

FanController::Status FanController::getStatus() const {
    MutexGuard guard(mutex);
    if (!guard.isLocked()) return Status::ERROR;
    return status;
}

FanController::Mode FanController::getControlMode() const {
    MutexGuard guard(mutex);
    if (!guard.isLocked()) return Mode::ERROR;
    return mode;
}

String FanController::getStatusString() const {
    MutexGuard guard(mutex);
    if (!guard.isLocked()) return "Mutex Error";
    
    String result = (mode == Mode::AUTO) ? "Auto" : "Manual";
    result += " - Speed: " + String(currentSpeed) + "%";  // Change PWM to Speed
    result += " - RPM: " + String(measuredRPM);
    
    switch (status) {
        case Status::SHUTOFF: result += " (Shutoff)"; break;
        case Status::ERROR:   result += " (Error)"; break;
        default: break;
    }
    
    return result;
}

//-----------------------------------------------------------------------------
// Component Registration
//-----------------------------------------------------------------------------

void FanController::registerTempSensor(TempSensor* sensor) {
    MutexGuard guard(mutex);
    if (guard.isLocked()) {
        tempSensor = sensor;
    }
}

void FanController::registerNTPManager(NTPManager* manager) {
    MutexGuard guard(mutex);
    if (guard.isLocked()) {
        ntpManager = manager;
    }
}