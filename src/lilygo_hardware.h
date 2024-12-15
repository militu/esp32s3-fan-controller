#ifndef LILYGO_HARDWARE_H
#define LILYGO_HARDWARE_H

#include "display_hardware.h"
#include "config.h"
#include <esp_lcd_panel_io.h>
#include <esp_lcd_panel_vendor.h>
#include <esp_lcd_panel_ops.h>
#include <debug_log.h>

class LilygoHardware : public DisplayHardware {
public:
    static LilygoHardware* create() { return new LilygoHardware(); }
    bool initialize() override;
    void setPower(bool on) override;
    void setBrightness(uint8_t level) override;
    void flush(const Rect& area, lv_color_t* pixels) override;
    const DisplayConfig& getConfig() const override { return config; }
    uint8_t getSleepButtonPin() const override { return Config::Hardware::PIN_BUTTON_1; }
    uint8_t getWakeButtonPin() const override { return Config::Hardware::PIN_BUTTON_2; }

protected:
    void powerOn() override;
    void powerOff() override;
    void enterSleep() override;
    void wakeFromSleep() override;
    void sendCommand(uint8_t cmd) override;
    void enterDeepSleep() override;
    void wakeFromDeepSleep() override;

private:
    LilygoHardware();
    bool initializeBus();
    bool initializePanel();
    bool configureDisplay();

    struct Pins {
        static constexpr uint8_t BL = 38;
        static constexpr uint8_t POWER = 15;
        static constexpr uint8_t RES = 5;
        static constexpr uint8_t CS = 6;
        static constexpr uint8_t DC = 7;
        static constexpr uint8_t WR = 8;
        static constexpr uint8_t RD = 9;
        static constexpr uint8_t D0 = 39;
        static constexpr uint8_t D1 = 40;
        static constexpr uint8_t D2 = 41;
        static constexpr uint8_t D3 = 42;
        static constexpr uint8_t D4 = 45;
        static constexpr uint8_t D5 = 46;
        static constexpr uint8_t D6 = 47;
        static constexpr uint8_t D7 = 48;
    };

    struct PanelCommands {
        static constexpr uint8_t SLPIN = 0x10;
        static constexpr uint8_t SLPOUT = 0x11;
        static constexpr uint8_t DISPON = 0x29;
        static constexpr uint8_t DISPOFF = 0x28;
    };

    static const DisplayConfig config;
    esp_lcd_panel_handle_t panelHandle;
    esp_lcd_panel_io_handle_t ioHandle;
    lv_disp_drv_t disp_drv;
};

#endif // LILYGO_HARDWARE_H