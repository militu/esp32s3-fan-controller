#ifndef ILI9341_HARDWARE_H
#define ILI9341_HARDWARE_H

#include "display_hardware.h"
#include <Adafruit_ILI9341.h>
#include <SPI.h>

class ILI9341Hardware : public DisplayHardware {
public:
    static ILI9341Hardware* create() { return new ILI9341Hardware(); }
    bool initialize() override;
    void setPower(bool on) override;
    void setBrightness(uint8_t level) override;
    void flush(const Rect& area, lv_color_t* pixels) override;
    const DisplayConfig& getConfig() const override { return config; }
    uint8_t getSleepButtonPin() const override { return 7; }
    uint8_t getWakeButtonPin() const override { return 0; }

protected:
    void powerOn() override;
    void powerOff() override;
    void enterSleep() override;
    void wakeFromSleep() override;
    void sendCommand(uint8_t cmd) override;

private:
    ILI9341Hardware();
    ~ILI9341Hardware();

    struct Pins {
        static constexpr uint8_t MOSI = 11;
        static constexpr uint8_t MISO = 13;
        static constexpr uint8_t SCK = 12;
        static constexpr uint8_t CS = 15;
        static constexpr uint8_t DC = 2;
        static constexpr uint8_t RST = 4;
        static constexpr uint8_t BL = 6;
    };

    static constexpr uint8_t SLPIN_COMMAND = 0x10;
    static constexpr uint8_t SLPOUT_COMMAND = 0x11;
    static constexpr uint8_t DISPON_COMMAND = 0x29;
    static constexpr uint8_t DISPOFF_COMMAND = 0x28;
    
    static const DisplayConfig config;
    Adafruit_ILI9341* tft;
};

#endif // ILI9341_HARDWARE_H