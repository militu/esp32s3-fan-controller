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
    , config{
        .minTemp = DEFAULT_MIN_TEMP,
        .maxTemp = DEFAULT_MAX_TEMP,
        .minPWM = FAN_MIN_PWM,
        .maxPWM = FAN_MAX_PWM,
        .nightMaxPWM = NIGHT_MODE_MAX_PWM,
        .minRPM = RPM_MINIMUM,
        .nightStartHour = NIGHT_MODE_START,
        .nightEndHour = NIGHT_MODE_END,
        .testMode = true     // Enable test mode for Wokwi
    }
    , mode(Mode::AUTO)
    , status(Status::OK)
    , targetPWM(0)
    , currentPWM(0)
    , measuredRPM(0)
    , stallCount(0)
    , ntpManager(nullptr)
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

    // Start with minimum speed to check if fan works
    targetPWM = config.minPWM;
    currentPWM = config.minPWM;
    ledcWrite(PWM_CHANNEL, currentPWM);

    TaskManager::TaskConfig taskConfig("Fan", TASK_STACK_SIZE, TASK_PRIORITY, TASK_CORE);
    esp_err_t err = taskManager.createTask(taskConfig, fanTask, this);
    
    if (err != ESP_OK) return err;

    initialized = true;
    
    // Set initial event to trigger first temperature reading
    if (events) {
        xEventGroupSetBits(events, TEMP_UPDATED);
    }
    
    return ESP_OK;
}

void FanController::registerTempSensor(TempSensor* sensor) {
    MutexGuard guard(mutex);
    if (guard.isLocked()) {
        tempSensor = sensor;
    }
}

//-----------------------------------------------------------------------------
// Hardware Setup & Control
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

//-----------------------------------------------------------------------------
// Process Events
//-----------------------------------------------------------------------------

void FanController::processEvents() {
    if (!initialized || !events || !tempSensor) return;

    EventBits_t bits = xEventGroupWaitBits(
        events,
        TEMP_UPDATED | NIGHT_MODE_CHANGED | CONTROL_MODE_CHANGED,
        pdTRUE,
        pdFALSE,
        0
    );

    if (bits == 0) return;

    // Move mutex guard to cover both the event check and temperature setting
    MutexGuard guard(mutex);
    if (!guard.isLocked()) return;

    DEBUG_LOG("Processing fan events: 0x%lx", (unsigned long)bits);

    if (mode == Mode::AUTO && tempSensor->isLastReadSuccess()) {
        float temp = tempSensor->getSmoothedTemp();
        DEBUG_LOG("About to update temperature to %.2f°C", temp);
        // Call internal version without mutex guard
        setTemperatureInternal(temp);
    }
}

//-----------------------------------------------------------------------------
// Core Operation Methods
//-----------------------------------------------------------------------------

void FanController::processUpdate() {
    if (!initialized) return;

    MutexGuard guard(mutex);
    if (!guard.isLocked()) return;

    updateRPM();

    // Stall detection
    if (currentPWM > config.minPWM && measuredRPM < config.minRPM) {
        stallCount++;
        if (stallCount >= STALL_RETRY_COUNT) {
            status = Status::SHUTOFF;
            currentPWM = 0;
            ledcWrite(PWM_CHANNEL, 0);
            return;
        }
    } else {
        stallCount = 0;
        status = Status::OK;
    }

    // Calculate target PWM based on current mode and conditions
    uint8_t newTargetPWM = targetPWM;
    
    if (nightModeEnabled && isNightTime()) {
        DEBUG_LOG("Night mode active, limiting PWM");
        newTargetPWM = min(newTargetPWM, config.nightMaxPWM);
    }

    if (targetPWM != newTargetPWM) {
        targetPWM = newTargetPWM;
        DEBUG_LOG("Target PWM adjusted to: %d", targetPWM);
    }

    if (status == Status::OK && currentPWM != targetPWM) {
        DEBUG_LOG("Updating currentPWM %d -> %d", currentPWM, targetPWM);
        currentPWM = targetPWM;
        ledcWrite(PWM_CHANNEL, currentPWM);
    }
}

