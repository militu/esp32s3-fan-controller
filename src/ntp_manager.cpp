#define DEBUG_LOG(msg, ...) if (Config::System::Debug::NTP) { Serial.printf(msg "\n", ##__VA_ARGS__); }

#include "ntp_manager.h"
#include <sys/time.h>

/*******************************************************************************
 * Construction / Destruction
 ******************************************************************************/

NTPManager::NTPManager(TaskManager& tm)
    : taskManager(tm)
    , mutex(xSemaphoreCreateMutex())
    , initialized(false)
    , timeSynchronized(false)
    , lastSyncTime(0)
    , lastSyncEpoch(0)
    , syncAttempts(0)
{
    if (!mutex) {
        DEBUG_LOG("NTPManager - Mutex creation failed!");
    }
}

NTPManager::~NTPManager() {
    if (mutex) {
        vSemaphoreDelete(mutex);
    }
}

/*******************************************************************************
 * Initialization
 ******************************************************************************/

esp_err_t NTPManager::begin() {
    DEBUG_LOG("NTP Manager Starting...");

    if (!mutex) {
        return ESP_ERR_NO_MEM;
    }

    MutexGuard guard(mutex);
    if (!guard.isLocked()) {
        return ESP_ERR_TIMEOUT;
    }

    // Configure NTP with timezone for France
    // This rule string handles both winter (CET) and summer (CEST) time
    // CET-1CEST,M3.5.0,M10.5.0/3 means:
    // - Base timezone is CET (UTC+1)
    // - CEST starts on the last Sunday of March
    // - CEST ends on the last Sunday of October at 3:00
    DEBUG_LOG("Configuring NTP with server: %s", Config::NTP::SERVER);
    configTime(0, 0, Config::NTP::SERVER, Config::NTP::BACKUP_SERVER);
    setenv("TZ", "CET-1CEST,M3.5.0,M10.5.0/3", 1);
    tzset();

    // Create background task
    TaskManager::TaskConfig taskConfig("NTP", NTP_STACK_SIZE, NTP_TASK_PRIORITY, NTP_TASK_CORE);
    esp_err_t err = taskManager.createTask(taskConfig, ntpTask, this);
    
    if (err != ESP_OK) {
        DEBUG_LOG("Failed to create NTP task: %d", err);
        return err;
    }

    initialized = true;
    DEBUG_LOG("NTP Manager initialized successfully");
    return ESP_OK;
}

/*******************************************************************************
 * Time Synchronization
 ******************************************************************************/

void NTPManager::processUpdate() {
    if (!initialized) return;

    MutexGuard guard(mutex);
    if (!guard.isLocked()) return;
    
    uint32_t currentTime = millis();
    
    // Handle initial sync or periodic resync
    if (!timeSynchronized || (currentTime - lastSyncTime >= Config::NTP::SYNC_INTERVAL)) {
        if (!attemptInProgress && 
            (currentTime - lastAttemptTime >= currentRetryDelay || lastAttemptTime == 0)) {
            
            if (syncAttempts >= Config::NTP::MAX_SYNC_ATTEMPTS) {
                DEBUG_LOG("NTP sync failed after max attempts");
                return;
            }

            // Start new attempt
            attemptInProgress = true;
            lastAttemptTime = currentTime;
            syncAttempts++;
            
            DEBUG_LOG("Starting NTP sync attempt %d/%d", 
                        syncAttempts, Config::NTP::MAX_SYNC_ATTEMPTS);
            
            struct tm timeinfo;
            if (getLocalTime(&timeinfo, Config::NTP::SYNC_TIMEOUT)) {
                lastSyncTime = currentTime;
                time(&lastSyncEpoch);
                timeSynchronized = true;
                attemptInProgress = false;
                syncAttempts = 0; // Reset on success
                
                char timeStr[32];
                strftime(timeStr, sizeof(timeStr), "%Y-%m-%d %H:%M:%S", &timeinfo);
                DEBUG_LOG("Time synchronized successfully: %s", timeStr);
            }
        }
        
        // Check if current attempt has timed out
        if (attemptInProgress && 
            (currentTime - lastAttemptTime >= Config::NTP::SYNC_TIMEOUT)) {
            attemptInProgress = false;
            currentRetryDelay *= Config::NTP::BACKOFF_FACTOR;
            DEBUG_LOG("Sync attempt %d failed, next delay: %lu ms", 
                        syncAttempts, currentRetryDelay);
        }
    }
}

