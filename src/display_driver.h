#ifndef DISPLAY_DRIVER_H
#define DISPLAY_DRIVER_H

#include "display_hardware.h"

#ifdef USE_LILYGO_S3
#include "lilygo_hardware.h"
#else
#include "ili9341_hardware.h"
#endif

class DisplayDriver {
public:
    // Constructor accepts a pointer to a DisplayHardware object
    explicit DisplayDriver(DisplayHardware* hw);
    ~DisplayDriver();

    // Initializes the display hardware
    bool begin();

    // Sets the brightness of the display
    void setBrightness(uint8_t brightness);

    // Flushes a rectangular area to the display
    void flush(const lv_area_t* area, lv_color_t* pixels);

    // Retrieves the width and height of the display
    uint16_t width() const;
    uint16_t height() const;

    // Controls the power state of the display
    void setPower(bool on);

    // Retrieves the current power state
    DisplayHardware::PowerState getPowerState() const;

    bool lockUI(TickType_t timeout = portMAX_DELAY) {
        return uiMutex ? (xSemaphoreTake(uiMutex, timeout) == pdTRUE) : false;
    }
    
    void unlockUI() {
        if (uiMutex) {
            xSemaphoreGive(uiMutex);
        }
    }

private:
    DisplayHardware* hardware;  // Pointer to an instance of DisplayHardware
    bool initialized;           // Flag to track initialization status
    SemaphoreHandle_t uiMutex;

};

// Factory function to create the appropriate DisplayDriver
DisplayDriver* createDisplayDriver();

#endif // DISPLAY_DRIVER_H
