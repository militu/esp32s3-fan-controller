/**
 * @file task_manager.h
 * @brief FreeRTOS task management and monitoring system
 * 
 * This class provides a wrapper around FreeRTOS functionality to manage tasks,
 * monitor their health, and handle task-related resources like queues and semaphores.
 */

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

class TaskManager {
public:
    //--------------------------------------------------------------------------
    // Constants
    //--------------------------------------------------------------------------
    
    /// Maximum number of tasks that can be managed simultaneously
    static const size_t MAX_TASKS = 10;
    
    /// Critical stack size threshold in bytes
    static const size_t STACK_WARNING_THRESHOLD = 200;

    //--------------------------------------------------------------------------
    // Types & Structures
    //--------------------------------------------------------------------------
    
    /**
     * @brief Monitors and tracks the health status of a task
     */
    struct TaskHealth {
        uint32_t lastRunTime;         ///< Last recorded runtime timestamp
        uint32_t missedDeadlines;     ///< Count of missed execution deadlines
        uint32_t consecutiveFailures; ///< Count of consecutive health check failures
        UBaseType_t stackHighWaterMark; ///< Lowest free stack space observed
        bool healthy;                 ///< Current health status

        /// Initialize a healthy task state
        TaskHealth() 
            : lastRunTime(0)
            , missedDeadlines(0)
            , consecutiveFailures(0)
            , stackHighWaterMark(0)
            , healthy(true) {}
    };

    /**
     * @brief Configuration parameters for task creation
     */
    struct TaskConfig {
        const char* name;     ///< Task identifier name
        uint32_t stackSize;   ///< Stack size in bytes
        UBaseType_t priority; ///< Task priority (0-configMAX_PRIORITIES)
        BaseType_t coreID;    ///< Core affinity (-1 for no affinity)
        
        /// Default configuration
        TaskConfig() 
            : name("")
            , stackSize(0)
            , priority(0)
            , coreID(0) {}
        
        /**
         * @brief Create a task configuration
         * @param taskName Unique identifier for the task
         * @param stack Stack size in bytes
         * @param prio Task priority level
         * @param core Core ID for task execution (-1 for any core)
         */
        TaskConfig(const char* taskName, uint32_t stack, UBaseType_t prio, BaseType_t core)
            : name(taskName)
            , stackSize(stack)
            , priority(prio)
            , coreID(core) {}
    };

    //--------------------------------------------------------------------------
    // Construction & Destruction
    //--------------------------------------------------------------------------
    
    TaskManager();
    ~TaskManager();

    // Prevent copying
    TaskManager(const TaskManager&) = delete;
    TaskManager& operator=(const TaskManager&) = delete;

    //--------------------------------------------------------------------------
    // Initialization
    //--------------------------------------------------------------------------
    
    /**
     * @brief Initialize the task manager
     * @return ESP_OK on success, appropriate error code otherwise
     */
    esp_err_t begin();

    /**
     * @brief Stop all tasks and cleanup resources
     */
    void stop();

    //--------------------------------------------------------------------------
    // Task Management
    //--------------------------------------------------------------------------
    
    /**
     * @brief Create and start a new task
     * @param config Task configuration parameters
     * @param function Task function to execute
     * @param parameters Optional parameters passed to the task
     * @return ESP_OK on success, error code otherwise
     */
    esp_err_t createTask(const TaskConfig& config, TaskFunction_t function, void* parameters = nullptr);

    /**
     * @brief Remove a task by name
     * @param taskName Name of the task to delete
     * @return ESP_OK on success, error code otherwise
     */
    esp_err_t deleteTask(const char* taskName);

    /**
     * @brief Temporarily pause a task
     * @param taskName Name of the task to suspend
     * @return ESP_OK on success, error code otherwise
     */
    esp_err_t suspendTask(const char* taskName);

    /**
     * @brief Resume a suspended task
     * @param taskName Name of the task to resume
     * @return ESP_OK on success, error code otherwise
     */
    esp_err_t resumeTask(const char* taskName);

    //--------------------------------------------------------------------------
    // Health Monitoring
    //--------------------------------------------------------------------------
    
    /**
     * @brief Check health status of all active tasks
     * @return true if all tasks are healthy, false otherwise
     */
    bool checkTaskHealth();

    /**
     * @brief Print detailed status of all tasks to Serial
     */
    void dumpTaskStatus();

    /**
     * @brief Get overall system health status
     * @return true if system is running normally, false otherwise
     */
    bool isSystemHealthy() const { return initialized && !suspended; }

    /**
     * @brief Update task's last run timestamp
     * @param taskName Name of the task to update
     */
    void updateTaskRunTime(const char* taskName);

    //--------------------------------------------------------------------------
    // Resource Management
    //--------------------------------------------------------------------------
    
    /**
     * @brief Create a FreeRTOS queue
     * @param queueLength Maximum number of items in queue
     * @param itemSize Size of each queue item in bytes
     * @return Queue handle or nullptr on failure
     */
    QueueHandle_t createQueue(size_t queueLength, size_t itemSize);

    /**
     * @brief Create a FreeRTOS mutex
     * @return Mutex handle or nullptr on failure
     */
    SemaphoreHandle_t createMutex();

    /**
     * @brief Create a FreeRTOS event group
     * @return Event group handle or nullptr on failure
     */
    EventGroupHandle_t createEventGroup();

private:
    //--------------------------------------------------------------------------
    // Internal Types
    //--------------------------------------------------------------------------
    
    /**
     * @brief Internal task tracking information
     */
    struct TaskInfo {
        TaskHandle_t handle;  ///< FreeRTOS task handle
        TaskHealth health;    ///< Health monitoring data
        TaskConfig config;    ///< Task configuration
        bool active;          ///< Task status flag

        TaskInfo() : handle(nullptr), active(false) {}
    };

    //--------------------------------------------------------------------------
    // Member Variables
    //--------------------------------------------------------------------------
    
    bool initialized;             ///< Initialization state
    bool suspended;              ///< System suspension state
    SemaphoreHandle_t mutex;     ///< Protection for shared resources
    TaskInfo tasks[MAX_TASKS];   ///< Task tracking array

    //--------------------------------------------------------------------------
    // Internal Methods
    //--------------------------------------------------------------------------
    
    /**
     * @brief Update health metrics for a specific task
     * @param taskIndex Index of task in tracking array
     */
    void updateTaskHealth(size_t taskIndex);

    /**
     * @brief Find task index by name
     * @param taskName Name to search for
     * @return Task index or -1 if not found
     */
    int findTaskIndex(const char* taskName) const;

    /**
     * @brief Reset health monitoring data for a task
     * @param taskIndex Index of task to reset
     */
    void resetTaskHealth(size_t taskIndex);
};

#endif // TASK_MANAGER_H