#include "display_ui.h"

/**
 * @brief Constructor - Initializes all UI elements to nullptr
 */
DisplayUI::DisplayUI()
    : screen(nullptr)
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
    createLeftContainer();
    createTemperatureArc();
    createStatusIndicators();
    createRightContainer();
    
    lv_scr_load(screen);
}

/**
 * @brief Updates all UI elements with current system state
 * 
 * @param temp Current temperature
 * @param fanSpeed Current fan speed percentage
 * @param targetSpeed Target fan speed percentage
 * @param mode Current operation mode
 * @param wifiConnected WiFi connection status
 * @param mqttConnected MQTT connection status
 * @param nightMode Night mode status
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

void DisplayUI::createLeftContainer() {
    left_container = lv_obj_create(screen);
    
    // Set container properties
    lv_obj_set_size(left_container, 40, 140);
    lv_obj_set_style_bg_color(left_container, lv_color_hex(0x101010), LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(left_container, LV_OPA_COVER, LV_STATE_DEFAULT);
    lv_obj_set_style_pad_all(left_container, 5, LV_STATE_DEFAULT);
    lv_obj_set_style_border_width(left_container, 1, LV_STATE_DEFAULT);
    lv_obj_set_style_border_color(left_container, lv_color_hex(0x404040), LV_STATE_DEFAULT);
    lv_obj_set_style_radius(left_container, 8, LV_STATE_DEFAULT);
    lv_obj_align(left_container, LV_ALIGN_LEFT_MID, 5, 0);
}

void DisplayUI::createTemperatureArc() {
    // Create and position arc
    arc = lv_arc_create(screen);
    lv_obj_set_size(arc, 120, 120);
    lv_obj_center(arc);
    
    // Configure arc properties
    lv_arc_set_rotation(arc, 135);
    lv_arc_set_bg_angles(arc, 0, 270);
    lv_arc_set_range(arc, 0, 100);
    
    // Set arc styles
    lv_obj_set_style_arc_width(arc, 12, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_arc_width(arc, 12, LV_PART_INDICATOR | LV_STATE_DEFAULT);
    lv_obj_set_style_arc_color(arc, lv_color_hex(0x202040), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_arc_color(arc, lv_color_hex(0x00FF00), LV_PART_INDICATOR | LV_STATE_DEFAULT);

    // Create temperature label
    tempLabel = lv_label_create(arc);
    lv_obj_center(tempLabel);
    lv_obj_set_style_text_color(tempLabel, lv_color_white(), LV_STATE_DEFAULT);
    lv_label_set_text(tempLabel, "0.0°C");
}

/**
 * @brief Creates and configures all status indicator labels
 */
void DisplayUI::createStatusIndicators() {
    // Create status indicators within the left container
    wifiLabel = createStatusLabel(left_container, LV_ALIGN_TOP_MID, 0, 10, LV_SYMBOL_WIFI);
    nightLabel = createStatusLabel(left_container, LV_ALIGN_CENTER, 0, 0, "N");
    mqttLabel = createStatusLabel(left_container, LV_ALIGN_BOTTOM_MID, 0, -10, "M");
}

void DisplayUI::createRightContainer() {
    lv_obj_t* right_container = lv_obj_create(screen);
    
    // Set container properties
    lv_obj_set_size(right_container, 100, 140);
    lv_obj_set_style_bg_color(right_container, lv_color_hex(0x101010), LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(right_container, LV_OPA_COVER, LV_STATE_DEFAULT);
    lv_obj_set_style_pad_all(right_container, 8, LV_STATE_DEFAULT);
    lv_obj_set_style_border_width(right_container, 1, LV_STATE_DEFAULT);
    lv_obj_set_style_border_color(right_container, lv_color_hex(0x404040), LV_STATE_DEFAULT);
    lv_obj_set_style_radius(right_container, 8, LV_STATE_DEFAULT);
    lv_obj_align(right_container, LV_ALIGN_RIGHT_MID, -5, 0);

    // Create speed and mode labels
    currentSpeedLabel = createInfoLabel(right_container, LV_ALIGN_TOP_LEFT, 0, 0, "Current: 0%");
    targetSpeedLabel = createInfoLabel(right_container, LV_ALIGN_TOP_LEFT, 0, 25, "Target: 0%");
    modeLabel = createInfoLabel(right_container, LV_ALIGN_TOP_LEFT, 0, 60, "Mode: Auto");
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
    // Update arc value
    int arcValue = constrain((int)(temp * 2), 0, 100);
    lv_arc_set_value(arc, arcValue);
    
    // Update temperature label
    char tempStr[32];
    snprintf(tempStr, sizeof(tempStr), "%.1f°C", temp);
    lv_label_set_text(tempLabel, tempStr);

    // Update arc color based on temperature ranges
    lv_color_t tempColor = (temp < 30.0f) ? lv_color_hex(0x00FF00) :
                          (temp < 40.0f) ? lv_color_hex(0xFFA500) :
                                         lv_color_hex(0xFF0000);
    lv_obj_set_style_arc_color(arc, tempColor, LV_PART_INDICATOR | LV_STATE_DEFAULT);
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