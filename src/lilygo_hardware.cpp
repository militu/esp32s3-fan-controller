#include "lilygo_hardware.h"
#include <driver/ledc.h>

static bool lvgl_flush_ready_cb(esp_lcd_panel_io_handle_t panel_io, esp_lcd_panel_io_event_data_t *edata, void *user_ctx) {
    lv_disp_drv_t* disp_drv = (lv_disp_drv_t*)user_ctx;
    lv_disp_flush_ready(disp_drv);
    return false;
}

const DisplayHardware::DisplayConfig LilygoHardware::config = {
    .width = 320,
    .height = 170,
    .bufferSize = 320 * 170,
};

LilygoHardware::LilygoHardware() : panelHandle(nullptr), ioHandle(nullptr) {}

bool LilygoHardware::initialize() {
    pinMode(Pins::POWER, OUTPUT);
    digitalWrite(Pins::POWER, HIGH);
    pinMode(Pins::RD, OUTPUT);
    digitalWrite(Pins::RD, HIGH);

    if (!initializeBus() || !initializePanel() || !configureDisplay()) {
        return false;
    }

    ledcSetup(0, 10000, 8);
    ledcAttachPin(Pins::BL, 0);
    ledcWrite(0, 255);

    lv_init();

    static lv_disp_draw_buf_t draw_buf;
    static lv_color_t *buf1 = (lv_color_t *)heap_caps_malloc(config.width * 40 * sizeof(lv_color_t), MALLOC_CAP_DMA);

    lv_disp_draw_buf_init(&draw_buf, buf1, nullptr, config.width * 40);

    lv_disp_drv_init(&disp_drv);
    disp_drv.hor_res = config.width;
    disp_drv.ver_res = config.height;
    disp_drv.flush_cb = [](lv_disp_drv_t *drv, const lv_area_t *area, lv_color_t *color_p) {
        LilygoHardware *instance = static_cast<LilygoHardware *>(drv->user_data);
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

void LilygoHardware::setPower(bool on) {
    on ? powerOn() : powerOff();
}

void LilygoHardware::setBrightness(uint8_t level) {
    ledcWrite(0, level);
}

void LilygoHardware::flush(const Rect& area, lv_color_t* pixels) {
    if (!panelHandle) return;
    esp_lcd_panel_draw_bitmap(panelHandle, area.x1, area.y1, area.x2 + 1, area.y2 + 1, pixels);
}

void LilygoHardware::powerOn() {
    if (!panelHandle) return;
    digitalWrite(Pins::BL, HIGH);
    esp_lcd_panel_disp_on_off(panelHandle, true);
}

void LilygoHardware::powerOff() {
    if (!panelHandle) return;
    esp_lcd_panel_disp_on_off(panelHandle, false);
    digitalWrite(Pins::BL, LOW);
}

void LilygoHardware::enterSleep() {
    sendCommand(PanelCommands::SLPIN);
    delay(5);
}

void LilygoHardware::wakeFromSleep() {
    sendCommand(PanelCommands::SLPOUT);
    delay(120);
}

void LilygoHardware::sendCommand(uint8_t cmd) {
    if (ioHandle) {
        esp_lcd_panel_io_tx_param(ioHandle, cmd, nullptr, 0);
    }
}

bool LilygoHardware::initializeBus() {
    esp_lcd_i80_bus_handle_t i80_bus;

    esp_lcd_i80_bus_config_t bus_config = {
        .dc_gpio_num = Pins::DC,
        .wr_gpio_num = Pins::WR,
        .clk_src = LCD_CLK_SRC_PLL160M,
        .data_gpio_nums = {
            Pins::D0, Pins::D1, Pins::D2, Pins::D3,
            Pins::D4, Pins::D5, Pins::D6, Pins::D7,
        },
        .bus_width = 8,
        .max_transfer_bytes = config.bufferSize * sizeof(uint16_t),
    };

    if (esp_lcd_new_i80_bus(&bus_config, &i80_bus) != ESP_OK) {
        return false;
    }

    esp_lcd_panel_io_i80_config_t io_config = {
        .cs_gpio_num = Pins::CS,
        .pclk_hz = 20 * 1000 * 1000,
        .trans_queue_depth = 10,
        .on_color_trans_done = lvgl_flush_ready_cb,
        .user_ctx = &disp_drv,
        .lcd_cmd_bits = 8,
        .lcd_param_bits = 8,
        .dc_levels = {
            .dc_idle_level = 0,
            .dc_cmd_level = 0,
            .dc_dummy_level = 0,
            .dc_data_level = 1,
        }
    };

    return esp_lcd_new_panel_io_i80(i80_bus, &io_config, &ioHandle) == ESP_OK;
}

bool LilygoHardware::initializePanel() {
    esp_lcd_panel_dev_config_t panel_config = {
        .reset_gpio_num = Pins::RES,
        .color_space = ESP_LCD_COLOR_SPACE_RGB,
        .bits_per_pixel = 16,
    };

    if (esp_lcd_new_panel_st7789(ioHandle, &panel_config, &panelHandle) != ESP_OK) {
        return false;
    }

    esp_lcd_panel_reset(panelHandle);
    esp_lcd_panel_init(panelHandle);
    esp_lcd_panel_invert_color(panelHandle, true);
    esp_lcd_panel_swap_xy(panelHandle, true);
    esp_lcd_panel_mirror(panelHandle, false, true);
    esp_lcd_panel_set_gap(panelHandle, 0, 35);

    return true;
}

bool LilygoHardware::configureDisplay() {
    // Here you can add any specific commands to configure the display further.
    const uint8_t init_cmds[] = {
        0x11, 0,
        0x3A, 1, 0x05,
        0xB2, 5, 0x0C, 0x0C, 0x00, 0x33, 0x33,
        0xB7, 1, 0x75,
        0xBB, 1, 0x28,
        0xC0, 1, 0x2C,
        0xC2, 1, 0x01,
        0xC3, 1, 0x1F,
        0xC6, 1, 0x13,
        0xD0, 2, 0xA4, 0xA1,
        0xE0, 14, 0xD0, 0x08, 0x11, 0x08, 0x0C, 0x15, 0x39, 0x33, 0x50, 0x36, 0x13, 0x14, 0x29, 0x2D,
        0xE1, 14, 0xD0, 0x08, 0x10, 0x08, 0x06, 0x06, 0x39, 0x44, 0x51, 0x0B, 0x16, 0x14, 0x2F, 0x31,
        0x29, 0
    };

    for (size_t i = 0; i < sizeof(init_cmds); ) {
        uint8_t cmd = init_cmds[i++];
        uint8_t len = init_cmds[i++];
        esp_lcd_panel_io_tx_param(ioHandle, cmd, len ? &init_cmds[i] : nullptr, len);
        i += len;

        if (cmd == 0x11 || cmd == 0x29) {
            delay(120);
        }
    }

    return true;
}