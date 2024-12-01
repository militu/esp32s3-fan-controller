#include "dashboard_screen.h"
#include "display_colors.h"

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
    , animationInProgress(false)
    , currentArcValue(0) {
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
    uint16_t margin = displayWidth * 0.04;
    uint16_t topBarHeight = displayHeight * 0.20;
    uint16_t delimiterHeight = displayHeight * 0.002;
    
    // Create top status bar
    createTopStatusBar(topBarHeight);
    
    // Create delimiter
    createDelimiter(topBarHeight, delimiterHeight);
    
    // Calculate main content area dimensions
    uint16_t contentStartY = topBarHeight + delimiterHeight + margin;
    uint16_t contentHeight = displayHeight - contentStartY - margin;
    
    // Create main content
    createMainContent(contentStartY, contentHeight);
    
    lv_scr_load(screen);
}

void DashboardScreen::createTopStatusBar(uint16_t height) {
    // Create container for status indicators
    lv_obj_t* topBar = lv_obj_create(screen);
    lv_obj_set_size(topBar, displayWidth, height);
    lv_obj_set_pos(topBar, 0, 0);
    
    // Make container fully transparent
    lv_obj_set_style_bg_opa(topBar, LV_OPA_0, LV_STATE_DEFAULT);
    lv_obj_set_style_border_width(topBar, 0, LV_STATE_DEFAULT);
    
    // Calculate margins and spacing
    uint16_t sideMargin = displayWidth * 0.02;
    uint16_t iconSpacing = displayWidth * 0.15;

    // Create status indicators with larger font
    const lv_font_t* iconFont = &lv_font_montserrat_24;

    // Left side status indicators
    wifiLabel = createStatusLabel(topBar, LV_ALIGN_LEFT_MID, sideMargin, 0, LV_SYMBOL_WIFI);
    mqttLabel = createStatusLabel(topBar, LV_ALIGN_LEFT_MID, sideMargin + iconSpacing, 0, MY_TOWER_BROADCAST);
    
    // Right side indicator
    nightLabel = createStatusLabel(topBar, LV_ALIGN_RIGHT_MID, -sideMargin, 0, MY_MOON_SYMBOL);

    // Set fonts
    lv_obj_set_style_text_font(wifiLabel, iconFont, LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(mqttLabel, &fa_tower_broadcast, LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(nightLabel, &fa_moon, LV_STATE_DEFAULT);
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
    uint16_t margin = displayWidth * 0.06;  // Increased margin
    uint16_t arcSize = min(displayWidth * 0.45, height * 0.9);  // Reduced arc size
    uint16_t infoWidth = displayWidth * 0.38;  // Increased info width
    
    // Center the entire content group
    uint16_t totalWidth = arcSize + margin * 1.5 + infoWidth;  // Increased spacing
    uint16_t startX = (displayWidth - totalWidth) / 2;
    
    // Create and position temperature arc
    createTemperatureArc(arcSize, startX);
    lv_obj_set_pos(lv_obj_get_parent(arc), startX, startY + (height - arcSize) / 2);
    
    // Position right container with adjusted size
    uint16_t rightX = startX + arcSize + margin * 1.5;
    uint16_t containerHeight = arcSize * 0.85;  // Container height 85% of arc
    uint16_t containerY = startY + (height - containerHeight) / 2;
    createRightContainer(infoWidth, containerHeight, rightX, containerY);
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
    // Create container for arc
    lv_obj_t* arc_container = lv_obj_create(screen);
    lv_obj_set_size(arc_container, size * 1.1, size * 1.1);
    lv_obj_set_style_bg_opa(arc_container, LV_OPA_0, LV_STATE_DEFAULT);  // Transparent background
    lv_obj_set_style_border_width(arc_container, 0, LV_STATE_DEFAULT);    // No border for arc container
    lv_obj_set_style_radius(arc_container, size * 0.55, LV_STATE_DEFAULT);
    lv_obj_set_style_pad_all(arc_container, size * 0.05, LV_STATE_DEFAULT);
    
    lv_obj_set_pos(arc_container, xPos - size * 0.05, (displayHeight - size * 1.1) / 2);
    
    arc = lv_arc_create(arc_container);
    lv_obj_set_size(arc, size, size);
    lv_obj_center(arc);
    lv_obj_set_user_data(arc_container, this);

    lv_arc_set_rotation(arc, 135);
    lv_arc_set_bg_angles(arc, 0, 270);
    lv_arc_set_range(arc, 0, 100);
    lv_arc_set_value(arc, 0);

    uint16_t arcWidth = size * 0.1;
    lv_obj_set_style_arc_width(arc, arcWidth, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_arc_width(arc, arcWidth, LV_PART_INDICATOR | LV_STATE_DEFAULT);
    lv_obj_set_style_arc_color(arc, lv_color_hex(DisplayColors::BORDER), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_arc_color(arc, lv_color_hex(DisplayColors::SUCCESS), LV_PART_INDICATOR | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(arc, lv_color_hex(DisplayColors::KNOB), LV_PART_KNOB | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(arc, LV_OPA_COVER, LV_PART_KNOB | LV_STATE_DEFAULT);

    tempLabel = lv_label_create(arc);
    lv_obj_center(tempLabel);
    lv_obj_set_style_text_font(tempLabel, &lv_font_montserrat_16, LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(tempLabel, lv_color_white(), LV_STATE_DEFAULT);
    lv_label_set_text(tempLabel, "0.0°C");
}

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
    lv_color_t tempColor = (temp < 30.0f) ? lv_color_hex(DisplayColors::TEMP_GOOD) :
                          (temp < 40.0f) ? lv_color_hex(DisplayColors::TEMP_WARNING) :
                                         lv_color_hex(DisplayColors::TEMP_CRITICAL);
    lv_obj_set_style_arc_color(arc, tempColor, LV_PART_INDICATOR | LV_STATE_DEFAULT);
}

void DashboardScreen::arcAnimCallback(void* var, int32_t value) {
    lv_arc_set_value((lv_obj_t*)var, value);
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

    // Update speed labels
    char currentStr[32], targetStr[32];
    snprintf(currentStr, sizeof(currentStr), "Current: %d%%", fanSpeed);
    snprintf(targetStr, sizeof(targetStr), "Target: %d%%", targetSpeed);
    
    lv_label_set_text(currentSpeedLabel, currentStr);
    lv_label_set_text(targetSpeedLabel, targetStr);

    // Update color based on speed difference
    lv_color_t speedColor = (abs(targetSpeed - fanSpeed) <= 5) ?
                        lv_color_hex(DisplayColors::SUCCESS) : 
                        lv_color_hex(DisplayColors::TEMP_WARNING);
    
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
        mode == FanController::Mode::AUTO ? lv_color_hex(DisplayColors::SUCCESS) : lv_color_hex(DisplayColors::TEMP_WARNING),
        LV_STATE_DEFAULT);
}