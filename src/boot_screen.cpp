#include "boot_screen.h"
#include "display_colors.h"

/*******************************************************************************
 * Construction / Destruction
 ******************************************************************************/

BootScreen::BootScreen()
    : displayWidth(0)
    , displayHeight(0)
    , screen(nullptr)
    , titleLabel(nullptr)
    , wifiLabel(nullptr)
    , wifiDetailLabel(nullptr)
    , ntpLabel(nullptr)
    , ntpDetailLabel(nullptr)
    , mqttLabel(nullptr)
    , mqttDetailLabel(nullptr)
    , initialized(false)
{
    uiMutex = xSemaphoreCreateMutex();
}

BootScreen::~BootScreen() {
    if (uiMutex != nullptr) {
        vSemaphoreDelete(uiMutex);
    }
}

/*******************************************************************************
 * Initialization & Core UI
 ******************************************************************************/

void BootScreen::begin() {
    if (initialized) return;
    createUI();
    initialized = true;
}

void BootScreen::createUI() {
    MutexGuard guard(uiMutex, pdMS_TO_TICKS(100));
    if (!guard.isLocked()) return;

    createMainScreen();
    
    // Margins and spacing calculations
    uint16_t marginX = displayWidth * 0.05;
    uint16_t titleHeight = displayHeight * 0.15;
    uint16_t lineSpacing = displayHeight * 0.03;
    
    // Title positioning with updated colors
    titleLabel = lv_label_create(screen);
    lv_label_set_text(titleLabel, "System Initializing...");
    lv_obj_align(titleLabel, LV_ALIGN_TOP_MID, 0, titleHeight * 0.3);
    lv_obj_set_style_text_font(titleLabel, &lv_font_montserrat_16, LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(titleLabel, lv_color_hex(DisplayColors::SUCCESS), LV_STATE_DEFAULT);

    // Decorative line with updated colors
    lv_obj_t* topLine = lv_line_create(screen);
    static lv_point_t linePoints[2];
    linePoints[0].x = marginX;
    linePoints[0].y = titleHeight - lineSpacing;
    linePoints[1].x = displayWidth - marginX;
    linePoints[1].y = titleHeight - lineSpacing;
    lv_line_set_points(topLine, linePoints, 2);
    lv_obj_set_style_line_color(topLine, lv_color_hex(DisplayColors::BORDER), LV_PART_MAIN);
    lv_obj_set_style_line_width(topLine, displayHeight * 0.005, LV_PART_MAIN);
    
    // Rest of layout calculations
    uint16_t topPadding = displayHeight * 0.08;
    uint16_t bottomPadding = displayHeight * 0.08;
    uint16_t contentStartY = titleHeight + topPadding;
    uint16_t availableHeight = displayHeight - titleHeight - topPadding - bottomPadding;
    uint16_t sectionHeight = availableHeight * 0.25;
    uint16_t sectionSpacing = availableHeight * 0.06;
    uint16_t totalSectionsHeight = (sectionHeight * 3) + (sectionSpacing * 2);
    uint16_t verticalOffset = (availableHeight - totalSectionsHeight) / 2;
    uint16_t finalStartY = contentStartY + verticalOffset;
    
    createStatusSection("WiFi", finalStartY, &wifiLabel, &wifiDetailLabel);
    createStatusSection("NTP", finalStartY + sectionHeight + sectionSpacing, &ntpLabel, &ntpDetailLabel);
    createStatusSection("MQTT", finalStartY + (sectionHeight + sectionSpacing) * 2, &mqttLabel, &mqttDetailLabel);

    lv_scr_load(screen);
}

void BootScreen::createMainScreen() {
    screen = lv_obj_create(NULL);
    lv_obj_set_scrollbar_mode(screen, LV_SCROLLBAR_MODE_OFF);
    
    // Create gradient background with new colors
    lv_obj_set_style_bg_color(screen, lv_color_hex(DisplayColors::BG_DARK), LV_STATE_DEFAULT);
    lv_obj_set_style_bg_grad_color(screen, lv_color_hex(DisplayColors::BG_LIGHT), LV_STATE_DEFAULT);
    lv_obj_set_style_bg_grad_dir(screen, LV_GRAD_DIR_VER, LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(screen, LV_OPA_COVER, LV_STATE_DEFAULT);
}

/*******************************************************************************
 * Status Management
 ******************************************************************************/

void BootScreen::updateStatus(const char* component, ComponentStatus status) {
    MutexGuard guard(uiMutex, pdMS_TO_TICKS(100));
    if (!guard.isLocked()) return;

    // First update the status, then call updateStatusWithDetail to ensure synchronization
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
        // Update main status
        String statusText = String(component) + ": " + getStatusText(status);
        lv_label_set_text(targetLabel, statusText.c_str());
        lv_obj_set_style_text_color(targetLabel, getStatusColor(status), LV_STATE_DEFAULT);
        
        // Clear detail if not provided
        lv_label_set_text(targetDetailLabel, "");
        
        // Update container style
        lv_obj_t* container = lv_obj_get_parent(targetLabel);
        animateContainer(container, status);
    }
}

void BootScreen::updateStatusWithDetail(const char* component, ComponentStatus status, const char* detail) {
    MutexGuard guard(uiMutex, pdMS_TO_TICKS(100));
    if (!guard.isLocked()) return;

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
        // Update main status first
        String statusText = String(component) + ": " + getStatusText(status);
        lv_label_set_text(targetLabel, statusText.c_str());
        lv_obj_set_style_text_color(targetLabel, getStatusColor(status), LV_STATE_DEFAULT);
        
        // Then update detail
        if (detail && strlen(detail) > 0) {
            lv_label_set_text(targetDetailLabel, detail);
        }
        
        // Update container style
        lv_obj_t* container = lv_obj_get_parent(targetLabel);
        animateContainer(container, status);
    }
}

