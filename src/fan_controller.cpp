/**
 * @file fan_controller.cpp
 * @brief Implementation of the FanController class for PWM-based fan control
 */

#include "fan_controller.h"
#include "temp_sensor.h"

// Static member initialization
volatile uint32_t FanController::pulseCount = 0;

/*******************************************************************************
 * Construction / Destruction
 ******************************************************************************/

FanController::FanController(TaskManager& tm, ConfigPreference& config)
    : taskManager(tm)
    , configPreference(config)
    , mutex(nullptr)
    , events(nullptr)
    , tempSensor(nullptr)
    , ntpManager(nullptr)
    , config{
        .minTemp = Config::Temperature::Control::MIN_TEMP,
        .maxTemp = Config::Temperature::Control::MAX_TEMP,
        .minSpeed = Config::Fan::Speed::MIN_PERCENT,
        .maxSpeed = Config::Fan::Speed::MAX_PERCENT,
        .minPWM = Config::Fan::Speed::MIN_PWM,
        .maxPWM = Config::Fan::Speed::MAX_PWM,
        .nightMaxSpeed = Config::NightMode::MAX_SPEED,
        .minRPM = Config::Fan::RPM::MINIMUM,
        .nightStartHour = Config::NightMode::START_HOUR,
        .nightEndHour = Config::NightMode::END_HOUR,
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
        DEBUG_LOG_FAN("FanController - Resource creation failed!");
    }
}

FanController::~FanController() {
    if (mutex) vSemaphoreDelete(mutex);
    if (events) vEventGroupDelete(events);
}

/*******************************************************************************
 * Initialization
 ******************************************************************************/

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
    ledcWrite(Config::Fan::PWM::CHANNEL, SpeedToRawPWM(currentSpeed));

    TaskManager::TaskConfig taskConfig("Fan", TASK_STACK_SIZE, TASK_PRIORITY, TASK_CORE);
    esp_err_t err = taskManager.createTask(taskConfig, fanTask, this);
    
    if (err != ESP_OK) return err;

    initialized = true;
    
    if (events) {
        xEventGroupSetBits(events, TEMP_UPDATED);
    }
    
    return ESP_OK;
}

/*******************************************************************************
 * Speed Control Methods
 ******************************************************************************/

void FanController::updateTargetSpeed(uint8_t requestedSpeed) {
    target.requestedSpeed = requestedSpeed;
    
    if (nightModeEnabled && isNightTime()) {
        // Use raw PWM values for comparison to avoid conversion rounding
        target.effectiveSpeed = min(requestedSpeed, config.nightMaxSpeed);
    } else {
        target.effectiveSpeed = target.requestedSpeed;
    }
    
    if (status == Status::OK && currentSpeed != target.effectiveSpeed) {
        currentSpeed = target.effectiveSpeed;
        ledcWrite(Config::Fan::PWM::CHANNEL, SpeedToRawPWM(currentSpeed));
    }
}

bool FanController::setSpeedDutyCycle(uint8_t percentSpeed) {
    if (!initialized || mode != Mode::MANUAL) return false;

    MutexGuard guard(mutex);
    if (!guard.isLocked()) return false;

    updateTargetSpeed(percentSpeed);

    // Save settings
    saveSettings(configPreference);

    return true;
}

void FanController::setTemperatureInternal(float temperature) {
    int speed = calculateSpeedForTemperature(temperature);
    DEBUG_LOG_FAN("Temperature %.2f -> speed %d", temperature, speed);
    updateTargetSpeed(speed);
}

