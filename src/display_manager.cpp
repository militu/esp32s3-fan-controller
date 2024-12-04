#include "display_manager.h"

/*******************************************************************************
 * Display Manager Implementation
 ******************************************************************************/

/**
 * Constructor for Display Manager
 * Initializes all dependencies and internal state
 */
DisplayManager::DisplayManager(TaskManager& tm, TempSensor& ts, FanController& fc, WifiManager& wm, MqttManager& mm)
    : taskManager(tm)
    , tempSensor(ts)
    , fanController(fc)
    , wifiManager(wm)
    , mqttManager(mm)
    , driver(nullptr)
    , initialized(false)
    , needsScreenTransition(false) // Initialize screen transition flag
{
}

bool DisplayManager::begin(DisplayDriver* displayDriver) {
    DEBUG_LOG_DISPLAY("DisplayManager: Starting initialization");

    if (initialized || !displayDriver) {
        DEBUG_LOG_DISPLAY("DisplayManager: Invalid initialization state");
        return false;
    }

    this->driver = displayDriver;

    if (!this->driver->begin()) {
        DEBUG_LOG_DISPLAY("DisplayManager: Driver initialization failed");
        return false;
    }

    DisplayUpdateCommandQueue = xQueueCreate(Config::Display::DisplayUpdate::Queue::SIZE, sizeof(DisplayUpdateCommand));
    if (!DisplayUpdateCommandQueue) {
        DEBUG_LOG_DISPLAY("DisplayManager: Queue creation failed");
        return false;
    }

    bootUI.init(driver->width(), driver->height());
    dashboardUI.init(driver->width(), driver->height());

    currentState = DisplayState::BOOT;
    bootUI.begin();

    TaskManager::TaskConfig renderConfig {
        "DisplayRender",
        Config::Display::DisplayRender::STACK_SIZE,
        Config::Display::DisplayRender::TASK_PRIORITY,
        Config::Display::DisplayRender::TASK_CORE
    };

    esp_err_t err = taskManager.createTask(renderConfig, displayRenderTask, this);
    if (err != ESP_OK) {
        DEBUG_LOG_DISPLAY("DisplayManager: DisplayRender task creation failed with error %d", err);
        return false;
    }

    TaskManager::TaskConfig updateConfig {
        "DisplayUpdate",
        Config::Display::DisplayUpdate::STACK_SIZE,
        Config::Display::DisplayUpdate::TASK_PRIORITY,
        Config::Display::DisplayUpdate::TASK_CORE
    };

    err = taskManager.createTask(updateConfig, displayUpdateTask, this);
    if (err != ESP_OK) {
        DEBUG_LOG_DISPLAY("DisplayManager: DisplayUpdate task creation failed with error %d", err);
        return false;
    }

    initialized = true;
    DEBUG_LOG_DISPLAY("DisplayManager: Initialization complete");
    return true;
}

/*******************************************************************************
 * Tasks
 ******************************************************************************/

void DisplayManager::displayRenderTask(void* parameters) {
    DisplayManager* display = static_cast<DisplayManager*>(parameters);
    display->processDisplayRender();
}

void DisplayManager::displayUpdateTask(void* parameters) {
    DisplayManager* display = static_cast<DisplayManager*>(parameters);
    display->processDisplayUpdates();
}

void DisplayManager::processDisplayRender() {
    TickType_t xLastWakeTime = xTaskGetTickCount();

    while (true) {
        {
            MutexGuard guard(dashboardUI.getUIMutex(), pdMS_TO_TICKS(10));
            if (guard.isLocked()) {
                lv_timer_handler();
            }
        }

        vTaskDelayUntil(&xLastWakeTime, pdMS_TO_TICKS(1));
    }
}

void DisplayManager::processDisplayUpdates() {
    TickType_t xLastWakeTime = xTaskGetTickCount();
    DisplayUpdateCommand cmd;
    
    while (true) {
        // Handle screen transition if needed
        if (needsScreenTransition) {
            DEBUG_LOG_DISPLAY("Executing screen transition to dashboard");
            dashboardUI.begin();
            needsScreenTransition = false;
            DEBUG_LOG_DISPLAY("Screen transition complete");
        }

        // Process screen-specific logic
        switch (currentState) {
            case DisplayState::BOOT:
                break;

            case DisplayState::DASHBOARD:
                // Process any pending UI commands
                while (xQueueReceive(DisplayUpdateCommandQueue, &cmd, 0) == pdTRUE) {
                    dashboardUI.update(
                        cmd.temperature,
                        cmd.currentSpeed,
                        cmd.targetSpeed,
                        cmd.controlMode,
                        cmd.wifiConnected,
                        cmd.mqttConnected,
                        cmd.nightModeEnabled,
                        cmd.nightModeActive
                    );
                }
                
                // Regular display updates
                static uint32_t last_update = 0;
                uint32_t now = millis();
                if (now - last_update >= Config::Display::DisplayRender::UPDATE_INTERVAL) {
                    updateDashboardValues();
                    last_update = now;
                }
                break;
        }

        vTaskDelayUntil(&xLastWakeTime, pdMS_TO_TICKS(Config::Display::DisplayRender::TASK_DELAY));
    }
}

