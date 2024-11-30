#include "dashboard_screen.h"

/**
 * @brief Constructor - Initializes all UI elements to nullptr
 */
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
    , initialized(false)
    , animationInProgress(false) {
    uiMutex = xSemaphoreCreateMutex();

}

DashboardScreen::~DashboardScreen() {
    if (uiMutex != nullptr) {
        vSemaphoreDelete(uiMutex);
    }
}

/**
 * @brief Initializes the UI if not already initialized
 */
void DashboardScreen::begin() {
    if (initialized) return;
    createUI();
    initialized = true;
}

/**
 * @brief Creates and configures all UI elements
 */
void DashboardScreen::createUI() {
    createMainScreen();
    
    // Calculate base dimensions and spacing
    uint16_t margin = displayWidth * 0.04;  // 4% margin  

    // Calculate widths for all elements
    uint16_t leftContainerWidth = displayWidth * 0.1;  // 10% of width
    uint16_t arcSize = displayWidth * 0.35;
    uint16_t rightContainerWidth = displayWidth * 0.4;  // 35% of width
    uint16_t containerHeight = arcSize;

    // Calculate horizontal positions
    uint16_t leftContainerX = margin;
    uint16_t arcX = leftContainerX + leftContainerWidth + margin;
    uint16_t rightContainerX = arcX + arcSize + margin;
    
    // Verify layout fits within display
    uint16_t totalWidth = leftContainerWidth + arcSize + rightContainerWidth + (margin * 4);
    if (totalWidth > displayWidth) {
        // Adjust sizes proportionally if they don't fit
        float scaleFactor = (float)(displayWidth - (margin * 4)) / (float)(totalWidth - (margin * 4));
        leftContainerWidth *= scaleFactor;
        arcSize *= scaleFactor;
        rightContainerWidth *= scaleFactor;
        
        // Recalculate positions
        leftContainerX = margin;
        arcX = leftContainerX + leftContainerWidth + margin;
        rightContainerX = arcX + arcSize + margin;
    }
    
    // Create elements with calculated dimensions and positions
    createLeftContainer(leftContainerWidth, containerHeight, leftContainerX);
    createTemperatureArc(arcSize, arcX);
    createRightContainer(rightContainerWidth, containerHeight, rightContainerX);
    createStatusIndicators();
    
    lv_scr_load(screen);
}


/**
 * @brief Updates all UI elements with current system state
 */
void DashboardScreen::update(float temp, int fanSpeed, int targetSpeed, FanController::Mode mode,
                      bool wifiConnected, bool mqttConnected, bool nightModeEnabled, bool nightModeActive) {
    if (!initialized) return;

    updateTemperatureDisplay(temp);
    updateStatusIndicators(wifiConnected, mqttConnected, nightModeEnabled, nightModeActive);
    updateSpeedDisplay(fanSpeed, targetSpeed);
    updateModeDisplay(mode);
}

/**
 * @brief Returns pointer to the temperature arc widget
 */
lv_obj_t* DashboardScreen::getArc() {
    return arc;
}

// Private helper methods