bool NTPManager::forceSync() {
    DEBUG_LOG("Force sync requested");
    
    MutexGuard guard(mutex);
    if (!guard.isLocked()) return false;
    
    // Reset retry parameters
    syncAttempts = 0;
    currentRetryDelay = Config::NTP::RETRY_DELAY;
    lastAttemptTime = 0;
    attemptInProgress = false;
    
    return true;
}

bool NTPManager::syncTime() {
DEBUG_LOG("Starting time synchronization (attempt %d)...", syncAttempts + 1);

MutexGuard guard(mutex);
if (!guard.isLocked()) return false;

if (syncAttempts >= Config::NTP::MAX_SYNC_ATTEMPTS) {
    DEBUG_LOG("Maximum sync attempts reached");
    return false;
}

struct tm timeinfo;
bool success = getLocalTime(&timeinfo, Config::NTP::SYNC_TIMEOUT);

if (success) {
    lastSyncTime = millis();
    time(&lastSyncEpoch);
    timeSynchronized = true;
    syncAttempts = 0; // Reset on success
    
    char timeStr[32];
    strftime(timeStr, sizeof(timeStr), "%Y-%m-%d %H:%M:%S", &timeinfo);
    DEBUG_LOG("Time synchronized successfully: %s", timeStr);
} else {
    syncAttempts++;
    DEBUG_LOG("Sync attempt failed (%d/%d)", syncAttempts, Config::NTP::MAX_SYNC_ATTEMPTS);
}

return success;
}

/*******************************************************************************
 * Time Management
 ******************************************************************************/

int NTPManager::getCurrentHour() const {
    MutexGuard guard(mutex);
    if (!guard.isLocked() || !timeSynchronized) {
        DEBUG_LOG("Cannot get current hour - %s", 
                 !guard.isLocked() ? "mutex lock failed" : "time not synchronized");
        return -1;
    }

    struct tm timeinfo;
    if (!getLocalTime(&timeinfo)) {
        DEBUG_LOG("Failed to get local time");
        return -1;
    }

    return timeinfo.tm_hour;
}

String NTPManager::getTimeString() const {
    MutexGuard guard(mutex);
    if (!guard.isLocked()) {
        return "Mutex Error";
    }
    
    if (!timeSynchronized) {
        return "Not synchronized";
    }

    struct tm timeinfo;
    if (!getLocalTime(&timeinfo)) {
        return "Time Error";
    }

    char timeStr[64];
    // Format includes timezone indicator (CET/CEST)
    strftime(timeStr, sizeof(timeStr), "%H:%M:%S %Z", &timeinfo);
    return String(timeStr);
}

bool NTPManager::isTimeSynchronized() const {
    MutexGuard guard(mutex);
    if (!guard.isLocked()) return false;
    return timeSynchronized;
}

/*******************************************************************************
 * Task Implementation
 ******************************************************************************/

void NTPManager::ntpTask(void* parameters) {
    NTPManager* ntp = static_cast<NTPManager*>(parameters);
    
    DEBUG_LOG("NTP task started");
    
    // Initial delay to allow WiFi to connect
    vTaskDelay(pdMS_TO_TICKS(5000));
    
    // Initial sync attempt
    if (!ntp->syncTime()) {
        DEBUG_LOG("Initial time sync failed, will retry in background");
    }
    
    while (true) {
        ntp->taskManager.updateTaskRunTime("NTP");
        ntp->processUpdate();
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}