#include "ILI9341_hardware.h"

const DisplayHardware::DisplayConfig ILI9341Hardware::config = {
    .width = 320,
    .height = 170,
    .bufferSize = 320 * 170  // Ensure this buffer size aligns with your needs
};

ILI9341Hardware::ILI9341Hardware() {
    tft = new Adafruit_ILI9341(Pins::CS, Pins::DC, Pins::RST);
}

ILI9341Hardware::~ILI9341Hardware() {
    delete tft;
}

bool ILI9341Hardware::initialize() {
    if (!tft) return false;

    SPI.begin();
    // Set optimized SPI settings similar to the previous implementation
    SPI.setFrequency(40000000); // Set to 40MHz or maximum reliable speed
    SPI.setDataMode(SPI_MODE0);
    SPI.setBitOrder(MSBFIRST);
    
    tft->begin();
    tft->setRotation(1); // Set display rotation

    return true; // Return success status
}

void ILI9341Hardware::setPower(bool on) {
    if (on) {
        powerOn();
    } else {
        powerOff();
    }
}

void ILI9341Hardware::setBrightness(uint8_t level) {
    Serial.printf("setBrightness called with value: %d\n", level);
    analogWrite(Pins::BL, level);
}

void ILI9341Hardware::flush(const Rect& area, lv_color_t* pixels) {
    if (!tft) return;

    uint32_t size = (area.x2 - area.x1 + 1) * (area.y2 - area.y1 + 1);
    tft->startWrite();    
    tft->setAddrWindow(area.x1, area.y1, 
                      area.x2 - area.x1 + 1, 
                      area.y2 - area.y1 + 1);
    tft->writePixels((uint16_t*)pixels, size);
    tft->endWrite();
}

void ILI9341Hardware::powerOn() {
    Serial.println("I am in herer!"); 
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

void ILI9341Hardware::sendCommand(uint8_t cmd) {
    tft->startWrite();
    tft->writeCommand(cmd);
    tft->endWrite();
}
