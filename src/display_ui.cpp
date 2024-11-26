#include "display_ui.h"

/*******************************************************************************
 * Constants and Color Definitions
 ******************************************************************************/
namespace {
    // UI Colors
    constexpr lv_color_t BG_COLOR = lv_color_hex(0x101010);         // Dark background
    constexpr lv_color_t BORDER_COLOR = lv_color_hex(0x404040);     // Border gray
    constexpr lv_color_t SUCCESS_COLOR = lv_color_hex(0x00FF00);    // Green for OK status
    constexpr lv_color_t WARNING_COLOR = lv_color_hex(0xFFA500);    // Orange for warnings
    constexpr lv_color_t ERROR_COLOR = lv_color_hex(0xFF0000);      // Red for errors
    constexpr lv_color_t NIGHT_COLOR = lv_color_hex(0x8080FF);      // Blue for night mode
    constexpr lv_color_t INACTIVE_COLOR = lv_color_hex(0x404040);   // Gray for inactive

    // UI Dimensions
    constexpr uint8_t LEFT_CONTAINER_WIDTH = 40;
    constexpr uint8_t RIGHT_CONTAINER_WIDTH = 100;
    constexpr uint8_t CONTAINER_HEIGHT = 140;
    constexpr uint8_t ARC_SIZE = 120;
    constexpr uint8_t PADDING = 8;
    constexpr uint8_t BORDER_RADIUS = 8;
}

/*******************************************************************************
 * DisplayUI Implementation
 ******************************************************************************/
DisplayUI::DisplayUI()
    : screen(nullptr)
    , arc(nullptr)
    , tempLabel(nullptr)
    , currentSpeedLabel(nullptr)
    , targetSpeedLabel(nullptr)
    , modeLabel(nullptr)
    , wifiLabel(nullptr)
    , mqttLabel(nullptr)
    , nightLabel(nullptr)
    , initialized(false) {
}

void DisplayUI::begin() {
    if (initialized) return;
    createUI();
    initialized = true;
}

