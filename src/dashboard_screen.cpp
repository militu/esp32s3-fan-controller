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

void DashboardScreen::begin() {
    if (initialized) return;
    
    // Create the screen first but don't load it yet
    createMainScreen();
    
    // Calculate base dimensions and spacing
    uint16_t margin = displayWidth * 0.04;
    uint16_t topBarHeight = displayHeight * 0.15;
    uint16_t delimiterHeight = displayHeight * 0.002;
    
    // Create all UI elements
    createTopStatusBar(topBarHeight);
    createDelimiter(topBarHeight, delimiterHeight);
    
    // Calculate main content area dimensions
    uint16_t contentStartY = topBarHeight + delimiterHeight + margin;
    uint16_t contentHeight = displayHeight - contentStartY - margin;
    
    // Create main content
    createMainContent(contentStartY, contentHeight);
    
    // Make sure all animations are stopped and initial values are set
    tempAnimationInProgress = false;
    currentSpeedAnimationInProgress = false;
    currentTempValue = 0;
    targetSpeedValue = 0;
    
    delay(1000);  // Give LVGL time to finish any pending operations
    // Now that everything is initialized, load the screen
    lv_scr_load(screen);
    
    // Mark as initialized after screen is loaded
    initialized = true;
}

void DashboardScreen::init(uint16_t width, uint16_t height) {
    displayWidth = width;
    displayHeight = height;
}

