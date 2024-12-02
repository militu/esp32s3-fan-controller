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
    , arc(nullptr)
    , left_container(nullptr)
    , tempLabel(nullptr)
    , currentSpeedLabel(nullptr)
    , targetSpeedLabel(nullptr)
    , modeLabel(nullptr)
    , wifiLabel(nullptr)
    , mqttLabel(nullptr)
    , nightLabel(nullptr)
    , speedMeter(nullptr)
    , speedLabel(nullptr)
    , currentSpeedIndicator(nullptr)
    , targetSpeedIndicator(nullptr)
    , initialized(false)
    , animationInProgress(false)
    , speedAnimationInProgress(false)
    , currentArcValue(0)
    , currentTargetSpeed(0) {
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
    animationInProgress = false;
    speedAnimationInProgress = false;
    currentArcValue = 0;
    currentTargetSpeed = 0;
    
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
    // updateModeDisplay(mode);
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
    createTemperatureArc(meterSize, startX);
    lv_obj_set_pos(lv_obj_get_parent(arc), startX, startY + (height - meterSize) / 2);
    
    // Create and position speed meter
    createSpeedMeter(meterSize, startX + meterSize + margin, startY + (height - meterSize) / 2);
}

void DashboardScreen::createRightContainer(uint16_t width, uint16_t height, uint16_t xPos, uint16_t yPos) {
    lv_obj_t* right_container = lv_obj_create(screen);
    
    lv_obj_set_size(right_container, width, height);
    lv_obj_set_style_bg_opa(right_container, LV_OPA_0, LV_STATE_DEFAULT);
    lv_obj_set_style_border_width(right_container, 1, LV_STATE_DEFAULT);
    lv_obj_set_style_border_color(right_container, lv_color_hex(DisplayColors::BORDER), LV_STATE_DEFAULT);
    lv_obj_set_style_radius(right_container, height * 0.05, LV_STATE_DEFAULT);
    
    // Adjust padding based on container size
    uint16_t padding = height * 0.08;
    lv_obj_set_style_pad_all(right_container, padding, LV_STATE_DEFAULT);
    
    lv_obj_set_pos(right_container, xPos, yPos);

    // Calculate label positions for even distribution
    uint16_t availableHeight = height - (padding * 2);
    uint16_t labelSpacing = availableHeight / 4;  // Create 3 equal sections with spacing
    
    // Create labels with dynamic positioning
    currentSpeedLabel = createInfoLabel(right_container, LV_ALIGN_TOP_LEFT, 0, labelSpacing - padding, "Current: 0%");
    targetSpeedLabel = createInfoLabel(right_container, LV_ALIGN_TOP_LEFT, 0, labelSpacing * 2 - padding, "Target: 0%");
    modeLabel = createInfoLabel(right_container, LV_ALIGN_TOP_LEFT, 0, labelSpacing * 3 - padding, "Mode: Auto");
    
    // Dynamic font size based on container size
    const lv_font_t* labelFont = (height >= 140) ? &lv_font_montserrat_16 :
                                (height >= 100) ? &lv_font_montserrat_14 :
                                                 &lv_font_montserrat_12;
    
    lv_obj_set_style_text_font(currentSpeedLabel, labelFont, LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(targetSpeedLabel, labelFont, LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(modeLabel, labelFont, LV_STATE_DEFAULT);
}

lv_obj_t* DashboardScreen::getArc() {
    return arc;
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

void DashboardScreen::createLeftContainer(uint16_t width, uint16_t height, uint16_t xPos) {
    left_container = lv_obj_create(screen);
    
    lv_obj_set_size(left_container, width, height);
    lv_obj_set_style_bg_opa(left_container, LV_OPA_0, LV_STATE_DEFAULT);  // Transparent background
    lv_obj_set_style_pad_all(left_container, width * 0.1, LV_STATE_DEFAULT);
    lv_obj_set_style_border_width(left_container, 1, LV_STATE_DEFAULT);  // Thin border
    lv_obj_set_style_border_color(left_container, lv_color_hex(DisplayColors::BORDER), LV_STATE_DEFAULT);
    lv_obj_set_style_radius(left_container, displayWidth * 0.015, LV_STATE_DEFAULT);
    
    lv_obj_set_pos(left_container, xPos, (displayHeight - height) / 2);
}

void DashboardScreen::createTemperatureArc(uint16_t size, uint16_t xPos) {
    // Create container for meter
    lv_obj_t* meter_container = lv_obj_create(screen);
    lv_obj_set_size(meter_container, size * 1.1, size * 1.1);
    lv_obj_set_style_bg_opa(meter_container, LV_OPA_0, LV_STATE_DEFAULT);  // Transparent background
    lv_obj_set_style_border_width(meter_container, 0, LV_STATE_DEFAULT);    // No border
    lv_obj_set_style_radius(meter_container, size * 0.55, LV_STATE_DEFAULT);
    lv_obj_set_style_pad_all(meter_container, size * 0.05, LV_STATE_DEFAULT);
    
    lv_obj_set_pos(meter_container, xPos - size * 0.05, (displayHeight - size * 1.1) / 2);
    
    // Create meter instead of arc
    arc = lv_meter_create(meter_container);
    lv_obj_set_size(arc, size, size);
    lv_obj_center(arc);
    lv_obj_set_user_data(meter_container, this);

    // Remove the circle from the middle
    lv_obj_remove_style(arc, NULL, LV_PART_INDICATOR);
    lv_obj_set_style_pad_all(arc, 0, LV_PART_MAIN);

    // Add a scale
    lv_meter_scale_t* scale = lv_meter_add_scale(arc);
    
    // Configure scale
    lv_meter_set_scale_ticks(arc, scale, 41, 2, 10, lv_color_hex(DisplayColors::BORDER));
    lv_meter_set_scale_major_ticks(arc, scale, 8, 4, 15, lv_color_hex(DisplayColors::TEXT_PRIMARY), 10);
    lv_meter_set_scale_range(arc, scale, 0, 100, 270, 135); // Match original arc's angle range

    // Add indicators
    // Background arc (gray)
    lv_meter_indicator_t* indic_bg = lv_meter_add_arc(arc, scale, size * 0.1, lv_color_hex(DisplayColors::BORDER), 0);
    lv_meter_set_indicator_start_value(arc, indic_bg, 0);
    lv_meter_set_indicator_end_value(arc, indic_bg, 100);

    // Main temperature indicator arc
    temperatureIndicator = lv_meter_add_arc(arc, scale, size * 0.1, lv_color_hex(DisplayColors::SUCCESS), 0);
    lv_meter_set_indicator_start_value(arc, temperatureIndicator, 0);
    lv_meter_set_indicator_end_value(arc, temperatureIndicator, 0);

    // Create temperature label
    tempLabel = lv_label_create(arc);
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
    lv_obj_set_user_data(speed_container, this);

    // Remove default indicator circle
    lv_obj_remove_style(speedMeter, NULL, LV_PART_INDICATOR);
    lv_obj_set_style_pad_all(speedMeter, 0, LV_PART_MAIN);

    // Add scale for speed
    lv_meter_scale_t* scale = lv_meter_add_scale(speedMeter);
    lv_meter_set_scale_ticks(speedMeter, scale, 41, 2, 10, lv_color_hex(DisplayColors::BORDER));
    lv_meter_set_scale_major_ticks(speedMeter, scale, 8, 4, 15, lv_color_hex(DisplayColors::TEXT_PRIMARY), 10);
    lv_meter_set_scale_range(speedMeter, scale, 0, 100, 270, 135);

    // Add background arc
    lv_meter_indicator_t* indic_bg = lv_meter_add_arc(speedMeter, scale, size * 0.1, lv_color_hex(DisplayColors::BORDER), 0);
    lv_meter_set_indicator_start_value(speedMeter, indic_bg, 0);
    lv_meter_set_indicator_end_value(speedMeter, indic_bg, 100);

    // Add target speed arc
    targetSpeedIndicator = lv_meter_add_arc(speedMeter, scale, size * 0.1, lv_color_hex(DisplayColors::SUCCESS), 0);
    lv_meter_set_indicator_start_value(speedMeter, targetSpeedIndicator, 0);
    lv_meter_set_indicator_end_value(speedMeter, targetSpeedIndicator, 0);

    // Add current speed needle
    currentSpeedIndicator = lv_meter_add_needle_line(speedMeter, scale, 3, lv_color_hex(DisplayColors::TEXT_PRIMARY), -10);
    lv_meter_set_indicator_value(speedMeter, currentSpeedIndicator, 0);

    // Create speed label
    speedLabel = lv_label_create(speedMeter);
    lv_obj_center(speedLabel);
    lv_obj_set_style_text_font(speedLabel, &lv_font_montserrat_16, LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(speedLabel, lv_color_white(), LV_STATE_DEFAULT);
    lv_label_set_text(speedLabel, "0%");
}

void DashboardScreen::createStatusIndicators() {
    // Calculate positions within left container
    lv_coord_t containerHeight = lv_obj_get_height(left_container);
    
    // Get arc dimensions for reference
    lv_obj_t* arc_container = lv_obj_get_parent(arc);
    lv_coord_t arcHeight = lv_obj_get_height(arc_container);
    
    // Calculate spacing to match arc height
    uint16_t spacing = arcHeight / 3;  // Divide height into three equal sections
    uint16_t topOffset = (containerHeight - arcHeight) / 2;  // Align with arc top
    
    // Create and position labels
    wifiLabel = createStatusLabel(left_container, LV_ALIGN_TOP_MID, 0, 0, LV_SYMBOL_WIFI);
    nightLabel = createStatusLabel(left_container, LV_ALIGN_CENTER, 0, 0, "N");
    mqttLabel = createStatusLabel(left_container, LV_ALIGN_BOTTOM_MID, 0, 0, "M");

    // Force an initial update to ensure proper colors
    updateStatusIndicators(false, false, false, false);
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

lv_obj_t* DashboardScreen::createInfoLabel(lv_obj_t* parent, lv_align_t align, lv_coord_t x_ofs, lv_coord_t y_ofs, const char* text) {
    lv_obj_t* label = lv_label_create(parent);
    lv_obj_align(label, align, x_ofs, y_ofs);
    lv_obj_set_style_text_font(label, &lv_font_montserrat_14, LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(label, lv_color_hex(DisplayColors::TEXT_PRIMARY), LV_STATE_DEFAULT);
    lv_label_set_text(label, text);
    return label;
}

/*******************************************************************************
 * UI State Updates
 ******************************************************************************/

void DashboardScreen::updateTemperatureDisplay(float temp) {
    if (!initialized || arc == nullptr) return;

    MutexGuard guard(uiMutex, pdMS_TO_TICKS(10));
    if (!guard.isLocked()) return;

    int targetValue = constrain((int)(temp * 2), 0, 100);
    
    // Only start new animation if we're not already animating and value changed
    if (!animationInProgress && targetValue != currentArcValue) {
        // Create smooth animation for the meter
        lv_anim_t anim;
        lv_anim_init(&anim);
        lv_anim_set_var(&anim, arc);  // Use the meter object itself
        lv_anim_set_exec_cb(&anim, [](void* var, int32_t v) {
            lv_obj_t* meter = (lv_obj_t*)var;
            auto screen = static_cast<DashboardScreen*>(lv_obj_get_user_data(lv_obj_get_parent(meter)));
            lv_meter_set_indicator_end_value(meter, screen->temperatureIndicator, v);
        });
        lv_anim_set_values(&anim, currentArcValue, targetValue);
        lv_anim_set_time(&anim, 1000);
        lv_anim_set_path_cb(&anim, lv_anim_path_ease_out);
        
        // Add a ready callback to know when animation is done
        lv_anim_set_ready_cb(&anim, [](lv_anim_t* a) {
            lv_obj_t* meter = (lv_obj_t*)a->var;
            auto screen = static_cast<DashboardScreen*>(lv_obj_get_user_data(lv_obj_get_parent(meter)));
            screen->animationInProgress = false;
        });
        
        animationInProgress = true;
        lv_anim_start(&anim);
        currentArcValue = targetValue;
    }

    // Update temperature label
    char tempStr[32];
    snprintf(tempStr, sizeof(tempStr), "%.1f°C", temp);
    lv_label_set_text(tempLabel, tempStr);

    // Update meter color based on temperature
    lv_color_t tempColor = (temp < 30.0f) ? lv_color_hex(DisplayColors::TEMP_GOOD) :
                          (temp < 40.0f) ? lv_color_hex(DisplayColors::TEMP_WARNING) :
                                         lv_color_hex(DisplayColors::TEMP_CRITICAL);
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
    if (!initialized || speedMeter == nullptr || 
        currentSpeedIndicator == nullptr || 
        targetSpeedIndicator == nullptr || 
        speedLabel == nullptr) return;

    MutexGuard guard(uiMutex, pdMS_TO_TICKS(10));
    if (!guard.isLocked()) return;

    // Update target speed arc (animated)
    if (!speedAnimationInProgress && targetSpeed != currentTargetSpeed) {
        lv_anim_t anim;
        lv_anim_init(&anim);
        lv_anim_set_var(&anim, speedMeter);
        lv_anim_set_exec_cb(&anim, [](void* var, int32_t v) {
            lv_obj_t* meter = (lv_obj_t*)var;
            auto screen = static_cast<DashboardScreen*>(lv_obj_get_user_data(lv_obj_get_parent(meter)));
            if (screen && screen->targetSpeedIndicator) {  // Add null check
                lv_meter_set_indicator_end_value(meter, screen->targetSpeedIndicator, v);
            }
        });
        // ... rest of the animation setup ...
    }

    // Update current speed needle (direct, no animation)
    if (currentSpeedIndicator) {  // Add null check
        lv_meter_set_indicator_value(speedMeter, currentSpeedIndicator, fanSpeed);
    }

    // Update the speed label
    char speedStr[32];
    snprintf(speedStr, sizeof(speedStr), "%d%%", fanSpeed);
    lv_label_set_text(speedLabel, speedStr);

    // Update colors based on speed difference
    lv_color_t speedColor = (abs(targetSpeed - fanSpeed) <= 5) ?
                        lv_color_hex(DisplayColors::SUCCESS) : 
                        lv_color_hex(DisplayColors::TEMP_WARNING);
}

void DashboardScreen::updateModeDisplay(FanController::Mode mode) {
    MutexGuard guard(uiMutex, pdMS_TO_TICKS(10));
    if (!guard.isLocked()) return;
    
    // Update mode label
    char modeStr[32];
    snprintf(modeStr, sizeof(modeStr), "Mode: %s", 
             mode == FanController::Mode::AUTO ? "Auto" : "Manual");
    lv_label_set_text(modeLabel, modeStr);
    
    // Update color based on mode
    lv_obj_set_style_text_color(modeLabel, 
        mode == FanController::Mode::AUTO ? lv_color_hex(DisplayColors::SUCCESS) : lv_color_hex(DisplayColors::TEMP_WARNING),
        LV_STATE_DEFAULT);
}

/*******************************************************************************
 * Animation Management
 ******************************************************************************/

void DashboardScreen::arcAnimCallback(void* var, int32_t value) {
    lv_arc_set_value((lv_obj_t*)var, value);
}