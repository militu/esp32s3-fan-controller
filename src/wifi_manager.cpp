#define DEBUG_LOG(msg, ...) if (Config::System::Debug::WIFI) { Serial.printf(msg "\n", ##__VA_ARGS__); }

#include "wifi_manager.h"

/*******************************************************************************
 * Construction / Destruction
 ******************************************************************************/

WifiManager::WifiManager(TaskManager& tm)
    : taskManager(tm)
    , mutex(xSemaphoreCreateMutex())
    , currentState(Config::System::State::STARTING)
    , initialized(false)
    , lastCheckTime(0)
    , connectionAttempts(0)
    , wasConnected(false) 
{
    if (!mutex) {
        DEBUG_LOG("WifiManager - Mutex creation failed!");
    }
}

WifiManager::~WifiManager() {
    if (mutex) {
        vSemaphoreDelete(mutex);
    }
}

/*******************************************************************************
 * Initialization & Connection
 ******************************************************************************/

esp_err_t WifiManager::begin() {
    DEBUG_LOG("WiFi Manager Starting...");

    if (!mutex) {
        return ESP_ERR_NO_MEM;
    }

    MutexGuard guard(mutex);
    if (!guard.isLocked()) {
        return ESP_ERR_TIMEOUT;
    }

    // Initialize WiFi in station mode
    WiFi.mode(WIFI_STA);
    WiFi.disconnect(true);
    delay(100);

    // Create background task for WiFi management
    TaskManager::TaskConfig taskConfig("WiFi", WIFI_STACK_SIZE, WIFI_TASK_PRIORITY, WIFI_TASK_CORE);
    esp_err_t err = taskManager.createTask(taskConfig, wifiTask, this);
    
    if (err != ESP_OK) {
        return err;
    }

    initialized = true;
    DEBUG_LOG("WiFi Manager initialized successfully");
    return ESP_OK;
}

esp_err_t WifiManager::connect() {
    DEBUG_LOG("Connecting to WiFi SSID: %s\n", Config::WiFi::SSID);

    MutexGuard guard(mutex);
    if (!guard.isLocked()) {
        return ESP_ERR_TIMEOUT;
    }
    
    currentState = Config::System::State::WIFI_CONNECTING;
    connectionAttempts = 0;
    currentRetryDelay = Config::WiFi::RETRY_DELAY;
    lastAttemptTime = 0;
    attemptInProgress = false;
    
    return ESP_OK;
}

/*******************************************************************************
 * Connection Management
 ******************************************************************************/

void WifiManager::processUpdate() {
    if (!initialized) return;

    MutexGuard guard(mutex);
    if (!guard.isLocked()) return;
    
    uint32_t currentTime = millis();
    
    // Handle connection attempts
    if (currentState == Config::System::State::WIFI_CONNECTING) {
        if (!attemptInProgress && 
            (currentTime - lastAttemptTime >= currentRetryDelay || lastAttemptTime == 0)) {
            
            if (connectionAttempts >= Config::WiFi::MAX_RETRIES) {
                updateState(Config::System::State::WIFI_ERROR);
                DEBUG_LOG("WiFi connection failed after max attempts");
                return;
            }

            // Start new attempt
            WiFi.begin(Config::WiFi::SSID, Config::WiFi::PASSWORD);
            attemptInProgress = true;
            lastAttemptTime = currentTime;
            connectionAttempts++;
            
            DEBUG_LOG("Starting connection attempt %d/%d", connectionAttempts, Config::WiFi::MAX_RETRIES);
        }
        
        // Check connection status
        if (attemptInProgress) {
            if (WiFi.status() == WL_CONNECTED) {
                currentState = Config::System::State::WIFI_CONNECTED;
                wasConnected = true;
                attemptInProgress = false;
                DEBUG_LOG("WiFi connected! IP: %s", WiFi.localIP().toString().c_str());
            } else if (currentTime - lastAttemptTime >= Config::WiFi::RETRY_DELAY) {
                // Attempt timed out
                attemptInProgress = false;
                currentRetryDelay *= Config::WiFi::BACKOFF_FACTOR;
                DEBUG_LOG("Connection attempt %d failed, next delay: %lu ms", 
                         connectionAttempts, currentRetryDelay);
            }
        }
    }
    
    // Periodic connection check
    if (currentTime - lastCheckTime >= Config::WiFi::CHECK_INTERVAL) {
        lastCheckTime = currentTime;
        if (WiFi.status() != WL_CONNECTED && currentState != Config::System::State::WIFI_CONNECTING) {
            DEBUG_LOG("WiFi connection lost. Reconnecting...");
            WiFi.disconnect();
            connect();
        }
    }
}

void WifiManager::updateState(Config::System::State newState) {
    MutexGuard guard(mutex);
    if (guard.isLocked()) {
        currentState = newState;
    }
}

/*******************************************************************************
 * Status Reporting
 ******************************************************************************/

String WifiManager::getStatusString() const {
    MutexGuard guard(mutex);
    if (!guard.isLocked()) return "Mutex Error";

    switch (currentState) {
        case Config::System::State::STARTING:             return "Starting";
        case Config::System::State::WIFI_CONNECTING:      return "Connecting";
        case Config::System::State::WIFI_CONNECTED:       return "Connected";
        case Config::System::State::WIFI_ERROR:           return "Error";
        case Config::System::State::RUNNING_WITH_WIFI:    return "Running (WiFi OK)";
        case Config::System::State::RUNNING_WITHOUT_WIFI: return "Running (No WiFi)";
        default:                               return "Unknown";
    }
}

/*******************************************************************************
 * Task Management
 ******************************************************************************/

void WifiManager::wifiTask(void* parameters) {
    WifiManager* wifi = static_cast<WifiManager*>(parameters);
    
    // Initial connection attempt
    if (wifi->connect() != ESP_OK) {
        DEBUG_LOG("Initial WiFi connection failed");
    }
    
    // Main task loop
    while (true) {
        wifi->taskManager.updateTaskRunTime("WiFi");
        wifi->processUpdate();
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}