lv_color_t BootScreen::getStatusColor(ComponentStatus status) {
    switch (status) {
        case ComponentStatus::PENDING: return lv_color_hex(DisplayColors::INACTIVE);
        case ComponentStatus::WORKING: return lv_color_hex(DisplayColors::WORKING);
        case ComponentStatus::SUCCESS: return lv_color_hex(DisplayColors::SUCCESS);
        case ComponentStatus::FAILED:  return lv_color_hex(DisplayColors::ERROR);
        default: return lv_color_hex(DisplayColors::TEXT_PRIMARY);
    }
}

const char* BootScreen::getStatusText(ComponentStatus status) {
    switch (status) {
        case ComponentStatus::PENDING: return "Pending";
        case ComponentStatus::WORKING: return "Working";
        case ComponentStatus::SUCCESS: return "Success";
        case ComponentStatus::FAILED:  return "Failed";
        default: return "Unknown";
    }
}

/*******************************************************************************
 * UI Layout & Components
 ******************************************************************************/

void BootScreen::createStatusSection(const char* title, uint16_t yOffset, 
                                   lv_obj_t** statusLabel, lv_obj_t** detailLabel) {
    // Container dimensions
    uint16_t containerWidth = displayWidth * 0.9;
    uint16_t containerHeight = displayHeight * 0.2;  // Fixed proportion of display height
    
    // Calculate padding and spacing
    uint16_t padding = containerHeight * 0.1;        // 10% of container height for padding
    
    // Create container
    lv_obj_t* cont = lv_obj_create(screen);
    lv_obj_set_size(cont, containerWidth, containerHeight);
    lv_obj_align(cont, LV_ALIGN_TOP_MID, 0, yOffset);
    
    // Container styling
    lv_obj_set_style_bg_color(cont, lv_color_hex(DisplayColors::BG_DARK), LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(cont, LV_OPA_50, LV_STATE_DEFAULT);
    lv_obj_set_style_border_color(cont, lv_color_hex(DisplayColors::BORDER), LV_STATE_DEFAULT);
    lv_obj_set_style_border_width(cont, displayWidth * 0.002, LV_STATE_DEFAULT);
    lv_obj_set_style_radius(cont, containerHeight * 0.1, LV_STATE_DEFAULT);
    lv_obj_set_style_pad_all(cont, padding, LV_STATE_DEFAULT);
    
    // Disable scrolling on container
    lv_obj_clear_flag(cont, LV_OBJ_FLAG_SCROLLABLE);

    // Calculate label dimensions
    uint16_t availableWidth = containerWidth - (padding * 2);
    uint16_t availableHeight = containerHeight - (padding * 2);
    uint16_t labelSpacing = availableHeight * 0.1;  // 10% spacing between labels

    // Status Label
    *statusLabel = lv_label_create(cont);
    lv_obj_set_width(*statusLabel, availableWidth);
    lv_obj_set_style_pad_all(*statusLabel, 0, LV_STATE_DEFAULT);
    lv_label_set_text(*statusLabel, title);
    
    // Select font size based on container height
    const lv_font_t* statusFont = (containerHeight >= 100) ? &lv_font_montserrat_14 :
                                 (containerHeight >= 80)  ? &lv_font_montserrat_12 :
                                                          &lv_font_montserrat_10;
    lv_obj_set_style_text_font(*statusLabel, statusFont, LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(*statusLabel, lv_color_hex(DisplayColors::TEXT_PRIMARY), LV_STATE_DEFAULT);
    
    // Position status label at top
    lv_obj_align(*statusLabel, LV_ALIGN_TOP_LEFT, 0, 0);

    // Detail Label
    *detailLabel = lv_label_create(cont);
    lv_obj_set_width(*detailLabel, availableWidth);
    lv_obj_set_style_pad_all(*detailLabel, 0, LV_STATE_DEFAULT);
    lv_label_set_text(*detailLabel, "Pending...");
    
    // Select smaller font for detail text
    const lv_font_t* detailFont = (containerHeight >= 100) ? &lv_font_montserrat_12 :
                                 (containerHeight >= 80)  ? &lv_font_montserrat_10 :
                                                          &lv_font_montserrat_8;
    lv_obj_set_style_text_font(*detailLabel, detailFont, LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(*detailLabel, lv_color_hex(DisplayColors::TEXT_SECONDARY), LV_STATE_DEFAULT);
    
    // Position detail label below status label with spacing
    lv_obj_align_to(*detailLabel, *statusLabel, LV_ALIGN_OUT_BOTTOM_LEFT, 0, labelSpacing);
    
    // Configure text wrapping
    lv_label_set_long_mode(*detailLabel, LV_LABEL_LONG_WRAP);
    lv_obj_set_style_text_line_space(*detailLabel, 2, LV_STATE_DEFAULT);  // Add some line spacing
}

const lv_font_t* BootScreen::selectDynamicFont(uint16_t width) {
    if (width >= 480) return &lv_font_montserrat_16;
    if (width >= 320) return &lv_font_montserrat_14;
    return &lv_font_montserrat_12;
}

const lv_font_t* BootScreen::selectDetailFont(uint16_t width) {
    if (width >= 480) return &lv_font_montserrat_14;
    if (width >= 320) return &lv_font_montserrat_12;
    return &lv_font_montserrat_10;
}

/*******************************************************************************
 * Animation & Visual Effects
 ******************************************************************************/

void BootScreen::animateContainer(lv_obj_t* container, ComponentStatus status) {
    // Set background color based on status - using darker versions of palette colors
    lv_color_t bgColor;
    switch (status) {
        case ComponentStatus::WORKING:
            // Darker version of WORKING color
            bgColor = lv_color_mix(
                lv_color_hex(DisplayColors::BG_DARK),
                lv_color_hex(DisplayColors::WORKING),
                64  // Mix ratio - smaller number = darker
            );
            break;
        case ComponentStatus::SUCCESS:
            // Darker version of SUCCESS color
            bgColor = lv_color_mix(
                lv_color_hex(DisplayColors::BG_DARK),
                lv_color_hex(DisplayColors::SUCCESS),
                64
            );
            break;
        case ComponentStatus::FAILED:
            // Darker version of ERROR color
            bgColor = lv_color_mix(
                lv_color_hex(DisplayColors::BG_DARK),
                lv_color_hex(DisplayColors::ERROR),
                64
            );
            break;
        default:
            bgColor = lv_color_hex(DisplayColors::BG_DARK);
    }
    lv_obj_set_style_bg_color(container, bgColor, LV_STATE_DEFAULT);

    // Create animation
    lv_anim_t a;
    lv_anim_init(&a);
    lv_anim_set_var(&a, container);
    lv_anim_set_values(&a, 50, 80);
    lv_anim_set_time(&a, 150);
    lv_anim_set_exec_cb(&a, [](void* obj, int32_t value) {
        lv_obj_set_style_bg_opa((lv_obj_t*)obj, value, LV_STATE_DEFAULT);
    });
    lv_anim_set_path_cb(&a, lv_anim_path_ease_out);
    lv_anim_start(&a);
}