#include "display_driver.h"


DisplayDriver::DisplayDriver(DisplayHardware* hw) 
    : hardware(hw)
    , initialized(false) {
    uiMutex = xSemaphoreCreateMutex();
    if (!uiMutex) {
        Serial.println("Failed to create UI mutex!");
    }
}

DisplayDriver::~DisplayDriver() { 
    if (uiMutex) {
        vSemaphoreDelete(uiMutex);
    }
    delete hardware; 
}

bool DisplayDriver::begin() {
    if (initialized || !hardware) return false;
    return (initialized = hardware->initialize());
}

void DisplayDriver::setBrightness(uint8_t brightness) {
    if (hardware) {
        hardware->setBrightness(brightness);
    }
}

void DisplayDriver::flush(const lv_area_t* area, lv_color_t* pixels) {
    if (hardware) {
        DisplayHardware::Rect rect{
            .x1 = static_cast<uint16_t>(area->x1),
            .y1 = static_cast<uint16_t>(area->y1),
            .x2 = static_cast<uint16_t>(area->x2),
            .y2 = static_cast<uint16_t>(area->y2)
        };
        hardware->flush(rect, pixels);
    }
}

uint16_t DisplayDriver::width() const { 
    return hardware ? hardware->getConfig().width : 0; 
}

uint16_t DisplayDriver::height() const { 
    return hardware ? hardware->getConfig().height : 0; 
}

void DisplayDriver::setPower(bool on) {
    if (hardware) {
        hardware->setPower(on);
    }
}


DisplayHardware::PowerState DisplayDriver::getPowerState() const {
    return hardware ? hardware->powerState : DisplayHardware::PowerState::OFF;
}

DisplayDriver* createDisplayDriver() {
    DisplayHardware* hw;
    #ifdef USE_LILYGO_S3
        hw = LilygoHardware::create();
    #else
        hw = ILI9341Hardware::create();
    #endif
    return new DisplayDriver(hw);
}