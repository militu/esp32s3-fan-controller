#ifndef DISPLAY_DRIVER_H
#define DISPLAY_DRIVER_H

/*******************************************************************************
 * Includes
 ******************************************************************************/
// Arduino and LVGL core
#include <Arduino.h>
#include "lvgl.h"
#include "config.h"
#include "debug_log.h"

// ESP32 LCD drivers
#include <esp_lcd_types.h>
#include <esp_lcd_panel_ops.h>
#include <esp_lcd_panel_vendor.h>
#include <esp_lcd_panel_io.h>
#include <driver/i2c.h>

// Display hardware drivers
#include <Adafruit_ILI9341.h>
#include <SPI.h>

// Local configurations
#include "display_config.h"

/*******************************************************************************
 * Display Buffer Configuration
 ******************************************************************************/
#define LVGL_LCD_BUF_SIZE           (TFT_WIDTH * 10)
#define EXAMPLE_LCD_PIXEL_CLOCK_HZ  (20 * 1000 * 1000)  // 20MHz pixel clock

/*******************************************************************************
 * Base Display Driver Class
 ******************************************************************************/
/**
 * Abstract base class for display drivers
 * Provides interface for basic display operations
 */
class DisplayDriver {
public:
    virtual ~DisplayDriver() = default;
    
    // Core display operations
    virtual bool begin() = 0;
    virtual void setBrightness(uint8_t brightness) = 0;
    virtual void flush(const lv_area_t* area, uint8_t* pixels) = 0;
    
    // Display properties
    virtual uint16_t width() const = 0;
    virtual uint16_t height() const = 0;
    
    // Optional test functionality
    virtual void testDisplay() {
        Serial.println("Base class testDisplay called");
    }

protected:
    bool initialized = false;
};

/*******************************************************************************
 * ILI9341 Display Driver
 ******************************************************************************/
/**
 * Driver implementation for ILI9341-based displays
 */
class ILI9341Driver : public DisplayDriver {
public:
    // Constructor and core functions
    ILI9341Driver(uint8_t cs, uint8_t dc);
    bool begin() override;
    void setBrightness(uint8_t brightness) override;
    void flush(const lv_area_t* area, uint8_t* pixels) override;
    
    // Display properties
    uint16_t width() const override { return TFT_WIDTH; }
    uint16_t height() const override { return TFT_HEIGHT_ILI9341; }
    
    // Display testing
    void testDisplay() override;

private:
    uint8_t csPin;              // Chip select pin
    uint8_t dcPin;              // Data/Command pin
    Adafruit_ILI9341* tft;      // Display driver instance
};

/*******************************************************************************
 * Lilygo S3 Display Driver
 ******************************************************************************/
/**
 * Driver implementation for Lilygo S3 display hardware
 */
class LilygoS3Driver : public DisplayDriver {
public:
    // Constructor and core functions
    LilygoS3Driver();
    bool begin() override;
    void setBrightness(uint8_t brightness) override;
    void flush(const lv_area_t* area, uint8_t* pixels) override;
    
    // Display properties
    uint16_t width() const override { return TFT_WIDTH; }
    uint16_t height() const override { return TFT_HEIGHT_LILYGO; }
    void testDisplay() override;

private:
    // Initialization helpers
    void initBus();             // Initialize display bus
    void initPanel();           // Initialize display panel
    
    // Hardware handles
    esp_lcd_panel_handle_t panelHandle;      // LCD panel handle
    esp_lcd_panel_io_handle_t ioHandle;      // LCD I/O handle
    lv_disp_t* disp;                         // LVGL display instance
};

#endif // DISPLAY_DRIVER_H