void DisplayUI::createUI() {
    // Initialize main screen
    screen = lv_obj_create(NULL);
    lv_obj_set_scrollbar_mode(screen, LV_SCROLLBAR_MODE_OFF);
    lv_obj_set_style_bg_color(screen, lv_color_black(), LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(screen, LV_OPA_COVER, LV_STATE_DEFAULT);

    createLeftContainer();
    createArc();
    createStatusIndicators();
    createRightContainer();
    
    lv_scr_load(screen);
}

void DisplayUI::createLeftContainer() {
    lv_obj_t* left_container = lv_obj_create(screen);
    lv_obj_set_size(left_container, LEFT_CONTAINER_WIDTH, CONTAINER_HEIGHT);
    lv_obj_set_style_bg_color(left_container, BG_COLOR, LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(left_container, LV_OPA_COVER, LV_STATE_DEFAULT);
    lv_obj_set_style_pad_all(left_container, 5, LV_STATE_DEFAULT);
    lv_obj_set_style_border_width(left_container, 1, LV_STATE_DEFAULT);
    lv_obj_set_style_border_color(left_container, BORDER_COLOR, LV_STATE_DEFAULT);
    lv_obj_set_style_radius(left_container, BORDER_RADIUS, LV_STATE_DEFAULT);
    lv_obj_align(left_container, LV_ALIGN_LEFT_MID, 5, 0);

    // Create status indicators
    wifiLabel = lv_label_create(left_container);
    lv_obj_align(wifiLabel, LV_ALIGN_TOP_MID, 0, 10);
    lv_obj_set_style_text_font(wifiLabel, &lv_font_montserrat_14, LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(wifiLabel, lv_color_white(), LV_STATE_DEFAULT);
    lv_label_set_text(wifiLabel, LV_SYMBOL_WIFI);

    nightLabel = lv_label_create(left_container);
    lv_obj_align(nightLabel, LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_style_text_font(nightLabel, &lv_font_montserrat_14, LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(nightLabel, lv_color_white(), LV_STATE_DEFAULT);
    lv_label_set_text(nightLabel, "N");

    mqttLabel = lv_label_create(left_container);
    lv_obj_align(mqttLabel, LV_ALIGN_BOTTOM_MID, 0, -10);
    lv_obj_set_style_text_font(mqttLabel, &lv_font_montserrat_14, LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(mqttLabel, lv_color_white(), LV_STATE_DEFAULT);
    lv_label_set_text(mqttLabel, "M");
}

void DisplayUI::createArc() {
    arc = lv_arc_create(screen);
    lv_obj_set_size(arc, ARC_SIZE, ARC_SIZE);
    lv_obj_center(arc);
    lv_arc_set_rotation(arc, 135);
    lv_arc_set_bg_angles(arc, 0, 270);
    lv_arc_set_range(arc, 0, 100);
    lv_obj_set_style_arc_width(arc, 12, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_arc_width(arc, 12, LV_PART_INDICATOR | LV_STATE_DEFAULT);
    lv_obj_set_style_arc_color(arc, lv_color_hex(0x202040), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_arc_color(arc, SUCCESS_COLOR, LV_PART_INDICATOR | LV_STATE_DEFAULT);

    tempLabel = lv_label_create(arc);
    lv_obj_center(tempLabel);
    lv_obj_set_style_text_color(tempLabel, lv_color_white(), LV_STATE_DEFAULT);
    lv_label_set_text(tempLabel, "0.0°C");
}

void DisplayUI::createRightContainer() {
    lv_obj_t* right_container = lv_obj_create(screen);
    lv_obj_set_size(right_container, RIGHT_CONTAINER_WIDTH, CONTAINER_HEIGHT);
    lv_obj_set_style_bg_color(right_container, BG_COLOR, LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(right_container, LV_OPA_COVER, LV_STATE_DEFAULT);
    lv_obj_set_style_pad_all(right_container, PADDING, LV_STATE_DEFAULT);
    lv_obj_set_style_border_width(right_container, 1, LV_STATE_DEFAULT);
    lv_obj_set_style_border_color(right_container, BORDER_COLOR, LV_STATE_DEFAULT);
    lv_obj_set_style_radius(right_container, BORDER_RADIUS, LV_STATE_DEFAULT);
    lv_obj_align(right_container, LV_ALIGN_RIGHT_MID, -5, 0);

    // Create status labels
    currentSpeedLabel = lv_label_create(right_container);
    lv_obj_align(currentSpeedLabel, LV_ALIGN_TOP_LEFT, 0, 0);
    lv_obj_set_style_text_font(currentSpeedLabel, &lv_font_montserrat_14, LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(currentSpeedLabel, lv_color_white(), LV_STATE_DEFAULT);
    lv_label_set_text(currentSpeedLabel, "Current: 0%");

    targetSpeedLabel = lv_label_create(right_container);
    lv_obj_align(targetSpeedLabel, LV_ALIGN_TOP_LEFT, 0, 25);
    lv_obj_set_style_text_font(targetSpeedLabel, &lv_font_montserrat_14, LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(targetSpeedLabel, lv_color_white(), LV_STATE_DEFAULT);
    lv_label_set_text(targetSpeedLabel, "Target: 0%");

    modeLabel = lv_label_create(right_container);
    lv_obj_align(modeLabel, LV_ALIGN_TOP_LEFT, 0, 60);
    lv_obj_set_style_text_font(modeLabel, &lv_font_montserrat_14, LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(modeLabel, lv_color_white(), LV_STATE_DEFAULT);
    lv_label_set_text(modeLabel, "Mode: Auto");
}

void DisplayUI::update(float temp, int fanSpeed, int targetSpeed, FanController::Mode mode,
                      bool wifiConnected, bool mqttConnected, bool nightMode) {
    if (!initialized) return;

    updateTemperatureDisplay(temp);
    updateStatusIndicators(wifiConnected, mqttConnected, nightMode);
    updateSpeedDisplay(fanSpeed, targetSpeed);
    updateModeDisplay(mode, nightMode);
}

void DisplayUI::updateTemperatureDisplay(float temp) {
    // Update temperature arc value
    int arcValue = constrain((int)(temp * 2), 0, 100);
    lv_arc_set_value(arc, arcValue);
    
    // Update temperature label
    char tempStr[32];
    snprintf(tempStr, sizeof(tempStr), "%.1f°C", temp);
    lv_label_set_text(tempLabel, tempStr);

    // Set arc color based on temperature
    lv_color_t tempColor = (temp < 30.0f) ? SUCCESS_COLOR :
                          (temp < 40.0f) ? WARNING_COLOR :
                                         ERROR_COLOR;
    lv_obj_set_style_arc_color(arc, tempColor, LV_PART_INDICATOR | LV_STATE_DEFAULT);
}

void DisplayUI::updateStatusIndicators(bool wifiConnected, bool mqttConnected, bool nightMode) {
    // Update WiFi status
    lv_label_set_text(wifiLabel, wifiConnected ? LV_SYMBOL_WIFI : LV_SYMBOL_CLOSE);
    lv_obj_set_style_text_color(wifiLabel, 
        wifiConnected ? SUCCESS_COLOR : ERROR_COLOR, 
        LV_STATE_DEFAULT);

    // Update night mode indicator
    lv_obj_set_style_text_color(nightLabel, 
        nightMode ? NIGHT_COLOR : INACTIVE_COLOR,
        LV_STATE_DEFAULT);

    // Update MQTT status
    lv_label_set_text(mqttLabel, mqttConnected ? "M" : "×");
    lv_obj_set_style_text_color(mqttLabel,
        mqttConnected ? SUCCESS_COLOR : ERROR_COLOR,
        LV_STATE_DEFAULT);
}

void DisplayUI::updateSpeedDisplay(int fanSpeed, int targetSpeed) {
    char currentStr[32], targetStr[32];
    snprintf(currentStr, sizeof(currentStr), "Current: %d%%", fanSpeed);
    snprintf(targetStr, sizeof(targetStr), "Target: %d%%", targetSpeed);
    
    lv_label_set_text(currentSpeedLabel, currentStr);
    lv_label_set_text(targetSpeedLabel, targetStr);

    // Set color based on speed match
    lv_color_t speedColor = (abs(targetSpeed - fanSpeed) <= 5) ?
                           SUCCESS_COLOR : WARNING_COLOR;
    
    lv_obj_set_style_text_color(currentSpeedLabel, speedColor, LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(targetSpeedLabel, speedColor, LV_STATE_DEFAULT);
}

void DisplayUI::updateModeDisplay(FanController::Mode mode, bool nightMode) {
    char modeStr[32];
    snprintf(modeStr, sizeof(modeStr), "Mode: %s%s", 
             mode == FanController::Mode::AUTO ? "Auto" : "Manual",
             nightMode ? " (N)" : "");
    lv_label_set_text(modeLabel, modeStr);
    
    lv_obj_set_style_text_color(modeLabel, 
        mode == FanController::Mode::AUTO ? SUCCESS_COLOR : WARNING_COLOR,
        LV_STATE_DEFAULT);
}

lv_obj_t* DisplayUI::getArc() {
    return arc;
}