void FanController::fanTask(void* parameters) {
    FanController* fan = static_cast<FanController*>(parameters);
    TickType_t lastRPMUpdate = xTaskGetTickCount();
    TickType_t lastEventCheck = xTaskGetTickCount();
    
    while (true) {
        fan->taskManager.updateTaskRunTime("Fan");
        
        TickType_t now = xTaskGetTickCount();
        
        // Check events more frequently than RPM
        if ((now - lastEventCheck) >= pdMS_TO_TICKS(EVENT_CHECK_INTERVAL)) {
            fan->processEvents();
            lastEventCheck = now;
        }
        
        // Update RPM less frequently
        if ((now - lastRPMUpdate) >= pdMS_TO_TICKS(RPM_UPDATE_INTERVAL)) {
            fan->processUpdate();
            lastRPMUpdate = now;
        }
        
        // Shorter delay to allow more responsive event handling
        vTaskDelay(pdMS_TO_TICKS(500));
    }
}

void FanController::updateRPM() {
    if (config.testMode) {
        // Simulate RPM in test mode with linear PWM-to-RPM relationship
        measuredRPM = map(currentPWM, 
                         config.minPWM, config.maxPWM, 
                         500, 2000);  // Simulate RPM range 500-2000
        DEBUG_LOG("Test Mode - Simulated RPM: %d for PWM: %d", measuredRPM, currentPWM);
        return;
    }

    // Calculate actual RPM from pulse count
    uint32_t pulses = pulseCount;
    pulseCount = 0;
    measuredRPM = (pulses * 60) / (RPM_UPDATE_INTERVAL / 1000.0) / FAN_PULSES_PER_REV;
}

void FanController::updatePWM() {
    if (status != Status::OK) return;

    if (currentPWM != targetPWM) {
        currentPWM = targetPWM;
        ledcWrite(PWM_CHANNEL, currentPWM);
        DEBUG_LOG("PWM updated to: %d", currentPWM);
    }
}

//-----------------------------------------------------------------------------
// Control Mode & Night Mode Methods
//-----------------------------------------------------------------------------

bool FanController::setControlMode(Mode newMode) {
    MutexGuard guard(mutex);
    if (!guard.isLocked()) return false;
    
    if (newMode == Mode::ERROR) return false;
    
    mode = newMode;
    if (events) {
        xEventGroupSetBits(events, CONTROL_MODE_CHANGED);
    }
    return true;
}

bool FanController::setPWMDutyCycle(int percentPWM) {
    if (!initialized || mode != Mode::MANUAL) return false;

    MutexGuard guard(mutex);
    if (!guard.isLocked()) return false;

    uint8_t rawPWM = percentToRawPWM(percentPWM);
    if (nightModeEnabled && isNightTime()) {  // Add isNightTime() check here
        applyNightMode(rawPWM);
    }
    targetPWM = rawPWM;
    return true;
}

void FanController::setTemperatureInternal(float temperature) {
    uint8_t rawPWM = calculatePWMForTemperature(temperature);
    DEBUG_LOG("Temperature %.2f -> rawPWM %d", temperature, rawPWM);
    
    if (nightModeEnabled) {
        uint8_t beforeNight = rawPWM;
        applyNightMode(rawPWM);
        DEBUG_LOG("Night mode adjusted PWM %d -> %d", beforeNight, rawPWM);
    }
    
    DEBUG_LOG("Setting targetPWM to %d", rawPWM);
    targetPWM = rawPWM;
}

