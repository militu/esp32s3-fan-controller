#ifndef DISPLAY_DRIVER_H
#define DISPLAY_DRIVER_H

#include "display_hardware.h"
#include "ili9341_hardware.h"
#include "lilygo_hardware.h"

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

private:
    DisplayHardware* hardware;  // Pointer to an instance of DisplayHardware
    bool initialized;           // Flag to track initialization status
};

// Factory function to create the appropriate DisplayDriver
DisplayDriver* createDisplayDriver();

#endif // DISPLAY_DRIVER_H
