#include "display_driver.h"

/*******************************************************************************
 * ILI9341 Display Driver Implementation
 ******************************************************************************/

/**
 * Constructor for ILI9341 display driver
 * @param cs Chip select pin
 * @param dc Data/Command pin
 */
ILI9341Driver::ILI9341Driver(uint8_t cs, uint8_t dc)
    : csPin(cs)
    , dcPin(dc)
    , tft(nullptr) {
}

/**
 * Initialize the display hardware and configuration
 * @return true if initialization successful, false otherwise
 */
bool ILI9341Driver::begin() {
    if (initialized) return false;

    // Initialize hardware SPI with optimized settings
    SPI.begin();
    // Set to maximum reliable speed for ILI9341
    SPI.setFrequency(40000000); // 40MHz
    // Use the most efficient data transfer mode
    SPI.setDataMode(SPI_MODE0);
    // Set to maximum bits per transfer
    SPI.setBitOrder(MSBFIRST);
    
    // Create and initialize the display driver
    tft = new Adafruit_ILI9341(csPin, dcPin, -1); // -1 for hardware SPI
    if (!tft) return false;
    
    // Initialize with optimized settings
    tft->begin(40000000); // Match SPI frequency
    tft->setRotation(1); // Set initial rotation
    
    initialized = true;
    return true;
}

/**
 * Set display brightness level
 * @param brightness Brightness level (0-255)
 */
void ILI9341Driver::setBrightness(uint8_t brightness) {
    DEBUG_LOG_DISPLAY("setBrightness called with value: %d\n", brightness);
}

/**
 * Flush display buffer to screen
 * @param area Display area to update
 * @param pixels Pixel data buffer
 */
void ILI9341Driver::flush(const lv_area_t* area, uint8_t* pixels) {
    if (!initialized || !tft) return;

    uint32_t start = millis();
    uint32_t size = (area->x2 - area->x1 + 1) * (area->y2 - area->y1 + 1);
    
    tft->startWrite();
    uint32_t afterStart = millis();
    
    tft->setAddrWindow(area->x1, area->y1, 
                      area->x2 - area->x1 + 1, 
                      area->y2 - area->y1 + 1);
    uint32_t afterWindow = millis();
    
    tft->writePixels((uint16_t*)pixels, size);
    uint32_t afterPixels = millis();
    
    tft->endWrite();
    uint32_t end = millis();
    
    lv_disp_flush_ready((lv_disp_drv_t*)lv_disp_get_default());
}

/**
 * Run display test sequence
 */
void ILI9341Driver::testDisplay() {
    if (!initialized || !tft) {
        Serial.println("Cannot test - display not initialized");
        return;
    }

    Serial.println("Running display test pattern...");
    
    // Clear display
    tft->fillScreen(ILI9341_BLACK);
    delay(500);
    
    // Draw test pattern
    tft->fillRect(0, 0, 50, 50, ILI9341_RED);
    tft->fillRect(60, 0, 50, 50, ILI9341_GREEN);
    tft->fillRect(120, 0, 50, 50, ILI9341_BLUE);
    
    // Draw test text
    tft->setTextColor(ILI9341_WHITE);
    tft->setTextSize(2);
    tft->setCursor(10, 100);
    tft->println("Display Test");
    
    // Draw border
    tft->drawRect(0, 0, tft->width(), tft->height(), ILI9341_WHITE);
    
    Serial.println("Test pattern complete");
}

/*******************************************************************************
 * LVGL Callback Handler
 ******************************************************************************/

/**
 * Callback for LVGL flush completion
 */
static bool notify_lvgl_flush_ready(esp_lcd_panel_io_handle_t panel_io, 
                                  esp_lcd_panel_io_event_data_t *edata, 
                                  void *user_ctx) {
    lv_disp_drv_t* disp_drv = (lv_disp_drv_t*)user_ctx;
    if (disp_drv) {
        lv_disp_flush_ready(disp_drv);
    }
    return false;
}

/*******************************************************************************
 * Lilygo S3 Display Driver Implementation
 ******************************************************************************/

/**
 * Constructor for Lilygo S3 display driver
 */
LilygoS3Driver::LilygoS3Driver() 
    : panelHandle(nullptr)
    , ioHandle(nullptr)
    , disp(nullptr) {
}

/**
 * Initialize the Lilygo S3 display
 * @return true if initialization successful, false otherwise
 */
bool LilygoS3Driver::begin() {
    if (initialized) return true;

    // Power and pin initialization
    pinMode(PIN_POWER_ON, OUTPUT);
    digitalWrite(PIN_POWER_ON, HIGH);

    pinMode(PIN_LCD_RD, OUTPUT);
    digitalWrite(PIN_LCD_RD, HIGH);

    // Initialize display hardware
    initBus();
    initPanel();

    // Configure backlight
    ledcSetup(0, 10000, 8);
    ledcAttachPin(PIN_LCD_BL, 0);
    ledcWrite(0, 255);

    // Initialize LVGL display buffer
    static lv_color_t* buf = (lv_color_t*)heap_caps_malloc(
        LVGL_LCD_BUF_SIZE * sizeof(lv_color_t), 
        MALLOC_CAP_DMA | MALLOC_CAP_INTERNAL
    );
    if (!buf) {
        return false;
    }

    static lv_disp_draw_buf_t draw_buf;
    lv_disp_draw_buf_init(&draw_buf, buf, NULL, LVGL_LCD_BUF_SIZE);

    // Configure LVGL display driver
    static lv_disp_drv_t disp_drv;
    lv_disp_drv_init(&disp_drv);
    disp_drv.hor_res = width();
    disp_drv.ver_res = height();
    disp_drv.flush_cb = [](lv_disp_drv_t * drv, const lv_area_t* area, lv_color_t* px_map) {
        LilygoS3Driver* driver = (LilygoS3Driver*)drv->user_data;
        if (driver) {
            driver->flush(area, (uint8_t*)px_map);
        }
    };
    disp_drv.draw_buf = &draw_buf;
    disp_drv.user_data = this;
    
    // Register display driver
    disp = lv_disp_drv_register(&disp_drv);
    if (!disp) {
        return false;
    }

    initialized = true;
    return true;
}

