#define DEBUG_LOG(msg, ...) if (DEBUG_WIFI) { Serial.printf(msg "\n", ##__VA_ARGS__); }

#include "wifi_manager.h"

WifiManager::WifiManager(TaskManager& tm)
    : taskManager(tm)
    , mutex(xSemaphoreCreateMutex())
    , currentState(SystemState::STARTING)
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
    DEBUG_LOG("Connecting to WiFi SSID: %s\n", WIFI_SSID);

    MutexGuard guard(mutex);
    if (!guard.isLocked()) {
        return ESP_ERR_TIMEOUT;
    }
    
    currentState = SystemState::WIFI_CONNECTING;
    
    // Use exponential backoff for retry delays
    currentRetryDelay = WIFI_RETRY_DELAY;
    
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
    
    connectionAttempts = 0;
    while (connectionAttempts < WIFI_MAX_RETRIES) {
        if (WiFi.status() == WL_CONNECTED) {
            currentState = SystemState::WIFI_CONNECTED;
            wasConnected = true;
            currentRetryDelay = WIFI_RETRY_DELAY;  // Reset delay on success
            DEBUG_LOG("WiFi connected! IP: %s", WiFi.localIP().toString().c_str());
            return ESP_OK;
        }
        
        vTaskDelay(pdMS_TO_TICKS(currentRetryDelay));
        currentRetryDelay *= WIFI_BACKOFF_FACTOR;  // Exponential backoff
        connectionAttempts++;
        DEBUG_LOG("Connection attempt %d failed, next delay: %lu ms", 
                 connectionAttempts, currentRetryDelay);
    }

    updateState(SystemState::WIFI_ERROR);
    DEBUG_LOG("WiFi connection failed after max attempts");
    return ESP_FAIL;
}

void WifiManager::processUpdate() {
    if (!initialized) return;

    MutexGuard guard(mutex);
    if (!guard.isLocked()) return;
    
    uint32_t currentTime = millis();
    
    // Periodic connection check and reconnection if needed
    if (currentTime - lastCheckTime >= WIFI_CHECK_INTERVAL) {
        lastCheckTime = currentTime;
        
        if (WiFi.status() != WL_CONNECTED) {
            DEBUG_LOG("WiFi connection lost. Reconnecting...");
            WiFi.disconnect();
            connect();
        }
    }
}

void WifiManager::updateState(SystemState newState) {
    MutexGuard guard(mutex);
    if (guard.isLocked()) {
        currentState = newState;
    }
}

String WifiManager::getStatusString() const {
    MutexGuard guard(mutex);
    if (!guard.isLocked()) return "Mutex Error";

    switch (currentState) {
        case SystemState::STARTING:             return "Starting";
        case SystemState::WIFI_CONNECTING:      return "Connecting";
        case SystemState::WIFI_CONNECTED:       return "Connected";
        case SystemState::WIFI_ERROR:           return "Error";
        case SystemState::RUNNING_WITH_WIFI:    return "Running (WiFi OK)";
        case SystemState::RUNNING_WITHOUT_WIFI: return "Running (No WiFi)";
        default:                               return "Unknown";
    }
}

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