void DashboardScreen::createMainScreen() {
    screen = lv_obj_create(NULL);
    lv_obj_set_scrollbar_mode(screen, LV_SCROLLBAR_MODE_OFF);
    lv_obj_set_style_bg_color(screen, lv_color_black(), LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(screen, LV_OPA_COVER, LV_STATE_DEFAULT);
}

void DashboardScreen::createLeftContainer(uint16_t width, uint16_t height, uint16_t xPos) {
    left_container = lv_obj_create(screen);
    
    lv_obj_set_size(left_container, width, height);
    lv_obj_set_style_bg_color(left_container, lv_color_hex(0x101010), LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(left_container, LV_OPA_COVER, LV_STATE_DEFAULT);
    lv_obj_set_style_pad_all(left_container, width * 0.1, LV_STATE_DEFAULT);
    lv_obj_set_style_border_width(left_container, 1, LV_STATE_DEFAULT);
    lv_obj_set_style_border_color(left_container, lv_color_hex(0x404040), LV_STATE_DEFAULT);
    lv_obj_set_style_radius(left_container, 8, LV_STATE_DEFAULT);
    
    // Position from left instead of using alignment
    lv_obj_set_pos(left_container, xPos, (displayHeight - height) / 2);
}

void DashboardScreen::createTemperatureArc(uint16_t size, uint16_t xPos) {
    arc = lv_arc_create(screen);
    lv_obj_set_size(arc, size, size);
    lv_obj_set_user_data(screen, this);  // Store screen reference

    // Position arc explicitly instead of centering
    lv_obj_set_pos(arc, xPos, (displayHeight - size) / 2);
    
    // Rest of arc configuration remains the same
    lv_arc_set_rotation(arc, 135);
    lv_arc_set_bg_angles(arc, 0, 270);
    lv_arc_set_range(arc, 0, 100);
    
    uint16_t arcWidth = size * 0.1;
    lv_obj_set_style_arc_width(arc, arcWidth, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_arc_width(arc, arcWidth, LV_PART_INDICATOR | LV_STATE_DEFAULT);
    lv_obj_set_style_arc_color(arc, lv_color_hex(0x202040), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_arc_color(arc, lv_color_hex(0x00FF00), LV_PART_INDICATOR | LV_STATE_DEFAULT);

    tempLabel = lv_label_create(arc);
    lv_obj_center(tempLabel);
    lv_obj_set_style_text_color(tempLabel, lv_color_white(), LV_STATE_DEFAULT);
    lv_label_set_text(tempLabel, "0.0°C");
}

void DashboardScreen::createRightContainer(uint16_t width, uint16_t height, uint16_t xPos) {
    lv_obj_t* right_container = lv_obj_create(screen);
    
    lv_obj_set_size(right_container, width, height);
    lv_obj_set_style_bg_color(right_container, lv_color_hex(0x101010), LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(right_container, LV_OPA_COVER, LV_STATE_DEFAULT);
    lv_obj_set_style_pad_all(right_container, width * 0.08, LV_STATE_DEFAULT);
    lv_obj_set_style_border_width(right_container, 1, LV_STATE_DEFAULT);
    lv_obj_set_style_border_color(right_container, lv_color_hex(0x404040), LV_STATE_DEFAULT);
    lv_obj_set_style_radius(right_container, 8, LV_STATE_DEFAULT);
    
    // Position from left instead of using alignment
    lv_obj_set_pos(right_container, xPos, (displayHeight - height) / 2);

    // Calculate vertical spacing for labels
    uint16_t labelSpacing = height * 0.20;
    uint16_t topMargin = height * 0.10;
    
    currentSpeedLabel = createInfoLabel(right_container, LV_ALIGN_TOP_LEFT, 0, topMargin, "Current: 0%");
    targetSpeedLabel = createInfoLabel(right_container, LV_ALIGN_TOP_LEFT, 0, topMargin + labelSpacing, "Target: 0%");
    modeLabel = createInfoLabel(right_container, LV_ALIGN_TOP_LEFT, 0, topMargin + labelSpacing * 2, "Mode: Auto");
}

void DashboardScreen::createStatusIndicators() {
    uint16_t containerHeight = lv_obj_get_height(left_container);
    uint16_t spacing = 15;
    
    wifiLabel = createStatusLabel(left_container, LV_ALIGN_TOP_MID, 0, spacing, LV_SYMBOL_WIFI);
    nightLabel = createStatusLabel(left_container, LV_ALIGN_CENTER, 0, 0, "N");
    mqttLabel = createStatusLabel(left_container, LV_ALIGN_BOTTOM_MID, 0, -spacing, "M");
}

lv_obj_t* DashboardScreen::createStatusLabel(lv_obj_t* parent, lv_align_t align, lv_coord_t x_ofs, lv_coord_t y_ofs, const char* text) {
    lv_obj_t* label = lv_label_create(parent);
    lv_obj_align(label, align, x_ofs, y_ofs);
    lv_obj_set_style_text_font(label, &lv_font_montserrat_14, LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(label, lv_color_white(), LV_STATE_DEFAULT);
    lv_label_set_text(label, text);
    return label;
}

lv_obj_t* DashboardScreen::createInfoLabel(lv_obj_t* parent, lv_align_t align, lv_coord_t x_ofs, lv_coord_t y_ofs, const char* text) {
    lv_obj_t* label = lv_label_create(parent);
    lv_obj_align(label, align, x_ofs, y_ofs);
    lv_obj_set_style_text_font(label, &lv_font_montserrat_14, LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(label, lv_color_white(), LV_STATE_DEFAULT);
    lv_label_set_text(label, text);
    return label;
}

void DashboardScreen::updateTemperatureDisplay(float temp) {
    MutexGuard guard(uiMutex, pdMS_TO_TICKS(10));
    if (!guard.isLocked()) return;

    int targetValue = constrain((int)(temp * 2), 0, 100);
    
    // Only start new animation if we're not already animating and value changed
    if (!animationInProgress && targetValue != currentArcValue) {
        // Create smooth animation
        lv_anim_t anim;
        lv_anim_init(&anim);
        lv_anim_set_var(&anim, arc);
        lv_anim_set_exec_cb(&anim, arcAnimCallback);
        lv_anim_set_values(&anim, currentArcValue, targetValue);
        lv_anim_set_time(&anim, 1000);  // 1 second - faster than before
        lv_anim_set_path_cb(&anim, lv_anim_path_ease_out); // Smooth easing
        
        // Add a ready callback to know when animation is done
        lv_anim_set_ready_cb(&anim, [](lv_anim_t* a) {
            auto screen = static_cast<DashboardScreen*>(lv_obj_get_user_data(lv_obj_get_parent(static_cast<lv_obj_t*>(a->var))));
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

    // Update arc color based on temperature
    lv_color_t tempColor = (temp < 30.0f) ? lv_color_hex(0x00FF00) :
                          (temp < 40.0f) ? lv_color_hex(0xFFA500) :
                                         lv_color_hex(0xFF0000);
    lv_obj_set_style_arc_color(arc, tempColor, LV_PART_INDICATOR | LV_STATE_DEFAULT);
}

void DashboardScreen::arcAnimCallback(void* var, int32_t value) {
    lv_arc_set_value((lv_obj_t*)var, value);
}

void DashboardScreen::updateStatusIndicators(bool wifiConnected, bool mqttConnected, bool nightModeEnabled, bool nightModeActive) {
    MutexGuard guard(uiMutex, pdMS_TO_TICKS(10));
    if (!guard.isLocked()) return;

    // Update WiFi status
    lv_label_set_text(wifiLabel, wifiConnected ? LV_SYMBOL_WIFI : LV_SYMBOL_CLOSE);
    lv_obj_set_style_text_color(wifiLabel, 
        wifiConnected ? lv_color_hex(0x00FF00) : lv_color_hex(0xFF0000), 
        LV_STATE_DEFAULT);

    // Update night mode status
    lv_color_t nightColor;
    if (nightModeActive) {
        nightColor = lv_color_hex(0x00FF00);  // Green
    } else if (nightModeEnabled) {
        nightColor = lv_color_hex(0x8080FF);  // Light blue
    } else {
        nightColor = lv_color_hex(0x404040);  // Dark blue
    }
    lv_obj_set_style_text_color(nightLabel, nightColor, LV_STATE_DEFAULT);

    // Update MQTT status
    lv_label_set_text(mqttLabel, mqttConnected ? "M" : "×");
    lv_obj_set_style_text_color(mqttLabel,
        mqttConnected ? lv_color_hex(0x00FF00) : lv_color_hex(0xFF0000),
        LV_STATE_DEFAULT);
}

void DashboardScreen::updateSpeedDisplay(int fanSpeed, int targetSpeed) {
    MutexGuard guard(uiMutex, pdMS_TO_TICKS(10));
    if (!guard.isLocked()) return;

    // Update speed labels
    char currentStr[32], targetStr[32];
    snprintf(currentStr, sizeof(currentStr), "Current: %d%%", fanSpeed);
    snprintf(targetStr, sizeof(targetStr), "Target: %d%%", targetSpeed);
    
    lv_label_set_text(currentSpeedLabel, currentStr);
    lv_label_set_text(targetSpeedLabel, targetStr);

    // Update color based on speed difference
    lv_color_t speedColor = (abs(targetSpeed - fanSpeed) <= 5) ?
                           lv_color_hex(0x00FF00) : 
                           lv_color_hex(0xFFA500);
    
    lv_obj_set_style_text_color(currentSpeedLabel, speedColor, LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(targetSpeedLabel, speedColor, LV_STATE_DEFAULT);
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
        mode == FanController::Mode::AUTO ? lv_color_hex(0x00FF00) : lv_color_hex(0xFFA500),
        LV_STATE_DEFAULT);
}