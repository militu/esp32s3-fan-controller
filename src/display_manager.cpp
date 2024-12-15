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
    , needsScreenTransition(false)
    , lastActivityTime(0)
    , screenOn(false)
    , displayEventQueue(nullptr)
{
}

bool DisplayManager::begin(DisplayDriver* displayDriver) {
    DEBUG_LOG_DISPLAY("DisplayManager: Starting initialization");

    if (initialized || !displayDriver) {
        DEBUG_LOG_DISPLAY("DisplayManager: Invalid initialization state");
        return false;
    }

    this->driver = displayDriver;
    DEBUG_LOG_DISPLAY("Display driver set");
    
    if (!this->driver->begin()) {
        DEBUG_LOG_DISPLAY("DisplayManager: Driver initialization failed");
        return false;
    }

    // Create queues...
    DisplayUpdateCommandQueue = xQueueCreate(Config::Display::DisplayUpdate::Queue::SIZE, 
                                           sizeof(DisplayUpdateCommand));
    if (!DisplayUpdateCommandQueue) {
        DEBUG_LOG_DISPLAY("DisplayManager: Queue creation failed");
        return false;
    }

    displayEventQueue = xQueueCreate(DISPLAY_EVENT_QUEUE_SIZE, sizeof(DisplayEventMessage));
    if (!displayEventQueue) {
        DEBUG_LOG_DISPLAY("Failed to create display event queue");
        return false;
    }

    screenOn = true;
    lastActivityTime = millis();

    bootUI.init(driver->width(), driver->height());
    dashboardUI.init(driver->width(), driver->height());

    // Make sure we're in a clean state before creating first screen
    lv_timer_handler();
    
    currentState = DisplayState::BOOT;
    bootUI.begin();

    // Create render task last
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
        bool locked = false;
        {
            MutexGuard guard(dashboardUI.getUIMutex(), pdMS_TO_TICKS(100));
            locked = guard.isLocked();
            if (locked) {
                lv_timer_handler();
            }
        }
        
        // Add delay if mutex failed
        if (!locked) {
            vTaskDelay(pdMS_TO_TICKS(10));
        }

        vTaskDelayUntil(&xLastWakeTime, pdMS_TO_TICKS(1));
    }
}

void DisplayManager::processDisplayUpdates() {
    TickType_t xLastWakeTime = xTaskGetTickCount();
    DisplayUpdateCommand cmd;
    
    while (true) {
        // Check for display events
        DisplayEventMessage event;
        while (xQueueReceive(displayEventQueue, &event, 0) == pdTRUE) {
                switch (event.event) {
                    case DisplayEvent::BUTTON_PRESS:
                        if (!screenOn) {
                            handleScreenPowerChange(true);
                        } else {
                            updateActivityTime();
                        }
                        break;
                }
        }

        // Check screen timeout periodically
        static uint32_t lastTimeoutCheck = 0;
        uint32_t now = millis();
        if (now - lastTimeoutCheck >= 1000) {  // Check every second
            lastTimeoutCheck = now;
            checkScreenTimeout();
        }
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

/*******************************************************************************
 * Screen timeout
 ******************************************************************************/

void DisplayManager::updateActivityTime() {
    if (!initialized || !driver) return;
    
    if (driver->lockUI(pdMS_TO_TICKS(100))) {
        lastActivityTime = millis();
        driver->unlockUI();
    }
}

void DisplayManager::checkScreenTimeout() {
    DEBUG_LOG_DISPLAY("Checking screen timeout...");
    
    if (!initialized || !driver) {
        DEBUG_LOG_DISPLAY("Skipping timeout check - not initialized (init: %d, driver: %p)", 
                         initialized, driver);
        return;
    }
    
    if (driver->lockUI(pdMS_TO_TICKS(100))) {
        DEBUG_LOG_DISPLAY("UI lock acquired for timeout check");
        uint32_t currentTime = millis();
        DEBUG_LOG_DISPLAY("Current time: %lu, Last activity: %lu, Diff: %lu, Timeout: %lu, Screen state: %s", 
                         currentTime, lastActivityTime, 
                         currentTime - lastActivityTime,
                         Config::Display::Sleep::SCREEN_TIMEOUT_MS,
                         screenOn ? "ON" : "OFF");
        
        if (screenOn && (currentTime - lastActivityTime >= Config::Display::Sleep::SCREEN_TIMEOUT_MS)) {
            DEBUG_LOG_DISPLAY("Timeout reached - turning screen off");
            handleScreenPowerChange(false);
        }
        
        driver->unlockUI();
        DEBUG_LOG_DISPLAY("UI lock released after timeout check");
    } else {
        DEBUG_LOG_DISPLAY("Failed to acquire UI lock for timeout check");
    }
}

void DisplayManager::handleScreenPowerChange(bool on) {
    if (!initialized || !driver) {
        DEBUG_LOG_DISPLAY("Cannot change screen power - not initialized");
        return;
    }
    
    DEBUG_LOG_DISPLAY("Screen power state changing to: %s", on ? "ON" : "OFF");
    screenOn = on;  // Set the state BEFORE calling setPower
    driver->setPower(on);
    
    if (on) {
        lastActivityTime = millis();
        DEBUG_LOG_DISPLAY("Activity timer reset to: %lu", lastActivityTime);
    }
    
    DEBUG_LOG_DISPLAY("Screen power state is now: %s", screenOn ? "ON" : "OFF");

}

void DisplayManager::handleButtonPress() {
    if (!initialized || !displayEventQueue) {
        DEBUG_LOG_DISPLAY("Cannot handle button press - not initialized");
        return;
    }

    DisplayEventMessage msg{DisplayEvent::BUTTON_PRESS};
    if (xQueueSend(displayEventQueue, &msg, pdMS_TO_TICKS(100)) != pdTRUE) {
        DEBUG_LOG_DISPLAY("Failed to queue button press event");
    }
}