void DashboardScreen::update(float temp, int fanSpeed, int targetSpeed, FanController::Mode mode,
                      bool wifiConnected, bool mqttConnected, bool nightModeEnabled, bool nightModeActive) {
    if (!initialized) return;

    updateTemperatureDisplay(temp);
    updateStatusIndicators(wifiConnected, mqttConnected, nightModeEnabled, nightModeActive);
    updateSpeedDisplay(fanSpeed, targetSpeed);
    updateModeDisplay(mode);
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
    
    // Make container fully transparent
    lv_obj_set_style_bg_opa(topBar, LV_OPA_0, LV_STATE_DEFAULT);
    lv_obj_set_style_border_width(topBar, 0, LV_STATE_DEFAULT);
    
    // Calculate margins and spacing
    uint16_t sideMargin = displayWidth * 0.01;
    uint16_t iconSpacing = displayWidth * 0.1;

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

void DashboardScreen::createDelimiter(uint16_t topOffset, uint16_t height) {
    // Calculate margins and dimensions
    uint16_t marginX = displayWidth * 0.05;  // Match boot screen margin
    uint16_t lineWidth = displayHeight * 0.005;  // Match boot screen line width
    
    // Create line
    lv_obj_t* delimiter = lv_line_create(screen);
    static lv_point_t linePoints[2];
    linePoints[0].x = marginX;
    linePoints[0].y = topOffset;
    linePoints[1].x = displayWidth - marginX;
    linePoints[1].y = topOffset;
    
    // Configure line
    lv_line_set_points(delimiter, linePoints, 2);
    lv_obj_set_style_line_color(delimiter, lv_color_hex(DisplayColors::BORDER), LV_PART_MAIN);
    lv_obj_set_style_line_width(delimiter, lineWidth, LV_PART_MAIN);
}

void DashboardScreen::createMainContent(uint16_t startY, uint16_t height) {
    // Calculate optimal dimensions
    uint16_t margin = displayWidth * 0.06;  // Space between meters
    uint16_t meterSize = displayWidth * 0.45;
    
    uint16_t startX = 0;
    
    // Create and position temperature meter
    createTemperatureMeter(meterSize, startX);
    lv_obj_set_pos(lv_obj_get_parent(tempMeter), startX, startY + (height - meterSize) / 2);
    
    // Create and position speed meter
    createSpeedMeter(meterSize, startX + meterSize + margin, startY + (height - meterSize) / 2);
}

void DashboardScreen::createMainScreen() {
    screen = lv_obj_create(NULL);
    lv_obj_set_scrollbar_mode(screen, LV_SCROLLBAR_MODE_OFF);
    
    // Match boot screen's gradient background
    lv_obj_set_style_bg_color(screen, lv_color_hex(0x101020), LV_STATE_DEFAULT);
    lv_obj_set_style_bg_grad_color(screen, lv_color_hex(0x202040), LV_STATE_DEFAULT);
    lv_obj_set_style_bg_grad_dir(screen, LV_GRAD_DIR_VER, LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(screen, LV_OPA_COVER, LV_STATE_DEFAULT);
}

void DashboardScreen::createTemperatureMeter(uint16_t size, uint16_t xPos) {
    // Create container for meter
    lv_obj_t* meter_container = lv_obj_create(screen);
    lv_obj_set_size(meter_container, size * 1.1, size * 1.1);
    lv_obj_set_style_bg_opa(meter_container, LV_OPA_0, LV_STATE_DEFAULT);  // Transparent background
    lv_obj_set_style_border_width(meter_container, 0, LV_STATE_DEFAULT);    // No border
    lv_obj_set_style_radius(meter_container, size * 0.55, LV_STATE_DEFAULT);
    lv_obj_set_style_pad_all(meter_container, size * 0.05, LV_STATE_DEFAULT);
    
    lv_obj_set_pos(meter_container, xPos - size * 0.05, (displayHeight - size * 1.1) / 2);
    
    // Create meter instead of arc
    tempMeter = lv_meter_create(meter_container);
    lv_obj_set_size(tempMeter, size, size);
    lv_obj_center(tempMeter);
    lv_obj_set_style_bg_opa(tempMeter, LV_OPA_0, LV_PART_MAIN);
    lv_obj_set_user_data(meter_container, this);

    // Remove the circle from the middle
    lv_obj_remove_style(tempMeter, NULL, LV_PART_INDICATOR);
    lv_obj_remove_style(tempMeter, NULL, LV_PART_MAIN);
    lv_obj_set_style_pad_all(tempMeter, 0, LV_PART_MAIN);

    // Add a scale
    lv_meter_scale_t* scale = lv_meter_add_scale(tempMeter);
    
    // Configure scale
    lv_meter_set_scale_ticks(tempMeter, scale, 41, 2, 10, lv_color_hex(DisplayColors::BORDER));
    lv_meter_set_scale_major_ticks(tempMeter, scale, 8, 4, 15, lv_color_hex(DisplayColors::TEXT_PRIMARY), 10);
    lv_meter_set_scale_range(tempMeter, scale, 0, 100, 270, 135); // Match original arc's angle range

    // Add indicators
    // Background arc (gray) Don't know yet if I want it.
    // lv_meter_indicator_t* indic_bg = lv_meter_add_arc(tempMeter, scale, size * 0.1, lv_color_hex(DisplayColors::BORDER), 0);
    // lv_meter_set_indicator_start_value(tempMeter, indic_bg, 0);
    // lv_meter_set_indicator_end_value(tempMeter, indic_bg, 100);

    // Main temperature indicator arc
    temperatureIndicator = lv_meter_add_arc(tempMeter, scale, size * 0.1, lv_color_hex(DisplayColors::SUCCESS), 0);
    lv_meter_set_indicator_start_value(tempMeter, temperatureIndicator, 0);
    lv_meter_set_indicator_end_value(tempMeter, temperatureIndicator, 0);

    // Create temperature label
    tempLabel = lv_label_create(tempMeter);
    lv_obj_center(tempLabel);
    lv_obj_set_style_text_font(tempLabel, &lv_font_montserrat_16, LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(tempLabel, lv_color_white(), LV_STATE_DEFAULT);
    lv_label_set_text(tempLabel, "0.0°C");
}

void DashboardScreen::createSpeedMeter(uint16_t size, uint16_t xPos, uint16_t yPos) {
    // Create container for speed meter
    lv_obj_t* speed_container = lv_obj_create(screen);
    lv_obj_set_size(speed_container, size * 1.1, size * 1.1);
    lv_obj_set_style_bg_opa(speed_container, LV_OPA_0, LV_STATE_DEFAULT);
    lv_obj_set_style_border_width(speed_container, 0, LV_STATE_DEFAULT);
    lv_obj_set_style_radius(speed_container, size * 0.55, LV_STATE_DEFAULT);
    lv_obj_set_style_pad_all(speed_container, size * 0.05, LV_STATE_DEFAULT);
    lv_obj_set_pos(speed_container, xPos - size * 0.05, yPos);
    
    // Create speed meter
    speedMeter = lv_meter_create(speed_container);
    lv_obj_set_size(speedMeter, size, size);
    lv_obj_center(speedMeter);
    lv_obj_set_style_bg_opa(speedMeter, LV_OPA_0, LV_PART_MAIN);
    lv_obj_set_user_data(speed_container, this);

    // Remove default indicator circle
    lv_obj_remove_style(speedMeter, NULL, LV_PART_INDICATOR);
    lv_obj_remove_style(speedMeter, NULL, LV_PART_MAIN);
    lv_obj_set_style_pad_all(speedMeter, 0, LV_PART_MAIN);

    // Add scale for speed
    lv_meter_scale_t* scale = lv_meter_add_scale(speedMeter);
    lv_meter_set_scale_ticks(speedMeter, scale, 41, 2, 10, lv_color_hex(DisplayColors::BORDER));
    lv_meter_set_scale_major_ticks(speedMeter, scale, 8, 4, 15, lv_color_hex(DisplayColors::TEXT_PRIMARY), 10);
    lv_meter_set_scale_range(speedMeter, scale, 0, 100, 270, 180);
    
    // Add target speed arc
    targetSpeedIndicator = lv_meter_add_arc(speedMeter, scale, size * 0.05, lv_color_hex(DisplayColors::TARGET_SPEED), 0);
    lv_meter_set_indicator_start_value(speedMeter, targetSpeedIndicator, 0);
    lv_meter_set_indicator_end_value(speedMeter, targetSpeedIndicator, 100);

    // Add current speed arc
    currentSpeedIndicator = lv_meter_add_arc(speedMeter, scale, size * 0.05, lv_color_hex(DisplayColors::CURRENT_SPEED), -size * 0.05);
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
    lv_obj_align(modeIndicator, LV_ALIGN_CENTER, 0, size/3);
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

    int targetValue = constrain((int)(temp * 2), 0, 100);
    
    if (!tempAnimationInProgress && targetValue != currentTempValue) {
        lv_anim_t anim;
        lv_anim_init(&anim);
        lv_anim_set_var(&anim, tempMeter);
        lv_anim_set_values(&anim, currentTempValue, targetValue);
        lv_anim_set_time(&anim, 2000);
        lv_anim_set_path_cb(&anim, lv_anim_path_ease_out);
        lv_anim_set_exec_cb(&anim, set_temp_value);
        lv_anim_set_ready_cb(&anim, [](lv_anim_t* a) {
            ((DashboardScreen*)lv_obj_get_user_data(lv_obj_get_parent((lv_obj_t*)a->var)))->tempAnimationInProgress = false;
        });
        
        tempAnimationInProgress = true;
        lv_anim_start(&anim);
        currentTempValue = targetValue;
    }

    char tempStr[32];
    snprintf(tempStr, sizeof(tempStr), "%.1f°C", temp);
    lv_label_set_text(tempLabel, tempStr);
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
        lv_anim_set_time(&anim, 2000);
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
        lv_anim_set_values(&anim, targetSpeedValue, fanSpeed);
        lv_anim_set_time(&anim, 2000);
        lv_anim_set_path_cb(&anim, lv_anim_path_ease_out);
        lv_anim_set_exec_cb(&anim, set_target_speed_value);
        lv_anim_set_ready_cb(&anim, [](lv_anim_t* a) {
            ((DashboardScreen*)lv_obj_get_user_data(lv_obj_get_parent((lv_obj_t*)a->var)))->targetSpeedAnimationInProgress = false;
        });
        
        currentSpeedAnimationInProgress = true;
        lv_anim_start(&anim);
        targetSpeedValue = targetSpeed;
    }

    // Update the speed label
    char speedStr[32];
    snprintf(speedStr, sizeof(speedStr), "%d%%", fanSpeed);
    lv_label_set_text(speedLabel, speedStr);
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
 * Animation Management
 ******************************************************************************/

