#ifndef DISPLAY_HARDWARE_H
#define DISPLAY_HARDWARE_H

#include <Arduino.h>
#include "lvgl.h" // Ensure LittlevGL library is included for lvgl types

class DisplayHardware {
public:
    virtual ~DisplayHardware() = default;
    
    struct Rect {
        uint16_t x1;
        uint16_t y1;
        uint16_t x2;
        uint16_t y2;
        
        uint16_t width() const { return x2 - x1 + 1; }
        uint16_t height() const { return y2 - y1 + 1; }
    };

    struct DisplayConfig {
        uint16_t width;
        uint16_t height;
        uint32_t bufferSize;
        
        uint32_t totalPixels() const { return static_cast<uint32_t>(width) * height; }
    };

    static DisplayHardware* createHardware();

    virtual bool initialize() = 0;
    virtual void setPower(bool on) = 0;
    virtual void setBrightness(uint8_t level) = 0;
    virtual void flush(const Rect& area, lv_color_t* pixels) = 0;
    virtual const DisplayConfig& getConfig() const = 0;

    virtual uint8_t getSleepButtonPin() const = 0;
    virtual uint8_t getWakeButtonPin() const = 0;
    
    static constexpr uint16_t BUTTON_DEBOUNCE_MS = 50;
    static constexpr uint16_t SLEEP_ANIMATION_MS = 1000;

    enum class PowerState {
        ON,
        OFF,
        SLEEP
    };
    
    PowerState powerState = PowerState::OFF;
    
protected:

    virtual void powerOn() = 0;
    virtual void powerOff() = 0;
    virtual void enterSleep() = 0;
    virtual void wakeFromSleep() = 0;
    virtual void sendCommand(uint8_t cmd) = 0;
    virtual void enterDeepSleep();
    virtual void wakeFromDeepSleep();
};

#endif // DISPLAY_HARDWARE_H