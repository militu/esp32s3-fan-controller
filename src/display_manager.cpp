#include "display_manager.h"

/*******************************************************************************
 * Display Manager Implementation
 ******************************************************************************/

/**
 * Constructor for Display Manager
 * Initializes all dependencies and internal state
 */
DisplayManager::DisplayManager(TempSensor& ts, FanController& fc, WifiManager& wm, MqttManager& mm)
    : tempSensor(ts)         // Temperature sensor reference
    , fanController(fc)      // Fan controller reference
    , wifiManager(wm)        // WiFi manager reference
    , mqttManager(mm)        // MQTT manager reference
    , driver(nullptr)        // Display driver pointer (set in begin())
    , initialized(false)     // Initialization state
    , lastUpdate(0)         // Last update timestamp
{
}

/**
 * Initialize the display system and LVGL
 * @param displayDriver Pointer to the display driver implementation
 * @return true if initialization successful, false otherwise
 */
bool DisplayManager::begin(DisplayDriver* displayDriver) {
    Serial.println("DisplayManager::begin started");
    
    // Validate initialization state
    if (initialized || !displayDriver) {
        Serial.println("Display already initialized or null driver");
        return false;
    }

    // Initialize display driver
    driver = displayDriver;
    if (!driver->begin()) {
        Serial.println("Failed to initialize display driver!");
        return false;
    }

    // Initialize LVGL
    lv_init();

    // Setup display buffer
    static uint32_t buf_size = driver->width() * 10;
    static lv_color_t* buf = new lv_color_t[buf_size];
    if (!buf) {
        Serial.println("Failed to allocate display buffer!");
        return false;
    }

    // Initialize LVGL display buffer
    static lv_disp_draw_buf_t draw_buf;
    lv_disp_draw_buf_init(&draw_buf, buf, NULL, buf_size);

    // Configure LVGL display driver
    static lv_disp_drv_t disp_drv;
    lv_disp_drv_init(&disp_drv);
    disp_drv.hor_res = driver->width();
    disp_drv.ver_res = driver->height();
    disp_drv.flush_cb = flush_cb;
    disp_drv.draw_buf = &draw_buf;
    disp_drv.user_data = this;
    
    // Register display with LVGL
    if (!lv_disp_drv_register(&disp_drv)) {
        Serial.println("Failed to register display!");
        return false;
    }

    // Initialize UI with display dimensions
    ui.init(driver->width(), driver->height());
    ui.begin();
    
    updateTimer = lv_timer_create(lvglTimerCallback, 100, this);

    initialized = true;
    return true;
}

/**
 * Main processing loop for display updates
 * Handles periodic updates and LVGL task processing
 */
void DisplayManager::process() {
    if (!initialized) return;

    // Check if it's time for a periodic update
    uint32_t now = millis();
    if (now - lastUpdate >= UPDATE_INTERVAL) {
        updateDisplayValues();
        lastUpdate = now;
    }

    // Process LVGL tasks
    lv_timer_handler();
}

/**
 * Update all display values from system state
 */
void DisplayManager::updateDisplayValues() {
    if (!initialized) return;
    
    // Update UI with current system state
    ui.update(
        tempSensor.getSmoothedTemp(),      // Current temperature
        fanController.getCurrentPWM(),      // Current fan speed
        fanController.getTargetPWM(),       // Target fan speed
        fanController.getControlMode(),     // Fan control mode
        wifiManager.isConnected(),          // WiFi status
        mqttManager.isConnected(),          // MQTT status
        fanController.isNightModeActive()   // Night mode status
    );
}

/**
 * LVGL timer callback for periodic updates
 * @param timer LVGL timer instance
 */
void DisplayManager::lvglTimerCallback(lv_timer_t* timer) {
    DisplayManager* display = static_cast<DisplayManager*>(timer->user_data);
    if (display) {
        display->updateDisplayValues();
    }
}

/**
 * LVGL display flush callback
 * Handles transferring frame buffer to physical display
 */
void DisplayManager::flush_cb(lv_disp_drv_t* disp_drv, const lv_area_t* area, lv_color_t* color_p) {
    DisplayManager* display = static_cast<DisplayManager*>(disp_drv->user_data);
    if (display && display->driver) {
        display->driver->flush(area, (uint8_t*)color_p);
        lv_disp_flush_ready(disp_drv);
    }
}