/**
 * Initialize the display bus
 */
void LilygoS3Driver::initBus() {
    esp_lcd_i80_bus_handle_t i80_bus = nullptr;
    
    // Configure I80 bus interface
    esp_lcd_i80_bus_config_t bus_config = {
        .dc_gpio_num = PIN_LCD_DC,
        .wr_gpio_num = PIN_LCD_WR,
        .clk_src = LCD_CLK_SRC_PLL160M,
        .data_gpio_nums = {
            PIN_LCD_D0, PIN_LCD_D1, PIN_LCD_D2, PIN_LCD_D3,
            PIN_LCD_D4, PIN_LCD_D5, PIN_LCD_D6, PIN_LCD_D7,
        },
        .bus_width = 8,
        .max_transfer_bytes = LVGL_LCD_BUF_SIZE * sizeof(uint16_t),
    };
    ESP_ERROR_CHECK(esp_lcd_new_i80_bus(&bus_config, &i80_bus));

    // Configure panel IO interface
    esp_lcd_panel_io_i80_config_t io_config = {
        .cs_gpio_num = PIN_LCD_CS,
        .pclk_hz = EXAMPLE_LCD_PIXEL_CLOCK_HZ,
        .trans_queue_depth = 20,
        .on_color_trans_done = notify_lvgl_flush_ready,
        .user_ctx = disp,
        .lcd_cmd_bits = 8,
        .lcd_param_bits = 8,
        .dc_levels = {
            .dc_idle_level = 0,
            .dc_cmd_level = 0,
            .dc_dummy_level = 0,
            .dc_data_level = 1,
        }
    };
    ESP_ERROR_CHECK(esp_lcd_new_panel_io_i80(i80_bus, &io_config, &ioHandle));
}

/**
 * Initialize the display panel
 */
void LilygoS3Driver::initPanel() {
    esp_lcd_panel_dev_config_t panel_config = {
        .reset_gpio_num = PIN_LCD_RES,
        .color_space = ESP_LCD_COLOR_SPACE_RGB,
        .bits_per_pixel = 16,
    };
    
    // Initialize ST7789V panel
    ESP_ERROR_CHECK(esp_lcd_new_panel_st7789(ioHandle, &panel_config, &panelHandle));
    ESP_ERROR_CHECK(esp_lcd_panel_reset(panelHandle));
    ESP_ERROR_CHECK(esp_lcd_panel_init(panelHandle));
    ESP_ERROR_CHECK(esp_lcd_panel_invert_color(panelHandle, true));
    ESP_ERROR_CHECK(esp_lcd_panel_swap_xy(panelHandle, true));
    ESP_ERROR_CHECK(esp_lcd_panel_mirror(panelHandle, false, true));
    ESP_ERROR_CHECK(esp_lcd_panel_set_gap(panelHandle, 0, 35));

    // ST7789V initialization commands
    const lcd_cmd_t lcd_st7789v[] = {
        {0x11, {0}, 0 | 0x80},
        {0x3A, {0X05}, 1},
        {0xB2, {0X0B, 0X0B, 0X00, 0X33, 0X33}, 5},
        {0xB7, {0X75}, 1},
        {0xBB, {0X28}, 1},
        {0xC0, {0X2C}, 1},
        {0xC2, {0X01}, 1},
        {0xC3, {0X1F}, 1},
        {0xC6, {0X13}, 1},
        {0xD0, {0XA7}, 1},
        {0xD0, {0XA4, 0XA1}, 2},
        {0xD6, {0XA1}, 1},
        {0xE0, {0XF0, 0X05, 0X0A, 0X06, 0X06, 0X03, 0X2B, 0X32, 0X43, 0X36, 0X11, 0X10, 0X2B, 0X32}, 14},
        {0xE1, {0XF0, 0X08, 0X0C, 0X0B, 0X09, 0X24, 0X2B, 0X22, 0X43, 0X38, 0X15, 0X16, 0X2F, 0X37}, 14}
    };

    // Send initialization sequence
    for (const auto& cmd : lcd_st7789v) {
        esp_lcd_panel_io_tx_param(ioHandle, cmd.cmd, cmd.data, cmd.len & 0x7f);
        if (cmd.len & 0x80) {
            delay(120);
        }
    }
}

/**
 * Set display brightness
 * @param brightness Brightness level (0-255)
 */
void LilygoS3Driver::setBrightness(uint8_t brightness) {
    ledcWrite(0, brightness);
}

/**
 * Flush display buffer to screen
 * @param area Display area to update
 * @param pixels Pixel data buffer
 */
void LilygoS3Driver::flush(const lv_area_t* area, uint8_t* pixels) {
    int offsetx1 = area->x1;
    int offsetx2 = area->x2;
    int offsety1 = area->y1;
    int offsety2 = area->y2;

    // Update display contents
    esp_lcd_panel_draw_bitmap(panelHandle, offsetx1, offsety1, 
                            offsetx2 + 1, offsety2 + 1, pixels);
    
    // Signal completion to LVGL
    lv_disp_flush_ready(lv_disp_get_default()->driver);
}