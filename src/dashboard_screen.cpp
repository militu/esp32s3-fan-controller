#include "dashboard_screen.h"
#include "display_colors.h"

// Define the static string constants
const char DashboardScreen::MY_MOON_SYMBOL[] = "\xEF\x86\x86";
const char DashboardScreen::MY_TOWER_BROADCAST[] = "\xEF\x94\x99";

/*******************************************************************************
 * Construction / Destruction
 ******************************************************************************/

DashboardScreen::DashboardScreen()
    : displayWidth(0)
    , displayHeight(0)
    , screen(nullptr)
    , tempMeter(nullptr)
    , tempLabel(nullptr)
    , modeLabel(nullptr)
    , wifiLabel(nullptr)
    , mqttLabel(nullptr)
    , nightLabel(nullptr)
    , speedMeter(nullptr)
    , speedLabel(nullptr)
    , currentSpeedIndicator(nullptr)
    , modeIndicator(nullptr)
    , targetSpeedIndicator(nullptr)
    , initialized(false)
    , tempAnimationInProgress(false)
    , currentSpeedAnimationInProgress(false)
    , currentTempValue(0)
    , currentSpeedValue(0)
    , targetSpeedValue(0) {
    uiMutex = xSemaphoreCreateMutex();

}

DashboardScreen::~DashboardScreen() {
    if (uiMutex != nullptr) {
        vSemaphoreDelete(uiMutex);
    }
}

/*******************************************************************************
 * Core UI Management
 ******************************************************************************/

bool DashboardScreen::begin() {
    DEBUG_LOG_DISPLAY("Dashboard begin entry - Task: 0x%lx", (unsigned long)xTaskGetCurrentTaskHandle());
    
    if (initialized) {
        DEBUG_LOG_DISPLAY("Dashboard already initialized");
        return true;
    }
    
    // Create the screen
    createMainScreen();
    if (!screen) {
        DEBUG_LOG_DISPLAY("Failed to create main screen");
        return false;
    }

    // Calculate dimensions
    uint16_t margin = displayWidth * Config::Display::Dashboard::MARGIN_TO_WIDTH_RATIO;
    uint16_t topBarHeight = displayHeight * Config::Display::Dashboard::TopBar::HEIGHT_TO_SCREEN_RATIO;
    
    // Create UI elements
    createTopStatusBar(topBarHeight);
    createMainContent(topBarHeight + margin, displayHeight - topBarHeight - margin * 2);

    // Make sure all animations are stopped and initial values are set
    tempAnimationInProgress = false;
    currentSpeedAnimationInProgress = false;
    currentTempValue = 0;
    targetSpeedValue = 0;
    
    // Wait for any pending LVGL operations
    delay(10);

    // Load the screen
    lv_scr_load(screen);
    delay(50); // Give time for the screen to load
    
    initialized = true;
    DEBUG_LOG_DISPLAY("Dashboard initialization complete");
    return true;
}

void DashboardScreen::init(uint16_t width, uint16_t height) {
    displayWidth = width;
    displayHeight = height;
    DEBUG_LOG_DISPLAY("Dashboard screen initialized with dimensions: %dx%d", width, height);
}

void DashboardScreen::update(float temp, int fanSpeed, int targetSpeed, FanController::Mode mode,
                      bool wifiConnected, bool mqttConnected, bool nightModeEnabled, bool nightModeActive) {
    if (!initialized) return;

    updateTemperatureDisplay(temp);
    updateStatusIndicators(wifiConnected, mqttConnected, nightModeEnabled, nightModeActive);
    updateSpeedDisplay(fanSpeed, targetSpeed);
    updateModeDisplay(mode);
}

bool DashboardScreen::isInitialized() const {
    MutexGuard guard(uiMutex);
    if (!guard.isLocked()) return false;
    return initialized && screen != nullptr;
}

/*******************************************************************************
 * UI Layout Construction
 ******************************************************************************/

