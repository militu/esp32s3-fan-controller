#define DEBUG_LOG(msg, ...) if (DEBUG_DISPLAY) { Serial.printf(msg "\n", ##__VA_ARGS__); }

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

    static uint32_t buf_size = driver->width() * 10;
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
    static uint32_t last_lvgl_time = 0;
    static uint32_t frame_count = 0;
    
    while (true) {
        uint32_t current_time = millis();
        uint32_t lvgl_interval = current_time - last_lvgl_time;
        
        // Log only every 100 frames to reduce overhead
        if (frame_count % 100 == 0) {
            DEBUG_LOG("Display: Frame %lu - Interval: %lu ms\n", frame_count, lvgl_interval);
        }
        last_lvgl_time = current_time;
        frame_count++;

        // Process any pending UI commands silently
        while (xQueueReceive(uiCommandQueue, &cmd, 0) == pdTRUE) {
            uint32_t start_update = millis();
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

        // Time the LVGL handler specifically
        uint32_t start_lvgl = millis();
        lv_timer_handler();
        uint32_t lvgl_time = millis() - start_lvgl;
        
        // Log only significant delays
        if (lvgl_time > 50) {  // Adjusted threshold to catch major delays
            DEBUG_LOG("Display: Long frame - LVGL took %lu ms\n", lvgl_time);
        }
        
        static uint32_t last_update = 0;
        uint32_t now = millis();
        
        if (now - last_update >= UPDATE_INTERVAL) {
            updateDisplayValues();
            last_update = now;
        }
        
        vTaskDelayUntil(&xLastWakeTime, pdMS_TO_TICKS(2));
    }
}void DisplayManager::updateDisplayValues() {
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