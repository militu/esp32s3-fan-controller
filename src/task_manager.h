#ifndef TASK_MANAGER_H
#define TASK_MANAGER_H

#include <Arduino.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"
#include "freertos/event_groups.h"
#include "esp_err.h"
#include "config.h"
#include "debug_log.h"

/**
 * @brief FreeRTOS task management and monitoring system
 * 
 * Features:
 * - Task creation and lifecycle management
 * - Task health monitoring and status reporting
 * - Resource management (queues, semaphores, event groups)
 * - System health monitoring
 */
class TaskManager {
public:
    //--------------------------------------------------------------------------
    // Types & Structures
    //--------------------------------------------------------------------------
    struct TaskHealth {
        uint32_t lastRunTime;          ///< Last recorded runtime timestamp
        uint32_t missedDeadlines;      ///< Count of missed execution deadlines
        uint32_t consecutiveFailures;   ///< Count of consecutive health check failures
        UBaseType_t stackHighWaterMark; ///< Lowest free stack space observed
        bool healthy;                   ///< Current health status

        TaskHealth() 
            : lastRunTime(0)
            , missedDeadlines(0)
            , consecutiveFailures(0)
            , stackHighWaterMark(0)
            , healthy(true) {}
    };

    struct TaskConfig {
        const char* name;       ///< Task identifier name
        uint32_t stackSize;     ///< Stack size in bytes
        UBaseType_t priority;   ///< Task priority (0-configMAX_PRIORITIES)
        BaseType_t coreID;      ///< Core affinity (-1 for no affinity)
        
        TaskConfig() 
            : name("")
            , stackSize(0)
            , priority(0)
            , coreID(0) {}
        
        TaskConfig(const char* taskName, uint32_t stack, UBaseType_t prio, BaseType_t core)
            : name(taskName)
            , stackSize(stack)
            , priority(prio)
            , coreID(core) {}
    };

    //--------------------------------------------------------------------------
    // Construction / Destruction
    //--------------------------------------------------------------------------
    TaskManager();
    ~TaskManager();

    // Prevent copying
    TaskManager(const TaskManager&) = delete;
    TaskManager& operator=(const TaskManager&) = delete;

    //--------------------------------------------------------------------------
    // Initialization
    //--------------------------------------------------------------------------
    esp_err_t begin();
    void stop();

    //--------------------------------------------------------------------------
    // Task Management
    //--------------------------------------------------------------------------
    esp_err_t createTask(const TaskConfig& config, TaskFunction_t function, void* parameters = nullptr);
    esp_err_t deleteTask(const char* taskName);
    esp_err_t suspendTask(const char* taskName);
    esp_err_t resumeTask(const char* taskName);

    //--------------------------------------------------------------------------
    // Health Monitoring
    //--------------------------------------------------------------------------
    bool checkTaskHealth();
    void dumpTaskStatus();
    bool isSystemHealthy() const { return initialized && !suspended; }
    void updateTaskRunTime(const char* taskName);

    //--------------------------------------------------------------------------
    // Resource Management
    //--------------------------------------------------------------------------
    QueueHandle_t createQueue(size_t queueLength, size_t itemSize);
    SemaphoreHandle_t createMutex();
    EventGroupHandle_t createEventGroup();

private:
    //--------------------------------------------------------------------------
    // Internal Types
    //--------------------------------------------------------------------------
    struct TaskInfo {
        TaskHandle_t handle;   ///< FreeRTOS task handle
        TaskHealth health;     ///< Health monitoring data
        TaskConfig config;     ///< Task configuration
        bool active;           ///< Task status flag

        TaskInfo() : handle(nullptr), active(false) {}
    };

    //--------------------------------------------------------------------------
    // Member Variables
    //--------------------------------------------------------------------------
    bool initialized;                                 ///< Initialization state
    bool suspended;                                   ///< System suspension state
    SemaphoreHandle_t mutex;                          ///< Protection for shared resources
    TaskInfo tasks[Config::TaskManager::MAX_TASKS];   ///< Task tracking array

    //--------------------------------------------------------------------------
    // Internal Methods
    //--------------------------------------------------------------------------
    void updateTaskHealth(size_t taskIndex);
    int findTaskIndex(const char* taskName) const;
    void resetTaskHealth(size_t taskIndex);
};

#endif // TASK_MANAGER_H