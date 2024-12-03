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
 * Features:
 * - Automatic connection and reconnection handling
 * - Connection status monitoring 
 * - Exponential backoff for retry attempts
 * - Thread-safe operation with FreeRTOS
 */
class WifiManager {
public:
    /**
     * @brief Construct a new Wifi Manager object
     * @param taskManager Reference to the system's task manager
     */
    WifiManager(TaskManager& taskManager);
    ~WifiManager();

    // Prevent copying
    WifiManager(const WifiManager&) = delete;
    WifiManager& operator=(const WifiManager&) = delete;

    /**
     * @brief Initialize the WiFi manager and start background task
     * @return esp_err_t ESP_OK on success, error code otherwise
     */
    esp_err_t begin();

    // Status queries
    bool isConnected() const { return WiFi.status() == WL_CONNECTED; }
    Config::System::State getState() const { return currentState; }
    String getStatusString() const;
    IPAddress getIPAddress() const { return WiFi.localIP(); }
    int32_t getSignalStrength() const { return WiFi.RSSI(); }
    
    // Task management
    static void wifiTask(void* parameters);
    void processUpdate();

    /**
     * @brief Get current connection attempt count
     * @return Current attempt number (0 if not attempting)
     */
    uint8_t getCurrentAttempt() const {
        MutexGuard guard(mutex);
        if (!guard.isLocked()) return 0;
        return connectionAttempts;
    }

    uint32_t getTotalTimeout();

private:    
    // Core components
    TaskManager& taskManager;
    SemaphoreHandle_t mutex;
    
    // State tracking
    Config::System::State currentState;
    bool initialized;
    uint32_t lastCheckTime;
    uint8_t connectionAttempts;
    bool wasConnected;

    // Retry mechanism
    uint32_t currentRetryDelay;
    uint32_t lastAttemptTime;
    bool attemptInProgress;

    // Internal methods
    esp_err_t connect();
    void updateState(Config::System::State newState);
};

#endif // WIFI_MANAGER_H