void DisplayManager::switchToDashboardUI() {
    DEBUG_LOG_DISPLAY("Attempting to switch to dashboard. Initialized: %d, Current State: %d", 
                     initialized, static_cast<int>(currentState));

    if (!initialized) {
        DEBUG_LOG_DISPLAY("Cannot switch to dashboard - not initialized");
        return;
    }
    
    if (currentState == DisplayState::DASHBOARD) {
        DEBUG_LOG_DISPLAY("Already in dashboard state");
        return;
    }
    
    DEBUG_LOG_DISPLAY("Requesting dashboard transition");
    currentState = DisplayState::DASHBOARD;
    needsScreenTransition = true;  // Request transition
    
    // Wait a bit to ensure transition is processed
    delay(50);
    
    // Force an immediate update of the dashboard values
    updateDashboardValues();
    DEBUG_LOG_DISPLAY("Dashboard switch requested");
}

void DisplayManager::updateBootStatus(const char* component, BootScreen::ComponentStatus status) {
    if (!initialized || currentState != DisplayState::BOOT) return;
    bootUI.updateStatus(component, status);
}

void DisplayManager::updateBootStatusDetail(const char* component, 
                                          BootScreen::ComponentStatus status,
                                          const char* detail) {
    if (!initialized || currentState != DisplayState::BOOT) return;
    bootUI.updateStatusWithDetail(component, status, detail);
}

void DisplayManager::updateDashboardValues() {
    if (!initialized) return;

    // Create a command with current state
    DisplayUpdateCommand cmd(
        tempSensor.getSmoothedTemp(),
        fanController.getCurrentSpeed(),
        fanController.getTargetSpeed(),
        fanController.getControlMode(),
        wifiManager.isConnected(),
        mqttManager.isConnected(),
        fanController.isNightModeEnabled(),
        fanController.isNightModeActive()
    );

    // Send to queue, with a short timeout
    // We use a timeout of 0 to prevent blocking if queue is full
    if (xQueueSend(DisplayUpdateCommandQueue, &cmd, 0) != pdTRUE) {
        // Optional: Add error handling if queue is full
        // For now, we'll just skip this update
    }
}

void DisplayManager::showComponentStatus(const char* component, 
                                      BootScreen::ComponentStatus status,
                                      const char* detail) {
    updateBootStatus(component, status);
    updateBootStatusDetail(component, status, detail);
}

/*******************************************************************************
 * Wifi
 ******************************************************************************/

void DisplayManager::showWifiInitializing() {
    showComponentStatus("WiFi", BootScreen::ComponentStatus::WORKING, "Starting initialization...");
}

void DisplayManager::showWifiConnecting(uint8_t attempt, uint8_t maxAttempts) {
    char detail[64];
    snprintf(detail, sizeof(detail), "Connecting... (Attempt %d/%d)", 
            attempt, maxAttempts);
    showComponentStatus("WiFi", BootScreen::ComponentStatus::WORKING, detail);
}

void DisplayManager::showWifiConnected(const char* ssid, const IPAddress& ip) {
    char detail[64];
    snprintf(detail, sizeof(detail), "Connected to %s (%s)", 
             ssid, ip.toString().c_str());
    showComponentStatus("WiFi", BootScreen::ComponentStatus::SUCCESS, detail);
}

void DisplayManager::showWifiFailed(const char* reason) {
    showComponentStatus("WiFi", BootScreen::ComponentStatus::FAILED, reason);
}

/*******************************************************************************
 * NTP
 ******************************************************************************/

void DisplayManager::showNTPInitializing() {
    showComponentStatus("NTP", BootScreen::ComponentStatus::WORKING, 
                        "Starting time service...");
}

void DisplayManager::showNTPSyncing(uint8_t attempt, uint8_t maxAttempts) {
    char detail[64];
    snprintf(detail, sizeof(detail), "Synchronizing time (Attempt %d/%d)...", 
             attempt, maxAttempts);
    showComponentStatus("NTP", BootScreen::ComponentStatus::WORKING, detail);
}

void DisplayManager::showNTPSynced(const String& timeStr) {
    char detail[64];
    snprintf(detail, sizeof(detail), "Time synchronized: %s", timeStr.c_str());
    showComponentStatus("NTP", BootScreen::ComponentStatus::SUCCESS, detail);
}

void DisplayManager::showNTPFailed(const char* reason) {
    showComponentStatus("NTP", BootScreen::ComponentStatus::FAILED, reason);
}

/*******************************************************************************
 * MQTT
 ******************************************************************************/

void DisplayManager::showMQTTInitializing() {
    showComponentStatus("MQTT", BootScreen::ComponentStatus::WORKING, 
                        "Starting MQTT service...");
}

void DisplayManager::showMQTTConnecting(uint8_t attempt, uint8_t maxAttempts) {
    char detail[64];
    snprintf(detail, sizeof(detail), "Connecting to broker (Attempt %d/%d)...", 
             attempt, maxAttempts);
    showComponentStatus("MQTT", BootScreen::ComponentStatus::WORKING, detail);
}

void DisplayManager::showMQTTConnected() {
    showComponentStatus("MQTT", BootScreen::ComponentStatus::SUCCESS, 
                        "Connected to broker");
}

void DisplayManager::showMQTTFailed(const char* reason) {
    showComponentStatus("MQTT", BootScreen::ComponentStatus::FAILED, reason);
}
