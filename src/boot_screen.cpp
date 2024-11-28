#include "boot_screen.h"

BootScreen::BootScreen()
    : displayWidth(0)
    , displayHeight(0)
    , screen(nullptr)
    , titleLabel(nullptr)
    , wifiLabel(nullptr)
    , ntpLabel(nullptr)
    , mqttLabel(nullptr)
    , initialized(false)
{
}

void BootScreen::begin() {
    if (initialized) return;
    createUI();
    initialized = true;
}

void BootScreen::createUI() {
    createMainScreen();
    
    // Create title with more space
    titleLabel = lv_label_create(screen);
    lv_label_set_text(titleLabel, "System Initializing...");
    lv_obj_align(titleLabel, LV_ALIGN_TOP_MID, 0, 20);
    lv_obj_set_style_text_font(titleLabel, &lv_font_montserrat_16, LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(titleLabel, lv_color_white(), LV_STATE_DEFAULT);

    // Create status labels with more spacing and individual detail labels
    uint16_t yStart = 70;
    uint16_t ySpacing = 50;  // Increased spacing
    
    // WiFi section
    wifiLabel = createStatusLabel("WiFi: Pending", yStart);
    wifiDetailLabel = createDetailLabel("", yStart + 20);
    
    // NTP section
    ntpLabel = createStatusLabel("NTP: Pending", yStart + ySpacing);
    ntpDetailLabel = createDetailLabel("", yStart + ySpacing + 20);
    
    // MQTT section
    mqttLabel = createStatusLabel("MQTT: Pending", yStart + ySpacing * 2);
    mqttDetailLabel = createDetailLabel("", yStart + ySpacing * 2 + 20);

    lv_scr_load(screen);
}

lv_obj_t* BootScreen::createDetailLabel(const char* text, lv_coord_t yOffset) {
    lv_obj_t* label = lv_label_create(screen);
    lv_label_set_text(label, text);
    lv_obj_align(label, LV_ALIGN_TOP_MID, 0, yOffset);
    lv_obj_set_style_text_font(label, &lv_font_montserrat_12, LV_STATE_DEFAULT); // Smaller font
    lv_obj_set_style_text_color(label, lv_color_hex(0x808080), LV_STATE_DEFAULT); // Lighter color
    return label;
}

void BootScreen::createMainScreen() {
    screen = lv_obj_create(NULL);
    lv_obj_set_scrollbar_mode(screen, LV_SCROLLBAR_MODE_OFF);
    lv_obj_set_style_bg_color(screen, lv_color_black(), LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(screen, LV_OPA_COVER, LV_STATE_DEFAULT);
}

lv_obj_t* BootScreen::createStatusLabel(const char* text, lv_coord_t yOffset) {
    lv_obj_t* label = lv_label_create(screen);
    lv_label_set_text(label, text);
    lv_obj_align(label, LV_ALIGN_TOP_MID, 0, yOffset);
    lv_obj_set_style_text_font(label, &lv_font_montserrat_14, LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(label, lv_color_white(), LV_STATE_DEFAULT);
    return label;
}

void BootScreen::updateStatus(const char* component, ComponentStatus status) {
    if (!initialized) return;

    lv_obj_t* targetLabel = nullptr;
    const char* baseText = nullptr;
    
    if (strcmp(component, "WiFi") == 0) {
        targetLabel = wifiLabel;
        baseText = "WiFi: ";
    } else if (strcmp(component, "NTP") == 0) {
        targetLabel = ntpLabel;
        baseText = "NTP: ";
    } else if (strcmp(component, "MQTT") == 0) {
        targetLabel = mqttLabel;
        baseText = "MQTT: ";
    }

    if (targetLabel && baseText) {
        String statusText = String(baseText) + getStatusText(status);
        lv_label_set_text(targetLabel, statusText.c_str());
        lv_obj_set_style_text_color(targetLabel, getStatusColor(status), LV_STATE_DEFAULT);
    }
}

void BootScreen::updateStatusWithDetail(const char* component, ComponentStatus status, const char* detail) {
    if (!initialized) return;

    lv_obj_t* targetLabel = nullptr;
    lv_obj_t* targetDetailLabel = nullptr;
    
    if (strcmp(component, "WiFi") == 0) {
        targetLabel = wifiLabel;
        targetDetailLabel = wifiDetailLabel;
    } else if (strcmp(component, "NTP") == 0) {
        targetLabel = ntpLabel;
        targetDetailLabel = ntpDetailLabel;
    } else if (strcmp(component, "MQTT") == 0) {
        targetLabel = mqttLabel;
        targetDetailLabel = mqttDetailLabel;
    }

    if (targetLabel && targetDetailLabel) {
        String statusText = String(component) + ": " + getStatusText(status);
        lv_label_set_text(targetLabel, statusText.c_str());
        lv_obj_set_style_text_color(targetLabel, getStatusColor(status), LV_STATE_DEFAULT);
        
        if (detail && strlen(detail) > 0) {
            lv_label_set_text(targetDetailLabel, detail);
        } else {
            lv_label_set_text(targetDetailLabel, "");
        }
    }
}

const char* BootScreen::getStatusText(ComponentStatus status) {
    switch (status) {
        case ComponentStatus::PENDING: return "Pending";
        case ComponentStatus::WORKING: return "Initializing...";
        case ComponentStatus::SUCCESS: return "OK";
        case ComponentStatus::FAILED:  return "Failed";
        default: return "Unknown";
    }
}

lv_color_t BootScreen::getStatusColor(ComponentStatus status) {
    switch (status) {
        case ComponentStatus::PENDING: return lv_color_hex(0x808080);  // Gray
        case ComponentStatus::WORKING: return lv_color_hex(0xFFA500);  // Orange
        case ComponentStatus::SUCCESS: return lv_color_hex(0x00FF00);  // Green
        case ComponentStatus::FAILED:  return lv_color_hex(0xFF0000);  // Red
        default: return lv_color_white();
    }
}