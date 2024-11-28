#ifndef NTP_MANAGER_H
#define NTP_MANAGER_H

#include <Arduino.h>
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include "time.h"
#include "config.h"
#include "task_manager.h"
#include "mutex_guard.h"

/**
 * @brief Manages NTP time synchronization for the system
 * 
 * Handles NTP server connection, time synchronization, and provides
 * thread-safe access to current time information. Uses FreeRTOS
 * tasks for background processing and mutex for thread safety.
 */
class NTPManager {
public:
    /**
     * @brief Construct a new NTP Manager object
     * @param taskManager Reference to the system's task manager
     */
    explicit NTPManager(TaskManager& taskManager);
    ~NTPManager();

    // Prevent copying
    NTPManager(const NTPManager&) = delete;
    NTPManager& operator=(const NTPManager&) = delete;

    /**
     * @brief Initialize the NTP manager and start background task
     * @return esp_err_t ESP_OK on success, error code otherwise
     */
    esp_err_t begin();

    /**
     * @brief Get current hour in 24-hour format
     * @return int Current hour (0-23) or -1 on error
     */
    int getCurrentHour() const;

    /**
     * @brief Check if time has been synchronized
     * @return true if time is synchronized
     */
    bool isTimeSynchronized() const;

    /**
     * @brief Get current time as a formatted string
     * @return String Current time in HH:MM:SS format
     */
    String getTimeString() const;

    /**
     * @brief Force an immediate time synchronization
     * @return true if synchronization was successful
     */
    bool forceSync();
    uint8_t getCurrentAttempt() const {
        MutexGuard guard(mutex);
        if (!guard.isLocked()) return 0;
        return syncAttempts;
    }

    static constexpr uint8_t MAX_SYNC_ATTEMPTS = 3;
    
private:
    // Constants
    static constexpr uint32_t NTP_STACK_SIZE = 4096;
    static constexpr UBaseType_t NTP_TASK_PRIORITY = 1;
    static constexpr BaseType_t NTP_TASK_CORE = 1;

    // Core components
    TaskManager& taskManager;
    SemaphoreHandle_t mutex;
    
    // State tracking
    bool initialized;
    bool timeSynchronized;
    uint32_t lastSyncTime;
    time_t lastSyncEpoch;
    uint32_t syncAttempts;

    // Task management
    static void ntpTask(void* parameters);
    void processUpdate();
    bool syncTime();

    // Internal time handling
    void updateLocalTime();
    static void timeAvailableCallback(struct timeval* tv);

    bool attemptInProgress;
    uint32_t lastAttemptTime;
    uint32_t currentRetryDelay;
    
};

#endif // NTP_MANAGER_H