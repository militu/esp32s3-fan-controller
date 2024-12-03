/**
 * @file task_manager.cpp
 * @brief Implementation of the TaskManager class for FreeRTOS task management
 */

#include "task_manager.h"

/*******************************************************************************
 * Construction / Destruction
 ******************************************************************************/

TaskManager::TaskManager() 
    : initialized(false)
    , suspended(false) {
    mutex = xSemaphoreCreateMutex();
}

TaskManager::~TaskManager() {
    stop();
    if (mutex) {
        vSemaphoreDelete(mutex);
    }
}

/*******************************************************************************
 * Initialization & Shutdown
 ******************************************************************************/

esp_err_t TaskManager::begin() {
    if (initialized) {
        return ESP_ERR_INVALID_STATE;
    }

    if (!mutex) {
        return ESP_ERR_NO_MEM;
    }

    initialized = true;
    return ESP_OK;
}

void TaskManager::stop() {
    if (!initialized) return;

    if (xSemaphoreTake(mutex, portMAX_DELAY) == pdTRUE) {
        // Clean up all active tasks
        for (size_t i = 0; i < Config::TaskManager::MAX_TASKS; i++) {
            if (tasks[i].active && tasks[i].handle) {
                vTaskDelete(tasks[i].handle);
                tasks[i].active = false;
                tasks[i].handle = nullptr;
            }
        }
        xSemaphoreGive(mutex);
    }

    initialized = false;
    suspended = false;
}

/*******************************************************************************
 * Task Management
 ******************************************************************************/

esp_err_t TaskManager::createTask(const TaskConfig& config, TaskFunction_t function, void* parameters) {
    // Validate initialization state
    if (!initialized || !mutex) {
        return ESP_ERR_INVALID_STATE;
    }

    if (xSemaphoreTake(mutex, portMAX_DELAY) != pdTRUE) {
        return ESP_ERR_TIMEOUT;
    }

    // Find available task slot
    int taskIndex = -1;
    for (size_t i = 0; i < Config::TaskManager::MAX_TASKS; i++) {
        if (!tasks[i].active) {
            taskIndex = i;
            break;
        }
    }

    if (taskIndex == -1) {
        xSemaphoreGive(mutex);
        return ESP_ERR_NO_MEM;
    }

    // Create FreeRTOS task
    BaseType_t result = xTaskCreatePinnedToCore(
        function,
        config.name,
        config.stackSize,
        parameters,
        config.priority,
        &tasks[taskIndex].handle,
        config.coreID
    );

    if (result != pdPASS) {
        xSemaphoreGive(mutex);
        return ESP_ERR_NO_MEM;
    }

    // Initialize task information
    tasks[taskIndex].config = config;
    tasks[taskIndex].active = true;
    resetTaskHealth(taskIndex);

    xSemaphoreGive(mutex);
    return ESP_OK;
}

esp_err_t TaskManager::deleteTask(const char* taskName) {
    if (!initialized || !mutex) {
        return ESP_ERR_INVALID_STATE;
    }

    if (xSemaphoreTake(mutex, portMAX_DELAY) != pdTRUE) {
        return ESP_ERR_TIMEOUT;
    }

    int taskIndex = findTaskIndex(taskName);
    if (taskIndex < 0) {
        xSemaphoreGive(mutex);
        return ESP_ERR_NOT_FOUND;
    }

    // Delete task and clean up resources
    if (tasks[taskIndex].handle) {
        vTaskDelete(tasks[taskIndex].handle);
        tasks[taskIndex].handle = nullptr;
        tasks[taskIndex].active = false;
    }

    xSemaphoreGive(mutex);
    return ESP_OK;
}

/*******************************************************************************
 * Resource Creation
 ******************************************************************************/

QueueHandle_t TaskManager::createQueue(size_t queueLength, size_t itemSize) {
    if (!initialized) {
        return nullptr;
    }
    return xQueueCreate(queueLength, itemSize);
}

SemaphoreHandle_t TaskManager::createMutex() {
    if (!initialized) {
        return nullptr;
    }
    return xSemaphoreCreateMutex();
}

EventGroupHandle_t TaskManager::createEventGroup() {
    if (!initialized) {
        return nullptr;
    }
    return xEventGroupCreate();
}

/*******************************************************************************
 * Health Monitoring
 ******************************************************************************/