bool FanController::setTemperature(float temperature) {
    if (!initialized || mode != Mode::AUTO) {
        DEBUG_LOG_FAN("setTemperature rejected - initialized: %d, mode: %d", 
                 initialized, (int)mode);
        return false;
    }

    MutexGuard guard(mutex);
    if (!guard.isLocked()) {
        DEBUG_LOG_FAN("setTemperature failed to acquire mutex");
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

    // Save settings
    saveSettings(configPreference);
    
    DEBUG_LOG_FAN("Control mode changed to: %s", 
              mode == Mode::AUTO ? "Auto" : "Manual");
    return true;
}

/*******************************************************************************
 * Night Mode Implementation
 ******************************************************************************/

bool FanController::setNightMode(bool enabled) {
    MutexGuard guard(mutex);
    if (!guard.isLocked()) return false;

    DEBUG_LOG_FAN("Night mode %s", enabled ? "enabled" : "disabled");
    nightModeEnabled = enabled;
    
    // Re-evaluate speed with new night mode state
    updateTargetSpeed(target.requestedSpeed);
    
    if (events) {
        xEventGroupSetBits(events, NIGHT_MODE_CHANGED);
    }

    // Save settings
    saveSettings(configPreference);

    return true;
}

bool FanController::validateNightSettings(uint8_t startHour, uint8_t endHour, uint8_t maxPercent) const {
    // Validate hours
    if (startHour > 23 || endHour > 23) return false;
    
    // Validate percentage range
    if (maxPercent < 0 || maxPercent > 100) return false;
    
    return true;
}

bool FanController::setNightSettings(uint8_t startHour, uint8_t endHour, uint8_t maxPercent) {
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
    
    DEBUG_LOG_FAN("Night settings updated - Start: %d, End: %d, MaxSpeed: %d%%", 
            startHour, endHour, maxPercent);

    // Only trigger update if settings actually changed
    if (prevStart != config.nightStartHour || 
        prevEnd != config.nightEndHour || 
        prevMaxSpeed != config.nightMaxSpeed) {
        
        // Always recalculate from original requested speed
        updateTargetSpeed(target.requestedSpeed);
    }

    // Save settings
    saveSettings(configPreference);

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
    
    DEBUG_LOG_FAN("Time check - Hour: %d, Start: %d, End: %d, Is night: %d", 
              currentHour, config.nightStartHour, config.nightEndHour, isNight);
    
    return isNight;
}

bool FanController::isNightModeEnabled() const {
    MutexGuard guard(mutex);
    if (!guard.isLocked()) return false;
    
    return nightModeEnabled;
}

bool FanController::isNightModeActive() const {
    MutexGuard guard(mutex);
    if (!guard.isLocked()) return false;
    
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

/*******************************************************************************
 * Core Operation Methods
 ******************************************************************************/

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
            ledcWrite(Config::Fan::PWM::CHANNEL, 0);
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

    DEBUG_LOG_FAN("Processing fan events: 0x%lx", (unsigned long)bits);

    // Handle temperature updates in auto mode
    if (mode == Mode::AUTO && tempSensor->isLastReadSuccess()) {
        float temp = tempSensor->getSmoothedTemp();
        DEBUG_LOG_FAN("Updating temperature to %.2f°C", temp);
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
        if ((now - lastRPMUpdate) >= pdMS_TO_TICKS(Config::Fan::RPM::UPDATE_INTERVAL)) {
            fan->processUpdate();
            lastRPMUpdate = now;
        }
        
        vTaskDelay(pdMS_TO_TICKS(500));
    }
}

/*******************************************************************************
 * Hardware Control Methods
 ******************************************************************************/

bool FanController::setupPWM() {
    ledcSetup(Config::Fan::PWM::CHANNEL, Config::Fan::PWM::FREQUENCY, Config::Fan::PWM::RESOLUTION);
    ledcAttachPin(Config::Fan::PWM::PIN, Config::Fan::PWM::CHANNEL);
    return true;
}

bool FanController::setupTachometer() {
    pinMode(Config::Fan::PWM::TACH_PIN, INPUT_PULLUP);
    attachInterrupt(digitalPinToInterrupt(Config::Fan::PWM::TACH_PIN), handleTachInterrupt, FALLING);
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
        DEBUG_LOG_FAN("Test Mode - Simulated RPM: %d for speed: %d", 
                 measuredRPM, currentSpeed);
        return;
    }

    // Calculate actual RPM from pulse count
    uint32_t pulses = pulseCount;
    pulseCount = 0;
    measuredRPM = (pulses * 60) / (Config::Fan::RPM::UPDATE_INTERVAL / 1000.0) / Config::Fan::RPM::PULSES_PER_REV;
}

/*******************************************************************************
 * Status & Recovery Methods
 ******************************************************************************/

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

/*******************************************************************************
 * Utility Methods
 ******************************************************************************/

uint8_t FanController::calculateSpeedForTemperature(float temp) const {
    temp = constrain(temp, config.minTemp, config.maxTemp);
    float ratio = (temp - config.minTemp) / (config.maxTemp - config.minTemp);
    return config.minSpeed + (ratio * (config.maxSpeed - config.minSpeed));
}

uint8_t FanController::SpeedToRawPWM(uint8_t percent) const {
    return map(constrain(percent, 0, 100), 0, 100, config.minPWM, config.maxPWM);
}

uint8_t FanController::rawPWMToSpeed(uint8_t raw) const {
    return map(raw, config.minPWM, config.maxPWM, 0, 100);
}

/*******************************************************************************
 * Protected Getters
 ******************************************************************************/

uint8_t FanController::getCurrentSpeed() const {
    MutexGuard guard(mutex);
    if (!guard.isLocked()) return 0;
    return currentSpeed;
}

uint8_t FanController::getTargetSpeed() const {
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

/*******************************************************************************
 * Component Registration
 ******************************************************************************/

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

/*******************************************************************************
 * Save and load preferences
 ******************************************************************************/

void FanController::saveSettings(ConfigPreference& configPref) {
    DEBUG_LOG_FAN("Saving settings - Mode: %d, Speed: %d, NightMode: %d", 
                  static_cast<uint8_t>(mode), currentSpeed, nightModeEnabled);
                  
    ConfigPreference::FanSettings settings;
    settings.fanMode = static_cast<uint8_t>(mode);
    settings.manualSpeed = currentSpeed;
    settings.nightModeEnabled = nightModeEnabled;
    settings.nightStartHour = config.nightStartHour;
    settings.nightEndHour = config.nightEndHour;
    settings.nightMaxSpeed = config.nightMaxSpeed;
    configPref.saveFanSettings(settings);
}

void FanController::loadSettings(ConfigPreference& configPref) {
    DEBUG_LOG_FAN("Loading fan settings...");
    ConfigPreference::FanSettings settings;
    if (configPref.loadFanSettings(settings)) {
        DEBUG_LOG_FAN("Loaded - Mode: %d, Speed: %d, NightMode: %d", 
                     settings.fanMode, settings.manualSpeed, settings.nightModeEnabled);
                     
        mode = static_cast<Mode>(settings.fanMode);
        if (mode == Mode::MANUAL) {
            setSpeedDutyCycle(settings.manualSpeed);
        }
        setNightMode(settings.nightModeEnabled);
        setNightSettings(settings.nightStartHour, 
                        settings.nightEndHour, 
                        settings.nightMaxSpeed);
    } else {
        DEBUG_LOG_FAN("Failed to load settings or using defaults");
    }
}