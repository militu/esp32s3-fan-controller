// wifi_manager.h
#ifndef WIFI_MANAGER_H
#define WIFI_MANAGER_H

#include <Arduino.h>
#include <WiFi.h>
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "config.h"
#include "task_manager.h"
#include "mutex_guard.h"

/**
 * @brief Manages WiFi connectivity for the ESP32
 * 
 * Handles WiFi connection, monitoring, and reconnection attempts.
 * Uses FreeRTOS tasks for background processing and mutex for thread safety.
 */
class WifiManager {
public:
    /**
     * @brief Construct a new Wifi Manager object
     * @param taskManager Reference to the system's task manager
     */
    WifiManager(TaskManager& taskManager);
    ~WifiManager();

    // Prevent copying to avoid resource conflicts
    WifiManager(const WifiManager&) = delete;
    WifiManager& operator=(const WifiManager&) = delete;

    /**
     * @brief Initialize the WiFi manager and start the background task
     * @return esp_err_t ESP_OK on success, error code otherwise
     */
    esp_err_t begin();

    // Status queries
    bool isConnected() const { return WiFi.status() == WL_CONNECTED; }
    SystemState getState() const { return currentState; }
    String getStatusString() const;
    IPAddress getIPAddress() const { return WiFi.localIP(); }
    int32_t getSignalStrength() const { return WiFi.RSSI(); }
    
    // Task management
    static void wifiTask(void* parameters);
    void processUpdate();

    uint8_t getCurrentAttempt() const {
        MutexGuard guard(mutex);
        if (!guard.isLocked()) return 0;
        return connectionAttempts;
    }

private:
    // Core components
    TaskManager& taskManager;
    SemaphoreHandle_t mutex;
    
    // State tracking
    SystemState currentState;
    bool initialized;
    uint32_t lastCheckTime;
    uint32_t connectionAttempts;
    bool wasConnected;

    // Internal methods
    esp_err_t connect();
    void updateState(SystemState newState);

    // Task configuration constants
    static constexpr uint32_t WIFI_STACK_SIZE = 4096;
    static constexpr UBaseType_t WIFI_TASK_PRIORITY = 2;
    static constexpr BaseType_t WIFI_TASK_CORE = 0;

    uint32_t currentRetryDelay;  // For implementing backoff
    uint32_t lastAttemptTime;     // Track when we last attempted connection
    bool attemptInProgress;       // Flag for connection attempt in progress

};

#endif // WIFI_MANAGER_H