void DashboardScreen::createTopStatusBar(uint16_t height) {

    // Create container for status indicators
    lv_obj_t* topBar = lv_obj_create(screen);
    lv_obj_set_scrollbar_mode(topBar, LV_SCROLLBAR_MODE_OFF);
    lv_obj_set_size(topBar, displayWidth, height);
    lv_obj_set_pos(topBar, 0, 0);
    
    // Subtle, solid background that's slightly lighter than BG_DARK
    lv_obj_set_style_bg_color(topBar, lv_color_hex(DisplayColors::BG_TOPBAR), LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(topBar, LV_OPA_100, LV_STATE_DEFAULT);  // Half transparency
    lv_obj_set_style_border_width(topBar, 0, LV_STATE_DEFAULT);
    lv_obj_set_style_radius(topBar, 0, LV_STATE_DEFAULT);

    // Calculate margins and spacing
    uint16_t sideMargin = displayWidth * Config::Display::Dashboard::TopBar::SIDE_PADDING_RATIO;
    uint16_t iconSpacing = displayWidth * Config::Display::Dashboard::TopBar::ICON_GAP_RATIO;

    // Create status indicators with larger font
    const lv_font_t* iconFont = &lv_font_montserrat_16;

    // Left side status indicators
    wifiLabel = createStatusLabel(topBar, LV_ALIGN_LEFT_MID, sideMargin, 0, LV_SYMBOL_WIFI);
    mqttLabel = createStatusLabel(topBar, LV_ALIGN_LEFT_MID, sideMargin + iconSpacing, 0, MY_TOWER_BROADCAST);
    
    // Right side indicator
    nightLabel = createStatusLabel(topBar, LV_ALIGN_RIGHT_MID, -sideMargin, 0, MY_MOON_SYMBOL);

    // Set fonts
    lv_obj_set_style_text_font(wifiLabel, iconFont, LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(mqttLabel, &fa_tower_broadcast_16, LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(nightLabel, &fa_moon_16, LV_STATE_DEFAULT);
}

void DashboardScreen::createMainScreen() {
    screen = lv_obj_create(NULL);
    if (!screen) {
        DEBUG_LOG_DISPLAY("Failed to create screen object");
        return;
    }

    lv_obj_set_scrollbar_mode(screen, LV_SCROLLBAR_MODE_OFF);
    
    // Match boot screen's gradient background
    lv_obj_set_style_bg_color(screen, lv_color_hex(DisplayColors::BG_DARK), LV_STATE_DEFAULT);
    lv_obj_set_style_bg_grad_color(screen, lv_color_hex(DisplayColors::BG_LIGHT), LV_STATE_DEFAULT);
    lv_obj_set_style_bg_grad_dir(screen, LV_GRAD_DIR_VER, LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(screen, LV_OPA_COVER, LV_STATE_DEFAULT);
}

void DashboardScreen::createMainContent(uint16_t startY, uint16_t height) {
    // Calculate optimal dimensions
    uint16_t meterSize = displayWidth * Config::Display::Dashboard::Meters::METER_SIZE_RATIO;
    // Calculate total remaining space
    uint16_t remainingSpace = displayWidth - (meterSize * 2);
    // Divide it into thirds
    uint16_t thirdSpace = remainingSpace / 3;

    uint16_t margin = thirdSpace; 
    uint16_t xPosFromLeft = thirdSpace;
    uint16_t xPosFromRight = thirdSpace;

    // Create and position temperature meter
    createTemperatureMeter(meterSize, xPosFromLeft);
    
    // Create and position speed meter
    createSpeedMeter(meterSize, xPosFromRight);
}

void DashboardScreen::createTemperatureMeter(uint16_t size, uint16_t xPosFromLeft) {
    // Create and setup container
    lv_obj_t* meter_container = lv_obj_create(screen);
    lv_obj_remove_style_all(meter_container);
        
    // Set container properties
    lv_obj_set_size(meter_container, size, size);
    lv_obj_set_style_bg_opa(meter_container, LV_OPA_0, LV_STATE_DEFAULT);
    lv_obj_set_style_border_width(meter_container, 0, LV_STATE_DEFAULT);
    lv_obj_set_style_pad_all(meter_container, 0, LV_STATE_DEFAULT);
    
    // Center the container
    lv_obj_align(meter_container, LV_ALIGN_BOTTOM_LEFT, xPosFromLeft, size * Config::Display::Dashboard::Meters::BOTTOM_OFFSET_RATIO);
    lv_obj_update_layout(meter_container);
    
    uint16_t widget_size = size * Config::Display::Dashboard::Meters::WIDGET_TO_CONTAINER_RATIO;

    // Create and setup temperature meter
    tempMeter = lv_meter_create(meter_container);
    // Remove default styles
    lv_obj_remove_style(tempMeter, NULL, LV_PART_INDICATOR);
    lv_obj_remove_style(tempMeter, NULL, LV_PART_MAIN);
    lv_obj_set_style_pad_all(tempMeter, 0, LV_PART_MAIN);

    lv_obj_set_size(tempMeter, widget_size, widget_size);
    lv_obj_center(tempMeter);
    lv_obj_set_style_bg_opa(tempMeter, LV_OPA_0, LV_PART_MAIN);
    lv_obj_set_user_data(meter_container, this);

    // Add and configure scale with updated colors
    lv_meter_scale_t* scale = lv_meter_add_scale(tempMeter);
    
    // Configure meter scale
    lv_meter_set_scale_ticks(tempMeter, scale, 51, 2, 10,      // 50 divisions = 1°C per minor tick
                            lv_color_hex(DisplayColors::METER));
    lv_meter_set_scale_major_ticks(tempMeter, scale, 10, 4,    // Every 10th tick = 10°C intervals
                                widget_size * Config::Display::Dashboard::Meters::Temperature::SCALE_THICKNESS_RATIO,
                                lv_color_hex(DisplayColors::METER), 10);
    lv_obj_set_style_text_color(tempMeter, lv_color_hex(DisplayColors::METER), LV_PART_TICKS);

    // Set scale range and angles to match the arc
    const int16_t angle_range = 270;  // Total angle range
    const int16_t angle_offset = 135; // Starting angle offset
    lv_meter_set_scale_range(tempMeter, scale, 
                            Config::Display::Dashboard::Meters::Temperature::MIN_TEMP,
                            Config::Display::Dashboard::Meters::Temperature::MAX_TEMP, 
                            angle_range, angle_offset);
    lv_arc_set_value(arcTempMeter, Config::Display::Dashboard::Meters::Temperature::MIN_TEMP);

    // Create and setup background arc
    arcTempMeter = lv_arc_create(meter_container);
    lv_obj_set_size(arcTempMeter, widget_size, widget_size);
    lv_obj_clear_flag(arcTempMeter, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_PRESS_LOCK | 
                     LV_OBJ_FLAG_CLICK_FOCUSABLE | LV_OBJ_FLAG_GESTURE_BUBBLE | 
                     LV_OBJ_FLAG_SNAPPABLE | LV_OBJ_FLAG_SCROLLABLE | 
                     LV_OBJ_FLAG_SCROLL_ELASTIC | LV_OBJ_FLAG_SCROLL_MOMENTUM | 
                     LV_OBJ_FLAG_SCROLL_CHAIN);

    // Remove arc knob and background
    lv_obj_set_style_bg_opa(arcTempMeter, 0, LV_PART_KNOB);  // Hide knob
    lv_obj_set_style_arc_opa(arcTempMeter, 0, LV_PART_MAIN); // Hide background arc
    lv_obj_set_style_arc_rounded(arcTempMeter, false, LV_PART_INDICATOR); // Remove rounded ends
    
    // Set arc properties and align with meter scale
    lv_obj_set_style_arc_opa(arcTempMeter, 255, LV_PART_INDICATOR);
    lv_obj_set_style_arc_img_src(arcTempMeter, &ui_img_gradient_225_170px_png, LV_PART_INDICATOR | LV_STATE_DEFAULT);
    lv_obj_set_style_arc_width(arcTempMeter, widget_size * Config::Display::Dashboard::Meters::Temperature::SCALE_THICKNESS_RATIO, LV_PART_INDICATOR);
    
    // Configure arc angles to match the meter scale
    lv_arc_set_rotation(arcTempMeter, angle_offset);
    lv_arc_set_bg_angles(arcTempMeter, 0, angle_range);
    lv_arc_set_angles(arcTempMeter, 0, 0); // Initial value
    
    // Set arc range to match the meter
    lv_arc_set_range(arcTempMeter, 
                     Config::Display::Dashboard::Meters::Temperature::MIN_TEMP * 2,  // Multiply by 2 since we're using half-degree precision
                     Config::Display::Dashboard::Meters::Temperature::MAX_TEMP * 2); // Multiply by 2 since we're using half-degree precision
    
    lv_obj_center(arcTempMeter);
    lv_obj_move_background(arcTempMeter);

    // Create and setup temperature label
    tempLabel = lv_label_create(tempMeter);
    lv_obj_center(tempLabel);
    lv_obj_set_style_text_font(tempLabel, &lv_font_montserrat_16, LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(tempLabel, lv_color_white(), LV_STATE_DEFAULT);
    lv_label_set_text(tempLabel, "0.0°C");
}

void DashboardScreen::createSpeedMeter(uint16_t size, uint16_t xPosFromRight) {
    // Create container for speed meter
    lv_obj_t* speed_container = lv_obj_create(screen);
    lv_obj_remove_style_all(speed_container);
        
    // Set container properties
    lv_obj_set_size(speed_container, size, size);
    lv_obj_set_style_bg_opa(speed_container, LV_OPA_0, LV_STATE_DEFAULT);
    lv_obj_set_style_border_width(speed_container, 0, LV_STATE_DEFAULT);
    lv_obj_set_style_pad_all(speed_container, 0, LV_STATE_DEFAULT);
    
    // Center the container
    lv_obj_align(speed_container, LV_ALIGN_BOTTOM_RIGHT, -xPosFromRight, size * Config::Display::Dashboard::Meters::BOTTOM_OFFSET_RATIO);
    lv_obj_update_layout(speed_container);
    
    uint16_t widget_size = size * Config::Display::Dashboard::Meters::WIDGET_TO_CONTAINER_RATIO;
    // Create speed meter
    speedMeter = lv_meter_create(speed_container);
    // Remove default indicator circle
    lv_obj_remove_style(speedMeter, NULL, LV_PART_INDICATOR);
    lv_obj_remove_style(speedMeter, NULL, LV_PART_MAIN);
    lv_obj_set_style_pad_all(speedMeter, 0, LV_PART_MAIN);

    lv_obj_set_size(speedMeter, widget_size, widget_size);
    lv_obj_center(speedMeter);
    lv_obj_set_style_bg_opa(speedMeter, LV_OPA_0, LV_PART_MAIN);
    lv_obj_set_user_data(speed_container, this);


    // Add scale for speed
    lv_meter_scale_t* scale = lv_meter_add_scale(speedMeter);
    lv_meter_set_scale_ticks(speedMeter, scale, 41, 2, 10,     // 40 divisions = 2.5% per minor tick
                            lv_color_hex(DisplayColors::METER));
    lv_meter_set_scale_major_ticks(speedMeter, scale, 8, 4,    // Every 8th tick = 20% intervals
                                widget_size * Config::Display::Dashboard::Meters::Fan::SCALE_THICKNESS_RATIO,
                                lv_color_hex(DisplayColors::METER), 10);
    lv_obj_set_style_text_color(speedMeter, lv_color_hex(DisplayColors::METER), LV_PART_TICKS);
    lv_meter_set_scale_range(speedMeter, scale,  
                             Config::Display::Dashboard::Meters::Fan::MIN_SPEED, 
                             Config::Display::Dashboard::Meters::Fan::MAX_SPEED, 270, 135);
    
    // Add target speed arc
    targetSpeedIndicator = lv_meter_add_arc(speedMeter, scale, 
                                            widget_size * Config::Display::Dashboard::Meters::Fan::ARC_THICKNESS_RATIO,
                                            lv_color_hex(DisplayColors::TARGET_SPEED), 0);
    lv_meter_set_indicator_value(speedMeter, targetSpeedIndicator, 0);

    // Add current speed arc
    currentSpeedIndicator = lv_meter_add_arc(speedMeter, scale, 
                                            widget_size * Config::Display::Dashboard::Meters::Fan::ARC_THICKNESS_RATIO,
                                            lv_color_hex(DisplayColors::CURRENT_SPEED), 
                                            -widget_size * Config::Display::Dashboard::Meters::Fan::ARC_THICKNESS_RATIO);
    lv_meter_set_indicator_value(speedMeter, currentSpeedIndicator, 0);

    // Create speed label
    speedLabel = lv_label_create(speedMeter);
    lv_obj_center(speedLabel);
    lv_obj_set_style_text_font(speedLabel, &lv_font_montserrat_16, LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(speedLabel, lv_color_white(), LV_STATE_DEFAULT);
    lv_label_set_text(speedLabel, "0%");

    modeIndicator = lv_label_create(speedMeter);
    lv_obj_set_style_text_font(modeIndicator, &lv_font_montserrat_14, LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(modeIndicator, lv_color_hex(DisplayColors::SUCCESS), LV_STATE_DEFAULT);
    // Position it at the bottom center, slightly below the center
    lv_obj_align(modeIndicator, LV_ALIGN_CENTER, 0, widget_size/3);
    lv_label_set_text(modeIndicator, "AUTO");
}

/*******************************************************************************
 * UI Element Creation
 ******************************************************************************/

lv_obj_t* DashboardScreen::createStatusLabel(lv_obj_t* parent, lv_align_t align, lv_coord_t x_ofs, lv_coord_t y_ofs, const char* text) {
    lv_obj_t* label = lv_label_create(parent);
    lv_obj_align(label, align, x_ofs, y_ofs);
    lv_obj_set_style_text_font(label, &lv_font_montserrat_14, LV_STATE_DEFAULT);
    // Set initial color explicitly during creation
    lv_obj_set_style_text_color(label, lv_color_hex(DisplayColors::INACTIVE), LV_STATE_DEFAULT);
    lv_label_set_text(label, text);
    return label;
}

/*******************************************************************************
 * UI State Updates
 ******************************************************************************/

void DashboardScreen::updateTemperatureDisplay(float temp) {
    MutexGuard guard(uiMutex, pdMS_TO_TICKS(10));
    if (!guard.isLocked()) return;

    int targetValue = constrain((int)(temp * 2), 
        Config::Display::Dashboard::Meters::Temperature::MIN_TEMP * 2,
        Config::Display::Dashboard::Meters::Temperature::MAX_TEMP * 2);

    if (!tempAnimationInProgress && targetValue != currentTempValue) {
        lv_anim_t anim;
        lv_anim_init(&anim);
        lv_anim_set_var(&anim, arcTempMeter);
        lv_anim_set_values(&anim, currentTempValue, targetValue);
        lv_anim_set_time(&anim, Config::Display::Dashboard::Meters::Animation::SPEED_MS);
        lv_anim_set_path_cb(&anim, lv_anim_path_ease_out);
        lv_anim_set_exec_cb(&anim, [](void* var, int32_t value) {
            lv_arc_set_value((lv_obj_t*)var, value);
        });
        lv_anim_set_ready_cb(&anim, [](lv_anim_t* a) {
            ((DashboardScreen*)lv_obj_get_user_data(lv_obj_get_parent((lv_obj_t*)a->var)))->tempAnimationInProgress = false;
        });
        
        tempAnimationInProgress = true;
        lv_anim_start(&anim);
        currentTempValue = targetValue;
    }

    // Update the temp label
    char tempStr[32];
    
    lv_color_t tempColor;
    if (temp < Config::Display::Dashboard::Meters::Temperature::GOOD_TO_WARNING_THRESHOLD) {
        tempColor = lv_color_hex(DisplayColors::TEMP_GOOD);
    } else if (temp < Config::Display::Dashboard::Meters::Temperature::WARNING_TO_CRITICAL_THRESHOLD) {
        tempColor = lv_color_hex(DisplayColors::TEMP_WARNING);
    } else {
        tempColor = lv_color_hex(DisplayColors::TEMP_CRITICAL);
    }
    
    snprintf(tempStr, sizeof(tempStr), "%.1f°C", temp);
    lv_label_set_text(tempLabel, tempStr);
    lv_obj_set_style_text_color(tempLabel, tempColor, LV_STATE_DEFAULT);
}

void DashboardScreen::updateStatusIndicators(bool wifiConnected, bool mqttConnected, 
                                             bool nightModeEnabled, bool nightModeActive) {
    MutexGuard guard(uiMutex, pdMS_TO_TICKS(10));
    if (!guard.isLocked()) return;

    // Update WiFi status with proper colors
    lv_obj_set_style_text_color(wifiLabel, 
        wifiConnected ? lv_color_hex(DisplayColors::SUCCESS) : lv_color_hex(DisplayColors::ERROR), 
        LV_STATE_DEFAULT);

    // Update MQTT status with broadcast tower icon
    lv_obj_set_style_text_color(mqttLabel,
        mqttConnected ? lv_color_hex(DisplayColors::SUCCESS) : lv_color_hex(DisplayColors::ERROR),
        LV_STATE_DEFAULT);

    // Night mode status (unchanged)
    lv_color_t nightColor = nightModeActive ? lv_color_hex(DisplayColors::SUCCESS) :
                           nightModeEnabled ? lv_color_hex(DisplayColors::WORKING) :
                                            lv_color_hex(DisplayColors::INACTIVE);
    lv_obj_set_style_text_color(nightLabel, nightColor, LV_STATE_DEFAULT);

    // Update last status
    lastStatus.wifiConnected = wifiConnected;
    lastStatus.mqttConnected = mqttConnected;
    lastStatus.nightModeEnabled = nightModeEnabled;
    lastStatus.nightModeActive = nightModeActive;
}

void DashboardScreen::updateSpeedDisplay(int fanSpeed, int targetSpeed) {
    MutexGuard guard(uiMutex, pdMS_TO_TICKS(10));
    if (!guard.isLocked()) return;

    if (!currentSpeedAnimationInProgress && fanSpeed != currentSpeedValue) {
        lv_anim_t anim;
        lv_anim_init(&anim);
        lv_anim_set_var(&anim, speedMeter);
        lv_anim_set_values(&anim, currentSpeedValue, fanSpeed);
        lv_anim_set_time(&anim, Config::Display::Dashboard::Meters::Animation::SPEED_MS);
        lv_anim_set_path_cb(&anim, lv_anim_path_ease_out);
        lv_anim_set_exec_cb(&anim, set_current_speed_value);
        lv_anim_set_ready_cb(&anim, [](lv_anim_t* a) {
            ((DashboardScreen*)lv_obj_get_user_data(lv_obj_get_parent((lv_obj_t*)a->var)))->currentSpeedAnimationInProgress = false;
        });
        
        currentSpeedAnimationInProgress = true;
        lv_anim_start(&anim);
        currentSpeedValue = fanSpeed;
    }

    if (!targetSpeedAnimationInProgress && targetSpeed != targetSpeedValue) {
        lv_anim_t anim;
        lv_anim_init(&anim);
        lv_anim_set_var(&anim, speedMeter);
        lv_anim_set_values(&anim, targetSpeedValue, targetSpeed);
        lv_anim_set_time(&anim, Config::Display::Dashboard::Meters::Animation::SPEED_MS);
        lv_anim_set_path_cb(&anim, lv_anim_path_ease_out);
        lv_anim_set_exec_cb(&anim, set_target_speed_value);
        lv_anim_set_ready_cb(&anim, [](lv_anim_t* a) {
            ((DashboardScreen*)lv_obj_get_user_data(lv_obj_get_parent((lv_obj_t*)a->var)))->targetSpeedAnimationInProgress = false;
        });
        
        targetSpeedAnimationInProgress = true;
        lv_anim_start(&anim);
        targetSpeedValue = targetSpeed;
    }

    // Update the speed label
    char speedStr[32];
    lv_color_t speedColor;
    if (fanSpeed < Config::Display::Dashboard::Meters::Fan::GOOD_TO_WARNING_THRESHOLD) {
        speedColor = lv_color_hex(DisplayColors::SPEED_GOOD);
    } else if (fanSpeed < Config::Display::Dashboard::Meters::Fan::WARNING_TO_CRITICAL_THRESHOLD) {
        speedColor = lv_color_hex(DisplayColors::SPEED_WARNING);
    } else {
        speedColor = lv_color_hex(DisplayColors::SPEED_CRITICAL);
    }

    snprintf(speedStr, sizeof(speedStr), "%d%%", fanSpeed);
    lv_label_set_text(speedLabel, speedStr);
    lv_obj_set_style_text_color(speedLabel, speedColor, LV_STATE_DEFAULT);
}

void DashboardScreen::updateModeDisplay(FanController::Mode mode) {
    MutexGuard guard(uiMutex, pdMS_TO_TICKS(10));
    if (!guard.isLocked()) return;
    
    lv_label_set_text(modeIndicator, mode == FanController::Mode::AUTO ? "AUTO" : "MANUAL");
    lv_obj_set_style_text_color(modeIndicator, 
        mode == FanController::Mode::AUTO ? lv_color_hex(DisplayColors::SUCCESS) : lv_color_hex(DisplayColors::TEMP_WARNING),
        LV_STATE_DEFAULT);
}

/*******************************************************************************
 * Utilities
 ******************************************************************************/

void DashboardScreen::addDebugPoint(lv_obj_t* parent, lv_coord_t x, lv_coord_t y, lv_color_t color) {
    // Create a small circle point
    lv_obj_t* point = lv_obj_create(parent);
    
    // Make it small (3x3 pixels)
    lv_obj_set_size(point, 8, 8);
    
    // Make it round
    lv_obj_set_style_radius(point, LV_RADIUS_CIRCLE, LV_STATE_DEFAULT);
    
    // Set the color
    lv_obj_set_style_bg_color(point, color, LV_STATE_DEFAULT);
    
    // Ensure it's visible by setting opacity
    lv_obj_set_style_bg_opa(point, LV_OPA_COVER, LV_STATE_DEFAULT);
    
    // Position it
    lv_obj_set_pos(point, x, y);
}


void DashboardScreen::addDebugPoints(lv_obj_t* meter_container, uint16_t size) {
    // Add more debug points to visualize the layout
    Serial.printf("Debug: Width = %d, Height = %d\n", lv_obj_get_width(meter_container), lv_obj_get_height(meter_container));

    // Container corners (red)
    addDebugPoint(meter_container, 0, 0, lv_color_make(255, 0, 0));  // Top-left
    addDebugPoint(meter_container, lv_obj_get_width(meter_container), 0, lv_color_make(255, 0, 0));  // Top-right
    addDebugPoint(meter_container, 0, lv_obj_get_height(meter_container), lv_color_make(255, 0, 0));  // Bottom-left
    addDebugPoint(meter_container, lv_obj_get_width(meter_container), lv_obj_get_height(meter_container), lv_color_make(255, 0, 0));  // Bottom-right
    
    // Container center (yellow)
    addDebugPoint(meter_container, 
                 lv_obj_get_width(meter_container)/2, 
                 lv_obj_get_height(meter_container)/2, 
                 lv_color_make(255, 255, 0));

}