bool TaskManager::checkTaskHealth() {
    if (!initialized || !mutex) {
        return false;
    }

    if (xSemaphoreTake(mutex, portMAX_DELAY) != pdTRUE) {
        return false;
    }

    bool allHealthy = true;
    for (size_t i = 0; i < Config::TaskManager::MAX_TASKS; i++) {
        if (tasks[i].active) {
            updateTaskHealth(i);
            if (!tasks[i].health.healthy) {
                allHealthy = false;
            }
        }
    }

    xSemaphoreGive(mutex);
    return allHealthy;
}

void TaskManager::updateTaskHealth(size_t taskIndex) {
    if (!tasks[taskIndex].active || !tasks[taskIndex].handle) {
        tasks[taskIndex].health.healthy = false;
        return;
    }

    TaskHealth& health = tasks[taskIndex].health;
    uint32_t currentTime = millis();
    
    // Monitor stack usage
    health.stackHighWaterMark = uxTaskGetStackHighWaterMark(tasks[taskIndex].handle);
    if (health.stackHighWaterMark < 200) {  // Critical stack threshold
        health.consecutiveFailures++;
        health.healthy = false;
        return;
    }

    // Check task state and runtime
    eTaskState state = eTaskGetState(tasks[taskIndex].handle);
    const uint32_t TASK_TIMEOUT_MS = 30000;  // 30 seconds timeout
    
    if ((state == eRunning || state == eReady || state == eBlocked) &&
        (currentTime - health.lastRunTime) < TASK_TIMEOUT_MS) {
        health.missedDeadlines = 0;
        health.consecutiveFailures = 0;
        health.healthy = true;
    } else {
        health.missedDeadlines++;
        health.consecutiveFailures++;
        health.healthy = false;
    }
}

void TaskManager::dumpTaskStatus() {
    if (!initialized || !mutex) {
        DEBUG_LOG_TASK_MANAGER("Task Manager not initialized!");
        return;
    }

    if (xSemaphoreTake(mutex, portMAX_DELAY) != pdTRUE) {
        DEBUG_LOG_TASK_MANAGER("Failed to take mutex in dumpTaskStatus!");
        return;
    }

    DEBUG_LOG_TASK_MANAGER("\n=== Task Status Dump ===");
    
    for (size_t i = 0; i < Config::TaskManager::MAX_TASKS; i++) {
        if (tasks[i].active && tasks[i].handle) {
            DEBUG_LOG_TASK_MANAGER("\nTask: %s\n", tasks[i].config.name);
            DEBUG_LOG_TASK_MANAGER("State: %d\n", eTaskGetState(tasks[i].handle));
            DEBUG_LOG_TASK_MANAGER("Priority: %d\n", tasks[i].config.priority);
            DEBUG_LOG_TASK_MANAGER("Stack High Water: %d\n", tasks[i].health.stackHighWaterMark);
            DEBUG_LOG_TASK_MANAGER("Last Run: %lu ms ago\n", millis() - tasks[i].health.lastRunTime);
            DEBUG_LOG_TASK_MANAGER("Missed Deadlines: %lu\n", tasks[i].health.missedDeadlines);
            DEBUG_LOG_TASK_MANAGER("Consecutive Failures: %lu\n", tasks[i].health.consecutiveFailures);
            DEBUG_LOG_TASK_MANAGER("Health: %s\n", tasks[i].health.healthy ? "HEALTHY" : "UNHEALTHY");
        }
    }
    
    DEBUG_LOG_TASK_MANAGER("\n=== End Task Status ===\n");
    
    xSemaphoreGive(mutex);
}

void TaskManager::updateTaskRunTime(const char* taskName) {
    if (xSemaphoreTake(mutex, portMAX_DELAY) == pdTRUE) {
        int taskIndex = findTaskIndex(taskName);
        if (taskIndex >= 0) {
            tasks[taskIndex].health.lastRunTime = millis();
            tasks[taskIndex].health.consecutiveFailures = 0;
        }
        xSemaphoreGive(mutex);
    }
}

/*******************************************************************************
 * Utility Methods
 ******************************************************************************/

int TaskManager::findTaskIndex(const char* taskName) const {
    for (size_t i = 0; i < Config::TaskManager::MAX_TASKS; i++) {
        if (tasks[i].active && strcmp(tasks[i].config.name, taskName) == 0) {
            return i;
        }
    }
    return -1;
}

void TaskManager::resetTaskHealth(size_t taskIndex) {
    if (taskIndex < Config::TaskManager::MAX_TASKS) {
        tasks[taskIndex].health = TaskHealth();
    }
}