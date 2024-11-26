#pragma once

#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>

/**
 * @brief RAII wrapper for FreeRTOS mutex handling.
 * 
 * This class implements the RAII pattern for FreeRTOS mutexes, automatically
 * acquiring the mutex on construction and releasing it on destruction.
 * It supports both const and non-const mutex handles.
 */
class MutexGuard {
public:
    /**
     * @brief Constructor for non-const mutex
     * @param m The mutex handle to be locked
     * @param timeout Maximum time to wait for the mutex (default: portMAX_DELAY)
     */
    MutexGuard(SemaphoreHandle_t& m, TickType_t timeout = portMAX_DELAY) 
        : mutex(m)
        , locked(false) 
    {
        if (xSemaphoreTake(mutex, timeout) == pdTRUE) {
            locked = true;
        }
    }

    /**
     * @brief Constructor for const mutex
     * @param m The const mutex handle to be locked
     * @param timeout Maximum time to wait for the mutex (default: portMAX_DELAY)
     */
    MutexGuard(const SemaphoreHandle_t& m, TickType_t timeout = portMAX_DELAY) 
        : mutex(const_cast<SemaphoreHandle_t&>(m))
        , locked(false) 
    {
        if (xSemaphoreTake(mutex, timeout) == pdTRUE) {
            locked = true;
        }
    }
    
    /**
     * @brief Destructor - automatically releases the mutex if it was locked
     */
    ~MutexGuard() {
        if (locked) {
            xSemaphoreGive(mutex);
        }
    }

    /**
     * @brief Check if the mutex was successfully locked
     * @return true if the mutex is locked, false otherwise
     */
    bool isLocked() const { 
        return locked; 
    }

private:
    SemaphoreHandle_t& mutex;  ///< Reference to the underlying mutex handle
    bool locked;               ///< Flag indicating if the mutex was successfully locked

    // Prevent copying to maintain RAII semantics
    MutexGuard(const MutexGuard&) = delete;
    MutexGuard& operator=(const MutexGuard&) = delete;
};