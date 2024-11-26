#include "display_ui.h"

/**
 * @brief Constructor - Initializes all UI elements to nullptr
 */
DisplayUI::DisplayUI()
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
    , initialized(false) {
}

/**
 * @brief Initializes the UI if not already initialized
 */
void DisplayUI::begin() {
    if (initialized) return;
    createUI();
    initialized = true;
}

/**
 * @brief Creates and configures all UI elements
 */
void DisplayUI::createUI() {
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
void DisplayUI::update(float temp, int fanSpeed, int targetSpeed, FanController::Mode mode,
                      bool wifiConnected, bool mqttConnected, bool nightMode) {
    if (!initialized) return;

    updateTemperatureDisplay(temp);
    updateStatusIndicators(wifiConnected, mqttConnected, nightMode);
    updateSpeedDisplay(fanSpeed, targetSpeed);
    updateModeDisplay(mode, nightMode);
}

/**
 * @brief Returns pointer to the temperature arc widget
 */
lv_obj_t* DisplayUI::getArc() {
    return arc;
}

// Private helper methods

void DisplayUI::createMainScreen() {
    screen = lv_obj_create(NULL);
    lv_obj_set_scrollbar_mode(screen, LV_SCROLLBAR_MODE_OFF);
    lv_obj_set_style_bg_color(screen, lv_color_black(), LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(screen, LV_OPA_COVER, LV_STATE_DEFAULT);
}

void DisplayUI::createLeftContainer(uint16_t width, uint16_t height, uint16_t xPos) {
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

void DisplayUI::createTemperatureArc(uint16_t size, uint16_t xPos) {
    arc = lv_arc_create(screen);
    lv_obj_set_size(arc, size, size);
    
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

void DisplayUI::createRightContainer(uint16_t width, uint16_t height, uint16_t xPos) {
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

void DisplayUI::createStatusIndicators() {
    uint16_t containerHeight = lv_obj_get_height(left_container);
    uint16_t spacing = 15;
    
    wifiLabel = createStatusLabel(left_container, LV_ALIGN_TOP_MID, 0, spacing, LV_SYMBOL_WIFI);
    nightLabel = createStatusLabel(left_container, LV_ALIGN_CENTER, 0, 0, "N");
    mqttLabel = createStatusLabel(left_container, LV_ALIGN_BOTTOM_MID, 0, -spacing, "M");
}

lv_obj_t* DisplayUI::createStatusLabel(lv_obj_t* parent, lv_align_t align, lv_coord_t x_ofs, lv_coord_t y_ofs, const char* text) {
    lv_obj_t* label = lv_label_create(parent);
    lv_obj_align(label, align, x_ofs, y_ofs);
    lv_obj_set_style_text_font(label, &lv_font_montserrat_14, LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(label, lv_color_white(), LV_STATE_DEFAULT);
    lv_label_set_text(label, text);
    return label;
}

lv_obj_t* DisplayUI::createInfoLabel(lv_obj_t* parent, lv_align_t align, lv_coord_t x_ofs, lv_coord_t y_ofs, const char* text) {
    lv_obj_t* label = lv_label_create(parent);
    lv_obj_align(label, align, x_ofs, y_ofs);
    lv_obj_set_style_text_font(label, &lv_font_montserrat_14, LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(label, lv_color_white(), LV_STATE_DEFAULT);
    lv_label_set_text(label, text);
    return label;
}

void DisplayUI::updateTemperatureDisplay(float temp) {
    int targetValue = constrain((int)(temp * 2), 0, 100);
    
    // Create smooth animation
    lv_anim_init(&arcAnim);
    lv_anim_set_var(&arcAnim, arc);
    lv_anim_set_exec_cb(&arcAnim, arcAnimCallback);
    lv_anim_set_values(&arcAnim, currentArcValue, targetValue);
    lv_anim_set_time(&arcAnim, 2000);  // 2 seconds
    lv_anim_set_repeat_delay(&arcAnim, 500);  // 500ms delay between repeats
    lv_anim_set_path_cb(&arcAnim, lv_anim_path_ease_in_out);
    lv_anim_start(&arcAnim);
    
    currentArcValue = targetValue;

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

void DisplayUI::arcAnimCallback(void* var, int32_t value) {
    lv_arc_set_value((lv_obj_t*)var, value);
}

void DisplayUI::updateStatusIndicators(bool wifiConnected, bool mqttConnected, bool nightMode) {
    // Update WiFi status
    lv_label_set_text(wifiLabel, wifiConnected ? LV_SYMBOL_WIFI : LV_SYMBOL_CLOSE);
    lv_obj_set_style_text_color(wifiLabel, 
        wifiConnected ? lv_color_hex(0x00FF00) : lv_color_hex(0xFF0000), 
        LV_STATE_DEFAULT);

    // Update night mode status
    lv_obj_set_style_text_color(nightLabel, 
        nightMode ? lv_color_hex(0x8080FF) : lv_color_hex(0x404040),
        LV_STATE_DEFAULT);

    // Update MQTT status
    lv_label_set_text(mqttLabel, mqttConnected ? "M" : "×");
    lv_obj_set_style_text_color(mqttLabel,
        mqttConnected ? lv_color_hex(0x00FF00) : lv_color_hex(0xFF0000),
        LV_STATE_DEFAULT);
}

void DisplayUI::updateSpeedDisplay(int fanSpeed, int targetSpeed) {
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

void DisplayUI::updateModeDisplay(FanController::Mode mode, bool nightMode) {
    // Update mode label
    char modeStr[32];
    snprintf(modeStr, sizeof(modeStr), "Mode: %s%s", 
             mode == FanController::Mode::AUTO ? "Auto" : "Manual",
             nightMode ? " (N)" : "");
    lv_label_set_text(modeLabel, modeStr);
    
    // Update color based on mode
    lv_obj_set_style_text_color(modeLabel, 
        mode == FanController::Mode::AUTO ? lv_color_hex(0x00FF00) : lv_color_hex(0xFFA500),
        LV_STATE_DEFAULT);
}