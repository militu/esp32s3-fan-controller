#define DEBUG_LOG(msg, ...) if (DEBUG_NTP) { Serial.printf(msg "\n", ##__VA_ARGS__); }

#include "ntp_manager.h"
#include <sys/time.h>

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
    DEBUG_LOG("Configuring NTP with server: %s", NTP_SERVER);
    configTime(0, 0, NTP_SERVER, "time.nist.gov");
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

void NTPManager::processUpdate() {
    if (!initialized) {
        DEBUG_LOG("Not initialized, skipping update");
        return;
    }

    uint32_t currentTime = millis();
    
    MutexGuard guard(mutex);
    if (!guard.isLocked()) {
        DEBUG_LOG("Failed to acquire mutex in processUpdate");
        return;
    }

    if (!timeSynchronized || (currentTime - lastSyncTime >= SYNC_INTERVAL)) {
        DEBUG_LOG("Time sync needed. Synchronized: %d, Last sync: %lu ms ago", 
                 timeSynchronized, currentTime - lastSyncTime);
        syncTime();
    }
}

bool NTPManager::syncTime() {
    DEBUG_LOG("Starting time synchronization (attempt %d)...", ++syncAttempts);
    
    struct tm timeinfo;
    if (!getLocalTime(&timeinfo, SYNC_TIMEOUT)) {
        DEBUG_LOG("Failed to obtain time from NTP");
        return false;
    }

    MutexGuard guard(mutex);
    if (!guard.isLocked()) {
        DEBUG_LOG("Failed to acquire mutex in syncTime");
        return false;
    }

    lastSyncTime = millis();
    time(&lastSyncEpoch);
    timeSynchronized = true;

    char timeStr[32];
    strftime(timeStr, sizeof(timeStr), "%Y-%m-%d %H:%M:%S", &timeinfo);
    DEBUG_LOG("Time synchronized successfully: %s", timeStr);
    return true;
}

bool NTPManager::forceSync() {
    DEBUG_LOG("Force sync requested");
    return syncTime();
}

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
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}