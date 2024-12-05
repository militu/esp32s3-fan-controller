#ifndef USE_LILYGO_S3

#include "ili9341_hardware.h"
#include <lvgl.h>

const DisplayHardware::DisplayConfig ILI9341Hardware::config = {
    .width = 320,
    .height = 170, // emulate Lilygo S3
    .bufferSize = 320 * 170, // emulate Lilygo S3
};

ILI9341Hardware::ILI9341Hardware() {
    tft = new Adafruit_ILI9341(Pins::CS, Pins::DC, Pins::RST);
}

ILI9341Hardware::~ILI9341Hardware() {
    delete tft;
}

bool ILI9341Hardware::initialize() {
    if (!tft) return false;

    pinMode(Pins::BL, OUTPUT);
    digitalWrite(Pins::BL, HIGH);  // Start with backlight on

    // Initialize SPI communication
    SPI.begin();
    SPI.setFrequency(40000000);
    SPI.setDataMode(SPI_MODE0);
    SPI.setBitOrder(MSBFIRST);
    
    tft->begin();
    tft->setRotation(1);

    // Initialize LVGL
    lv_init();

    static lv_disp_draw_buf_t draw_buf;
    static lv_color_t* buf1 = (lv_color_t*)heap_caps_malloc(config.width * 40 * sizeof(lv_color_t), MALLOC_CAP_DMA);
    
    lv_disp_draw_buf_init(&draw_buf, buf1, nullptr, config.width * 40);
    
    lv_disp_drv_init(&disp_drv);
    disp_drv.hor_res = config.width;
    disp_drv.ver_res = config.height;
    disp_drv.flush_cb = [](lv_disp_drv_t *drv, const lv_area_t *area, lv_color_t *color_p) {
        ILI9341Hardware *instance = static_cast<ILI9341Hardware *>(drv->user_data);
        instance->flush({
            static_cast<uint16_t>(area->x1),
            static_cast<uint16_t>(area->y1),
            static_cast<uint16_t>(area->x2),
            static_cast<uint16_t>(area->y2)
        }, color_p);
    };
    disp_drv.draw_buf = &draw_buf;
    disp_drv.user_data = this;
    lv_disp_drv_register(&disp_drv);

    return true;
}

void ILI9341Hardware::setPower(bool on) {
    on ? powerOn() : powerOff();
}

void ILI9341Hardware::setBrightness(uint8_t level) {
    analogWrite(Pins::BL, level);
}

void ILI9341Hardware::flush(const Rect& area, lv_color_t* pixels) {
    if (!tft) return;

    tft->startWrite();    
    tft->setAddrWindow(area.x1, area.y1, area.width(), area.height());
    tft->writePixels((uint16_t*)pixels, area.width() * area.height());
    tft->endWrite();

    lv_disp_flush_ready(&disp_drv);
}

void ILI9341Hardware::sendCommand(uint8_t cmd) {
    tft->startWrite();
    tft->writeCommand(cmd);
    tft->endWrite();
}

// The following methods are not working on Wokwi simulation
void ILI9341Hardware::powerOn() {
    sendCommand(SLPOUT_COMMAND);
    delay(120);
    sendCommand(DISPON_COMMAND);
}

void ILI9341Hardware::powerOff() {
    sendCommand(DISPOFF_COMMAND);
    delay(120);
    sendCommand(SLPIN_COMMAND);
}


void ILI9341Hardware::enterSleep() {
    sendCommand(SLPIN_COMMAND);
    delay(120);
}

void ILI9341Hardware::wakeFromSleep() {
    sendCommand(SLPOUT_COMMAND);
    delay(120);
}

void ILI9341Hardware::enterDeepSleep() {
    // Send display to sleep
    sendCommand(SLPIN_COMMAND);
    delay(5);

    // Power down display
    digitalWrite(Pins::BL, LOW);  // Turn off backlight
    digitalWrite(Pins::CS, HIGH); // Deselect display
    digitalWrite(Pins::DC, LOW);  // Command mode

    // Configure wake-up source (Button 0)
    esp_sleep_enable_ext0_wakeup((gpio_num_t)getWakeButtonPin(), 0);
    
    // Enter deep sleep
    esp_deep_sleep_start();
}

void ILI9341Hardware::wakeFromDeepSleep() {
    // Initialize SPI again since it was powered down
    SPI.begin();
    SPI.setFrequency(40000000);
    SPI.setDataMode(SPI_MODE0);
    SPI.setBitOrder(MSBFIRST);

    // Wake display
    sendCommand(SLPOUT_COMMAND);
    delay(120);  // Required delay after sleep out

    // Restore backlight
    digitalWrite(Pins::BL, HIGH);

    // Re-initialize display if needed
    tft->setRotation(1);
    tft->fillScreen(0); // Clear screen
}

#endif // USE_LILYGO_S3