// Modify existing public method to use internal version
bool FanController::setTemperature(float temperature) {
    if (!initialized || mode != Mode::AUTO) {
        DEBUG_LOG("setTemperature rejected - initialized: %d, mode: %d", initialized, (int)mode);
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

//-----------------------------------------------------------------------------
// Night Mode Implementation
//-----------------------------------------------------------------------------

bool FanController::setNightMode(bool enabled) {
    MutexGuard guard(mutex);
    if (!guard.isLocked()) return false;

    DEBUG_LOG("Night mode %s", enabled ? "enabled" : "disabled");
    nightModeEnabled = enabled;
    if (events) {
        xEventGroupSetBits(events, NIGHT_MODE_CHANGED);
    }
    return true;
}

bool FanController::setNightSettings(uint8_t startHour, uint8_t endHour, uint8_t maxPWM) {
    MutexGuard guard(mutex);
    if (!guard.isLocked()) return false;

    if (startHour > 23 || endHour > 23) return false;
    if (maxPWM < config.minPWM || maxPWM > config.maxPWM) return false;

    config.nightStartHour = startHour;
    config.nightEndHour = endHour;
    config.nightMaxPWM = maxPWM;

    DEBUG_LOG("Night settings updated - Start: %d, End: %d, MaxPWM: %d", 
              startHour, endHour, maxPWM);
    return true;
}

void FanController::registerNTPManager(NTPManager* manager) {
    MutexGuard guard(mutex);
    if (guard.isLocked()) {
        ntpManager = manager;
    }
}

bool FanController::isNightTime() const {
    // First check if we have valid NTP time
    if (ntpManager && ntpManager->isTimeSynchronized()) {
        int currentHour = ntpManager->getCurrentHour();
        if (currentHour < 0) {
            // NTP error, fall back to RTC
            return isNightTimeRTC();
        }

        if (config.nightStartHour < config.nightEndHour) {
            // Simple case: night period doesn't cross midnight
            return currentHour >= config.nightStartHour && 
                    currentHour < config.nightEndHour;
        } else {
            // Complex case: night period crosses midnight
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
        isNight = currentHour >= config.nightStartHour && currentHour < config.nightEndHour;
    } else {
        // Handles cases like 22:00 to 06:00
        isNight = currentHour >= config.nightStartHour || currentHour < config.nightEndHour;
    }
    
    DEBUG_LOG("Time check - Current hour: %d, Start: %d, End: %d, Is night: %d", 
              currentHour, config.nightStartHour, config.nightEndHour, isNight);
    
    return isNight;
}

//-----------------------------------------------------------------------------
// Status & Recovery Methods
//-----------------------------------------------------------------------------

bool FanController::attemptRecovery() {
    MutexGuard guard(mutex);
    if (!guard.isLocked()) return false;

    if (status != Status::SHUTOFF) return false;

    stallCount = 0;
    status = Status::OK;
    currentPWM = config.minPWM;
    targetPWM = config.minPWM;
    ledcWrite(PWM_CHANNEL, currentPWM);
    return true;
}

//-----------------------------------------------------------------------------
// Utility Methods
//-----------------------------------------------------------------------------

uint8_t FanController::calculatePWMForTemperature(float temp) {
    temp = constrain(temp, config.minTemp, config.maxTemp);
    float ratio = (temp - config.minTemp) / (config.maxTemp - config.minTemp);
    return config.minPWM + (ratio * (config.maxPWM - config.minPWM));
}

void FanController::applyNightMode(uint8_t& pwm) const {
    if (pwm > config.nightMaxPWM) {
        pwm = config.nightMaxPWM;
    }
}

uint8_t FanController::percentToRawPWM(int percent) const {
    return map(constrain(percent, 0, 100), 0, 100, config.minPWM, config.maxPWM);
}

int FanController::rawPWMToPercent(uint8_t raw) const {
    return map(raw, config.minPWM, config.maxPWM, 0, 100);
}

//-----------------------------------------------------------------------------
// Protected Getters
//-----------------------------------------------------------------------------

int FanController::getCurrentPWM() const {
    MutexGuard guard(mutex);
    if (!guard.isLocked()) return 0;
    return rawPWMToPercent(currentPWM);
}

int FanController::getTargetPWM() const {
    MutexGuard guard(mutex);
    if (!guard.isLocked()) return 0;
    return rawPWMToPercent(targetPWM);
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
    result += " - PWM: " + String(rawPWMToPercent(currentPWM)) + "%";
    result += " - RPM: " + String(measuredRPM);
    
    switch (status) {
        case Status::SHUTOFF:
            result += " (Shutoff)";
            break;
        case Status::ERROR:
            result += " (Error)";
            break;
        default:
            break;
    }
    
    return result;
}

bool FanController::isNightModeActive() const {
    MutexGuard guard(mutex);
    if (!guard.isLocked()) return false;
    return nightModeEnabled;
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

uint8_t FanController::getNightMaxPWM() const {
    MutexGuard guard(mutex);
    if (!guard.isLocked()) return 0;
    return config.nightMaxPWM;
}

bool FanController::isStalled() const {
    MutexGuard guard(mutex);
    if (!guard.isLocked()) return true; // Assume stalled if can't check
    return status == Status::SHUTOFF;
}