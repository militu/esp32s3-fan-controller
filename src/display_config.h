#ifndef DISPLAY_CONFIG_H
#define DISPLAY_CONFIG_H

// Common display configuration
#define LVGL_BUFFER_SIZE        (320 * 10)  // 10 lines of screen width

// ILI9341 Configuration (when using SPI)
#define ILI9341_MOSI_PIN       11
#define ILI9341_MISO_PIN       13
#define ILI9341_SCK_PIN        12
#define ILI9341_CS_PIN         15
#define ILI9341_DC_PIN          2
#define ILI9341_RST_PIN         4
#define ILI9341_BL_PIN          6

// LilyGO S3 Configuration
#define PIN_LCD_BL             38
#define PIN_LCD_D0             39
#define PIN_LCD_D1             40
#define PIN_LCD_D2             41
#define PIN_LCD_D3             42
#define PIN_LCD_D4             45
#define PIN_LCD_D5             46
#define PIN_LCD_D6             47
#define PIN_LCD_D7             48
#define PIN_POWER_ON           15
#define PIN_LCD_RES            5
#define PIN_LCD_CS             6
#define PIN_LCD_DC             7
#define PIN_LCD_WR             8
#define PIN_LCD_RD             9

// Display timing
#define EXAMPLE_LCD_PIXEL_CLOCK_HZ   (20 * 1000 * 1000)
#define LCD_CLK_SRC                  LCD_CLK_SRC_PLL160M

// ST7789V Panel Commands
struct lcd_cmd_t {
    uint8_t cmd;
    uint8_t data[16];
    uint8_t len;
};

// Display dimensions
#define TFT_WIDTH              320
#define TFT_HEIGHT_ILI9341     240
#define TFT_HEIGHT_LILYGO      170

#endif // DISPLAY_CONFIG_H