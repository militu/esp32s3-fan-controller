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
 * Features:
 * - Automatic NTP server synchronization
 * - Retry mechanism with exponential backoff
 * - Thread-safe time access
 * - Timezone and DST handling
 */
class NTPManager {
public:
    /**
     * @brief Constructor 
     * @param taskManager Reference to the system's task manager
     */
    explicit NTPManager(TaskManager& taskManager);
    ~NTPManager();

    // Prevent copying
    NTPManager(const NTPManager&) = delete;
    NTPManager& operator=(const NTPManager&) = delete;

    /**
     * @brief Initialize NTP manager and start background task
     * @return ESP_OK on success, error code otherwise
     */
    esp_err_t begin();

    // Time management
    int getCurrentHour() const;
    bool isTimeSynchronized() const;
    String getTimeString() const;
    bool forceSync();

    // Status tracking
    uint8_t getCurrentAttempt() const {
        MutexGuard guard(mutex);
        if (!guard.isLocked()) return 0;
        return syncAttempts;
    }
    
private:
    // Core components
    TaskManager& taskManager;
    SemaphoreHandle_t mutex;
    
    // State tracking
    bool initialized;
    bool timeSynchronized;
    uint32_t lastSyncTime;
    time_t lastSyncEpoch;
    uint8_t syncAttempts;

    // Retry mechanism
    bool attemptInProgress;
    uint32_t lastAttemptTime;
    uint32_t currentRetryDelay;

    // Task management
    static void ntpTask(void* parameters);
    void processUpdate();
    bool syncTime();
};

#endif // NTP_MANAGER_H