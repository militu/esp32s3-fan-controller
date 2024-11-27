#include "display_manager.h"

/*******************************************************************************
 * Display Manager Implementation
 ******************************************************************************/

/**
 * Constructor for Display Manager
 * Initializes all dependencies and internal state
 */
DisplayManager::DisplayManager(TaskManager& tm, TempSensor& ts, FanController& fc, WifiManager& wm, MqttManager& mm)
    : taskManager(tm)
    , tempSensor(ts)
    , fanController(fc)
    , wifiManager(wm)
    , mqttManager(mm)
    , driver(nullptr)
    , initialized(false)
{
}

bool DisplayManager::begin(DisplayDriver* displayDriver) {
    if (initialized || !displayDriver) return false;
    
    // Add this line early in the method
    uiCommandQueue = xQueueCreate(QUEUE_SIZE, sizeof(UICommand));
    if (!uiCommandQueue) return false;

    driver = displayDriver;
    if (!driver->begin()) return false;

    lv_init();

    static uint32_t buf_size = driver->width() * 40;
    static lv_disp_draw_buf_t draw_buf;
    static lv_color_t *buf1 = (lv_color_t *)heap_caps_malloc(
        buf_size * sizeof(lv_color_t), MALLOC_CAP_DMA);
    static lv_color_t *buf2 = (lv_color_t *)heap_caps_malloc(
        buf_size * sizeof(lv_color_t), MALLOC_CAP_DMA);

    lv_disp_draw_buf_init(&draw_buf, buf1, buf2, buf_size);

    static lv_disp_drv_t disp_drv;
    lv_disp_drv_init(&disp_drv);
    disp_drv.hor_res = driver->width();
    disp_drv.ver_res = driver->height();
    disp_drv.flush_cb = flush_cb;
    disp_drv.draw_buf = &draw_buf;
    disp_drv.user_data = this;
    disp_drv.full_refresh = 0;


    if (!lv_disp_drv_register(&disp_drv)) return false;

    ui.init(driver->width(), driver->height());
    ui.begin();

    initialized = true;

    TaskManager::TaskConfig lvglConfig{
        "LVGL",
        LVGL_STACK_SIZE,
        LVGL_TASK_PRIORITY,
        LVGL_TASK_CORE
    };
    
    if (taskManager.createTask(lvglConfig, lvglTask, this) != ESP_OK) {
        initialized = false;
        return false;
    }

    return true;
}

void DisplayManager::lvglTask(void* parameters) {
    DisplayManager* display = static_cast<DisplayManager*>(parameters);
    display->processLVGL();
}

void DisplayManager::processLVGL() {
    TickType_t xLastWakeTime = xTaskGetTickCount();
    UICommand cmd;
    
    while (true) {
        // Process any pending UI commands
        while (xQueueReceive(uiCommandQueue, &cmd, 0) == pdTRUE) {
            // Update UI with the latest command
            ui.update(
                cmd.temperature,
                cmd.currentPWM,
                cmd.targetPWM,
                cmd.controlMode,
                cmd.wifiConnected,
                cmd.mqttConnected,
                cmd.nightMode
            );
        }

        // Handle LVGL timers and animations
        lv_timer_handler();
        
        static uint32_t last_update = 0;
        uint32_t now = millis();
        
        if (now - last_update >= UPDATE_INTERVAL) {
            updateDisplayValues();
            last_update = now;
        }
        
        vTaskDelayUntil(&xLastWakeTime, pdMS_TO_TICKS(2));
    }
}

void DisplayManager::updateDisplayValues() {
    if (!initialized) return;

    // Create a command with current state
    UICommand cmd(
        tempSensor.getSmoothedTemp(),
        fanController.getCurrentPWM(),
        fanController.getTargetPWM(),
        fanController.getControlMode(),
        wifiManager.isConnected(),
        mqttManager.isConnected(),
        fanController.isNightModeActive()
    );

    // Send to queue, with a short timeout
    // We use a timeout of 0 to prevent blocking if queue is full
    if (xQueueSend(uiCommandQueue, &cmd, 0) != pdTRUE) {
        // Optional: Add error handling if queue is full
        // For now, we'll just skip this update
    }
}

void DisplayManager::flush_cb(lv_disp_drv_t* disp_drv, const lv_area_t* area, lv_color_t* color_p) {
    DisplayManager* display = static_cast<DisplayManager*>(disp_drv->user_data);
    if (display && display->driver) {
        display->driver->flush(area, (uint8_t*)color_p);
        lv_disp_flush_ready(disp_drv);
    }
}