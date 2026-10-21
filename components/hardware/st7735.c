#include "st7735.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <math.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "driver/spi_master.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "esp_heap_caps.h"

static const char *TAG = "ST7735";

// ST7735 SPI Commands
#define ST7735_NOP     0x00
#define ST7735_SWRESET 0x01
#define ST7735_SLPIN   0x10
#define ST7735_SLPOUT  0x11
#define ST7735_NORON   0x13
#define ST7735_INVOFF  0x20
#define ST7735_INVON   0x21
#define ST7735_DISPOFF 0x28
#define ST7735_DISPON  0x29
#define ST7735_CASET   0x2A
#define ST7735_RASET   0x2B
#define ST7735_RAMWR   0x2C
#define ST7735_COLMOD  0x3A
#define ST7735_MADCTL  0x36

#define ST7735_FRMCTR1 0xB1
#define ST7735_FRMCTR2 0xB2
#define ST7735_FRMCTR3 0xB3
#define ST7735_INVCTR  0xB4
#define ST7735_PWCTR1  0xC0
#define ST7735_PWCTR2  0xC1
#define ST7735_PWCTR3  0xC2
#define ST7735_PWCTR4  0xC3
#define ST7735_PWCTR5  0xC4
#define ST7735_VMCTR1  0xC5

#define ST7735_GMCTRP1 0xE0
#define ST7735_GMCTRN1 0xE1

#define ST77XX_MADCTL_MY  0x80
#define ST77XX_MADCTL_MX  0x40
#define ST77XX_MADCTL_MV  0x20
#define ST77XX_MADCTL_ML  0x10
#define ST77XX_MADCTL_RGB 0x00
#define ST7735_MADCTL_BGR 0x08

#define DELAY_FLAG 0x80

// Standard 5x7 ASCII Font Table (characters 0x20 ' ' to 0x7E '~')
static const uint8_t font_5x7[] = {
    0x00, 0x00, 0x00, 0x00, 0x00, // 32 ' '
    0x00, 0x00, 0x5F, 0x00, 0x00, // 33 '!'
    0x00, 0x07, 0x00, 0x07, 0x00, // 34 '"'
    0x14, 0x7F, 0x14, 0x7F, 0x14, // 35 '#'
    0x24, 0x2A, 0x7F, 0x2A, 0x12, // 36 '$'
    0x23, 0x13, 0x08, 0x64, 0x62, // 37 '%'
    0x36, 0x49, 0x55, 0x22, 0x50, // 38 '&'
    0x00, 0x05, 0x03, 0x00, 0x00, // 39 '''
    0x00, 0x1C, 0x22, 0x41, 0x00, // 40 '('
    0x00, 0x41, 0x22, 0x1C, 0x00, // 41 ')'
    0x08, 0x2A, 0x1C, 0x2A, 0x08, // 42 '*'
    0x08, 0x08, 0x3E, 0x08, 0x08, // 43 '+'
    0x00, 0x50, 0x30, 0x00, 0x00, // 44 ','
    0x08, 0x08, 0x08, 0x08, 0x08, // 45 '-'
    0x00, 0x60, 0x60, 0x00, 0x00, // 46 '.'
    0x20, 0x10, 0x08, 0x04, 0x02, // 47 '/'
    0x3E, 0x51, 0x49, 0x45, 0x3E, // 48 '0'
    0x00, 0x42, 0x7F, 0x40, 0x00, // 49 '1'
    0x42, 0x61, 0x51, 0x49, 0x46, // 50 '2'
    0x21, 0x41, 0x45, 0x4B, 0x31, // 51 '3'
    0x18, 0x14, 0x12, 0x7F, 0x10, // 52 '4'
    0x27, 0x45, 0x45, 0x45, 0x39, // 53 '5'
    0x3C, 0x4A, 0x49, 0x49, 0x30, // 54 '6'
    0x01, 0x71, 0x09, 0x05, 0x03, // 55 '7'
    0x36, 0x49, 0x49, 0x49, 0x36, // 56 '8'
    0x06, 0x49, 0x49, 0x29, 0x1E, // 57 '9'
    0x00, 0x36, 0x36, 0x00, 0x00, // 58 ':'
    0x00, 0x56, 0x36, 0x00, 0x00, // 59 ';'
    0x00, 0x08, 0x14, 0x22, 0x41, // 60 '<'
    0x14, 0x14, 0x14, 0x14, 0x14, // 61 '='
    0x41, 0x22, 0x14, 0x08, 0x00, // 62 '>'
    0x02, 0x01, 0x51, 0x09, 0x06, // 63 '?'
    0x32, 0x49, 0x79, 0x41, 0x3E, // 64 '@'
    0x7E, 0x11, 0x11, 0x11, 0x7E, // 65 'A'
    0x7F, 0x49, 0x49, 0x49, 0x36, // 66 'B'
    0x3E, 0x41, 0x41, 0x41, 0x22, // 67 'C'
    0x7F, 0x41, 0x41, 0x22, 0x1C, // 68 'D'
    0x7F, 0x49, 0x49, 0x49, 0x41, // 69 'E'
    0x7F, 0x09, 0x09, 0x01, 0x01, // 70 'F'
    0x3E, 0x41, 0x41, 0x51, 0x32, // 71 'G'
    0x7F, 0x08, 0x08, 0x08, 0x7F, // 72 'H'
    0x00, 0x41, 0x7F, 0x41, 0x00, // 73 'I'
    0x20, 0x40, 0x41, 0x3F, 0x01, // 74 'J'
    0x7F, 0x08, 0x14, 0x22, 0x41, // 75 'K'
    0x7F, 0x40, 0x40, 0x40, 0x40, // 76 'L'
    0x7F, 0x02, 0x04, 0x02, 0x7F, // 77 'M'
    0x7F, 0x04, 0x08, 0x10, 0x7F, // 78 'N'
    0x3E, 0x41, 0x41, 0x41, 0x3E, // 79 'O'
    0x7F, 0x09, 0x09, 0x09, 0x06, // 80 'P'
    0x3E, 0x41, 0x51, 0x21, 0x5E, // 81 'Q'
    0x7F, 0x09, 0x19, 0x29, 0x46, // 82 'R'
    0x46, 0x49, 0x49, 0x49, 0x31, // 83 'S'
    0x01, 0x01, 0x7F, 0x01, 0x01, // 84 'T'
    0x3F, 0x40, 0x40, 0x40, 0x3F, // 85 'U'
    0x1F, 0x20, 0x40, 0x20, 0x1F, // 86 'V'
    0x7F, 0x20, 0x18, 0x20, 0x7F, // 87 'W'
    0x63, 0x14, 0x08, 0x14, 0x63, // 88 'X'
    0x03, 0x04, 0x78, 0x04, 0x03, // 89 'Y'
    0x61, 0x51, 0x49, 0x45, 0x43, // 90 'Z'
    0x00, 0x7F, 0x41, 0x41, 0x00, // 91 '['
    0x02, 0x04, 0x08, 0x10, 0x20, // 92 '\'
    0x00, 0x41, 0x41, 0x7F, 0x00, // 93 ']'
    0x04, 0x02, 0x01, 0x02, 0x04, // 94 '^'
    0x40, 0x40, 0x40, 0x40, 0x40, // 95 '_'
    0x00, 0x01, 0x02, 0x04, 0x00, // 96 '`'
    0x20, 0x54, 0x54, 0x54, 0x78, // 97 'a'
    0x7F, 0x48, 0x44, 0x44, 0x38, // 98 'b'
    0x38, 0x44, 0x44, 0x44, 0x20, // 99 'c'
    0x38, 0x44, 0x44, 0x48, 0x7F, // 100 'd'
    0x38, 0x54, 0x54, 0x54, 0x18, // 101 'e'
    0x08, 0x7E, 0x09, 0x01, 0x02, // 102 'f'
    0x08, 0x14, 0x54, 0x54, 0x3C, // 103 'g'
    0x7F, 0x08, 0x04, 0x04, 0x78, // 104 'h'
    0x00, 0x44, 0x7D, 0x40, 0x00, // 105 'i'
    0x20, 0x40, 0x44, 0x3D, 0x00, // 106 'j'
    0x7F, 0x10, 0x28, 0x44, 0x00, // 107 'k'
    0x00, 0x41, 0x7F, 0x40, 0x00, // 108 'l'
    0x7C, 0x04, 0x18, 0x04, 0x78, // 109 'm'
    0x7C, 0x08, 0x04, 0x04, 0x78, // 110 'n'
    0x38, 0x44, 0x44, 0x44, 0x38, // 111 'o'
    0x7C, 0x14, 0x14, 0x14, 0x08, // 112 'p'
    0x08, 0x14, 0x14, 0x18, 0x7C, // 113 'q'
    0x7C, 0x08, 0x04, 0x04, 0x08, // 114 'r'
    0x48, 0x54, 0x54, 0x54, 0x20, // 115 's'
    0x04, 0x3F, 0x44, 0x40, 0x20, // 116 't'
    0x3C, 0x40, 0x40, 0x20, 0x7C, // 117 'u'
    0x1C, 0x20, 0x40, 0x20, 0x1C, // 118 'v'
    0x3C, 0x40, 0x30, 0x40, 0x3C, // 119 'w'
    0x44, 0x28, 0x10, 0x28, 0x44, // 120 'x'
    0x0C, 0x50, 0x50, 0x50, 0x3C, // 121 'y'
    0x44, 0x64, 0x54, 0x4C, 0x44, // 122 'z'
    0x00, 0x08, 0x36, 0x41, 0x00, // 123 '{'
    0x00, 0x00, 0x77, 0x00, 0x00, // 124 '|'
    0x00, 0x41, 0x36, 0x08, 0x00, // 125 '}'
    0x08, 0x08, 0x2A, 0x1C, 0x08  // 126 '~'
};

// Initialization command tables
static const uint8_t Rcmd1[] = {
    15,
    ST7735_SWRESET, DELAY_FLAG, 150,
    ST7735_SLPOUT,  DELAY_FLAG, 255,
    ST7735_FRMCTR1, 3, 0x01, 0x2C, 0x2D,
    ST7735_FRMCTR2, 3, 0x01, 0x2C, 0x2D,
    ST7735_FRMCTR3, 6, 0x01, 0x2C, 0x2D, 0x01, 0x2C, 0x2D,
    ST7735_INVCTR,  1, 0x07,
    ST7735_PWCTR1,  3, 0xA2, 0x02, 0x84,
    ST7735_PWCTR2,  1, 0xC5,
    ST7735_PWCTR3,  2, 0x0A, 0x00,
    ST7735_PWCTR4,  2, 0x8A, 0x2A,
    ST7735_PWCTR5,  2, 0x8A, 0xEE,
    ST7735_VMCTR1,  1, 0x0E,
    ST7735_INVOFF,  0,
    ST7735_MADCTL,  1, 0xC8,
    ST7735_COLMOD,  1, 0x05
};

static const uint8_t Rcmd2green[] = {
    2,
    ST7735_CASET,   4, 0x00, 0x02, 0x00, 0x7F + 0x02,
    ST7735_RASET,   4, 0x00, 0x01, 0x00, 0x9F + 0x01
};

static const uint8_t Rcmd2red[] = {
    2,
    ST7735_CASET,   4, 0x00, 0x00, 0x00, 0x7F,
    ST7735_RASET,   4, 0x00, 0x00, 0x00, 0x9F
};

static const uint8_t Rcmd3[] = {
    4,
    ST7735_GMCTRP1, 16,
      0x02, 0x1C, 0x07, 0x12, 0x37, 0x32, 0x29, 0x2D,
      0x29, 0x25, 0x2B, 0x39, 0x00, 0x01, 0x03, 0x10,
    ST7735_GMCTRN1, 16,
      0x03, 0x1D, 0x07, 0x06, 0x2E, 0x2C, 0x29, 0x2D,
      0x2E, 0x2E, 0x37, 0x3F, 0x00, 0x00, 0x02, 0x10,
    ST7735_NORON,   DELAY_FLAG, 10,
    ST7735_DISPON,  DELAY_FLAG, 100
};

// Internal driver state
static spi_device_handle_t s_spi = NULL;
static st7735_config_t s_cfg;
static int16_t s_width = 160;
static int16_t s_height = 128;
static int16_t s_raw_width = 128;
static int16_t s_raw_height = 160;
static uint8_t s_colstart = 0;
static uint8_t s_rowstart = 0;
static uint8_t s_xstart = 0;
static uint8_t s_ystart = 0;
static uint8_t s_rotation = 1;

#define ST7735_DMA_BUF_LINES   16
#define ST7735_DMA_BUF_PIXELS  (160 * ST7735_DMA_BUF_LINES)
#define ST7735_DMA_BUF_SIZE    (ST7735_DMA_BUF_PIXELS * 2) // 5,120 bytes DMA buffer
static uint8_t *s_line_dma_buf = NULL;

static void st7735_write_cmd(uint8_t cmd)
{
    gpio_set_level((gpio_num_t)s_cfg.dc_pin, 0);
    spi_transaction_t t;
    memset(&t, 0, sizeof(t));
    t.length = 8;
    t.flags = SPI_TRANS_USE_TXDATA;
    t.tx_data[0] = cmd;
    spi_device_polling_transmit(s_spi, &t);
}

static void st7735_write_data(const uint8_t *data, size_t len)
{
    if (len == 0) return;
    gpio_set_level((gpio_num_t)s_cfg.dc_pin, 1);
    spi_transaction_t t;
    memset(&t, 0, sizeof(t));
    t.length = len * 8;
    t.tx_buffer = data;
    spi_device_polling_transmit(s_spi, &t);
}

static void st7735_write_data8(uint8_t d)
{
    gpio_set_level((gpio_num_t)s_cfg.dc_pin, 1);
    spi_transaction_t t;
    memset(&t, 0, sizeof(t));
    t.length = 8;
    t.flags = SPI_TRANS_USE_TXDATA;
    t.tx_data[0] = d;
    spi_device_polling_transmit(s_spi, &t);
}

static void st7735_write_data16(uint16_t d)
{
    gpio_set_level((gpio_num_t)s_cfg.dc_pin, 1);
    spi_transaction_t t;
    memset(&t, 0, sizeof(t));
    t.length = 16;
    t.flags = SPI_TRANS_USE_TXDATA;
    t.tx_data[0] = (d >> 8) & 0xFF;
    t.tx_data[1] = d & 0xFF;
    spi_device_polling_transmit(s_spi, &t);
}

static void execute_cmd_list(const uint8_t *addr)
{
    uint8_t num_cmds = *addr++;
    while (num_cmds--) {
        uint8_t cmd = *addr++;
        st7735_write_cmd(cmd);
        uint8_t num_args = *addr++;
        uint16_t ms = num_args & DELAY_FLAG;
        num_args &= ~DELAY_FLAG;
        if (num_args) {
            st7735_write_data(addr, num_args);
            addr += num_args;
        }
        if (ms) {
            ms = *addr++;
            if (ms == 255) ms = 500;
            vTaskDelay(pdMS_TO_TICKS(ms));
        }
    }
}

void st7735_set_rotation(uint8_t m)
{
    uint8_t madctl = 0;
    s_rotation = m % 4;
    switch (s_rotation) {
        case 0: // Portrait (128x160)
            madctl = (s_cfg.tab_type == ST7735_INIT_BLACKTAB) ?
                     (ST77XX_MADCTL_MX | ST77XX_MADCTL_MY | ST77XX_MADCTL_RGB) :
                     (ST77XX_MADCTL_MX | ST77XX_MADCTL_MY | ST7735_MADCTL_BGR);
            s_width = s_raw_width;
            s_height = s_raw_height;
            s_xstart = s_colstart;
            s_ystart = s_rowstart;
            break;
        case 1: // Landscape (160x128)
            madctl = (s_cfg.tab_type == ST7735_INIT_BLACKTAB) ?
                     (ST77XX_MADCTL_MY | ST77XX_MADCTL_MV | ST77XX_MADCTL_RGB) :
                     (ST77XX_MADCTL_MY | ST77XX_MADCTL_MV | ST7735_MADCTL_BGR);
            s_width = s_raw_height;
            s_height = s_raw_width;
            s_xstart = s_rowstart;
            s_ystart = s_colstart;
            break;
        case 2: // Inverted Portrait
            madctl = (s_cfg.tab_type == ST7735_INIT_BLACKTAB) ?
                     (ST77XX_MADCTL_RGB) :
                     (ST7735_MADCTL_BGR);
            s_width = s_raw_width;
            s_height = s_raw_height;
            s_xstart = s_colstart;
            s_ystart = s_rowstart;
            break;
        case 3: // Inverted Landscape
            madctl = (s_cfg.tab_type == ST7735_INIT_BLACKTAB) ?
                     (ST77XX_MADCTL_MX | ST77XX_MADCTL_MV | ST77XX_MADCTL_RGB) :
                     (ST77XX_MADCTL_MX | ST77XX_MADCTL_MV | ST7735_MADCTL_BGR);
            s_width = s_raw_height;
            s_height = s_raw_width;
            s_xstart = s_rowstart;
            s_ystart = s_colstart;
            break;
    }
    st7735_write_cmd(ST7735_MADCTL);
    st7735_write_data8(madctl);
}

int16_t st7735_get_width(void) { return s_width; }
int16_t st7735_get_height(void) { return s_height; }

void st7735_set_addr_window(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1)
{
    st7735_write_cmd(ST7735_CASET);
    st7735_write_data16(x0 + s_xstart);
    st7735_write_data16(x1 + s_xstart);

    st7735_write_cmd(ST7735_RASET);
    st7735_write_data16(y0 + s_ystart);
    st7735_write_data16(y1 + s_ystart);

    st7735_write_cmd(ST7735_RAMWR);
}

void st7735_draw_pixel(int16_t x, int16_t y, uint16_t color)
{
    if (x < 0 || x >= s_width || y < 0 || y >= s_height) return;
    st7735_set_addr_window(x, y, x, y);
    st7735_write_data16(color);
}

void st7735_draw_fast_h_line(int16_t x, int16_t y, int16_t w, uint16_t color)
{
    if (y < 0 || y >= s_height || w <= 0) return;
    if (x < 0) { w += x; x = 0; }
    if (x + w > s_width) { w = s_width - x; }
    if (w <= 0) return;

    st7735_set_addr_window(x, y, x + w - 1, y);
    uint8_t hi = (color >> 8) & 0xFF;
    uint8_t lo = color & 0xFF;
    size_t chunk = (w > 160) ? 160 : w;
    for (size_t i = 0; i < chunk; i++) {
        s_line_dma_buf[i * 2] = hi;
        s_line_dma_buf[i * 2 + 1] = lo;
    }
    size_t remaining = w;
    while (remaining > 0) {
        size_t to_send = (remaining > chunk) ? chunk : remaining;
        st7735_write_data(s_line_dma_buf, to_send * 2);
        remaining -= to_send;
    }
}

void st7735_draw_fast_v_line(int16_t x, int16_t y, int16_t h, uint16_t color)
{
    if (x < 0 || x >= s_width || h <= 0) return;
    if (y < 0) { h += y; y = 0; }
    if (y + h > s_height) { h = s_height - y; }
    if (h <= 0) return;

    st7735_set_addr_window(x, y, x, y + h - 1);
    uint8_t hi = (color >> 8) & 0xFF;
    uint8_t lo = color & 0xFF;
    size_t chunk = (h > 160) ? 160 : h;
    for (size_t i = 0; i < chunk; i++) {
        s_line_dma_buf[i * 2] = hi;
        s_line_dma_buf[i * 2 + 1] = lo;
    }
    size_t remaining = h;
    while (remaining > 0) {
        size_t to_send = (remaining > chunk) ? chunk : remaining;
        st7735_write_data(s_line_dma_buf, to_send * 2);
        remaining -= to_send;
    }
}

void st7735_fill_rect(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t color)
{
    if (x >= s_width || y >= s_height || w <= 0 || h <= 0) return;
    if (x < 0) { w += x; x = 0; }
    if (y < 0) { h += y; y = 0; }
    if (x + w > s_width) { w = s_width - x; }
    if (y + h > s_height) { h = s_height - y; }
    if (w <= 0 || h <= 0) return;

    st7735_set_addr_window(x, y, x + w - 1, y + h - 1);
    uint8_t hi = (color >> 8) & 0xFF;
    uint8_t lo = color & 0xFF;
    for (int i = 0; i < ST7735_DMA_BUF_PIXELS; i++) {
        s_line_dma_buf[i * 2] = hi;
        s_line_dma_buf[i * 2 + 1] = lo;
    }
    size_t total_pixels = (size_t)w * h;
    while (total_pixels > 0) {
        size_t chunk = (total_pixels > ST7735_DMA_BUF_PIXELS) ? ST7735_DMA_BUF_PIXELS : total_pixels;
        st7735_write_data(s_line_dma_buf, chunk * 2);
        total_pixels -= chunk;
    }
}

void st7735_fill_screen(uint16_t color)
{
    st7735_fill_rect(0, 0, s_width, s_height, color);
}

void st7735_draw_rect(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t color)
{
    st7735_draw_fast_h_line(x, y, w, color);
    st7735_draw_fast_h_line(x, y + h - 1, w, color);
    st7735_draw_fast_v_line(x, y, h, color);
    st7735_draw_fast_v_line(x + w - 1, y, h, color);
}

void st7735_draw_line(int16_t x0, int16_t y0, int16_t x1, int16_t y1, uint16_t color)
{
    int16_t steep = abs(y1 - y0) > abs(x1 - x0);
    if (steep) {
        int16_t t = x0; x0 = y0; y0 = t;
        t = x1; x1 = y1; y1 = t;
    }
    if (x0 > x1) {
        int16_t t = x0; x0 = x1; x1 = t;
        t = y0; y0 = y1; y1 = t;
    }
    int16_t dx = x1 - x0;
    int16_t dy = abs(y1 - y0);
    int16_t err = dx / 2;
    int16_t ystep = (y0 < y1) ? 1 : -1;
    for (; x0 <= x1; x0++) {
        if (steep) {
            st7735_draw_pixel(y0, x0, color);
        } else {
            st7735_draw_pixel(x0, y0, color);
        }
        err -= dy;
        if (err < 0) {
            y0 += ystep;
            err += dx;
        }
    }
}

void st7735_draw_char(int16_t x, int16_t y, char c, uint16_t color, uint16_t bg, uint8_t size)
{
    if (c < 32 || c > 126) c = '?';
    const uint8_t *glyph = &font_5x7[(c - 32) * 5];

    for (int8_t i = 0; i < 5; i++) {
        uint8_t line = glyph[i];
        for (int8_t j = 0; j < 8; j++) {
            if (line & 0x1) {
                if (size == 1) {
                    st7735_draw_pixel(x + i, y + j, color);
                } else {
                    st7735_fill_rect(x + i * size, y + j * size, size, size, color);
                }
            } else if (bg != color) {
                if (size == 1) {
                    st7735_draw_pixel(x + i, y + j, bg);
                } else {
                    st7735_fill_rect(x + i * size, y + j * size, size, size, bg);
                }
            }
            line >>= 1;
        }
    }
}

void st7735_draw_string(int16_t x, int16_t y, const char *str, uint16_t color, uint16_t bg, uint8_t size)
{
    int16_t cursor_x = x;
    int16_t cursor_y = y;

    while (*str) {
        if (*str == '\n') {
            cursor_x = x;
            cursor_y += 8 * size;
        } else if (*str != '\r') {
            st7735_draw_char(cursor_x, cursor_y, *str, color, bg, size);
            cursor_x += 6 * size;
        }
        str++;
    }
}

/* ==================== SCREEN TEST & GRADIENT PATTERNS ==================== */

// Smooth HSV to RGB565 converter
static inline uint16_t hsv_to_rgb565(uint8_t hue)
{
    uint8_t r = 0, g = 0, b = 0;
    uint8_t region = hue / 43;
    uint8_t rem = (hue - (region * 43)) * 6;

    switch (region) {
        case 0: r = 255; g = rem; b = 0; break;
        case 1: r = 255 - rem; g = 255; b = 0; break;
        case 2: r = 0; g = 255; b = rem; break;
        case 3: r = 0; g = 255 - rem; b = 255; break;
        case 4: r = rem; g = 0; b = 255; break;
        default: r = 255; g = 0; b = 255 - rem; break;
    }
    return ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3);
}

void st7735_draw_gradient(uint8_t step)
{
    st7735_set_addr_window(0, 0, s_width - 1, s_height - 1);

    for (int y = 0; y < s_height; y++) {
        for (int x = 0; x < s_width; x++) {
            // Diagonal rainbow gradient calculation
            uint8_t hue = (uint8_t)((x * 255 / s_width) + (y * 128 / s_height) + step);
            uint16_t c = hsv_to_rgb565(hue);
            s_line_dma_buf[x * 2] = (c >> 8) & 0xFF;
            s_line_dma_buf[x * 2 + 1] = c & 0xFF;
        }
        st7735_write_data(s_line_dma_buf, s_width * 2);
    }
}

void st7735_draw_rgb_gradients(void)
{
    st7735_fill_screen(ST7735_BLACK);
    st7735_draw_string(4, 4, "RGB GRADIENT TEST", ST7735_WHITE, ST7735_BLACK, 1);

    int bar_h = 16;
    int y = 20;

    // Red Gradient (0 to 31)
    st7735_set_addr_window(4, y, s_width - 5, y + bar_h - 1);
    for (int row = 0; row < bar_h; row++) {
        for (int x = 0; x < (s_width - 8); x++) {
            uint8_t r = (x * 255) / (s_width - 9);
            uint16_t c = st7735_color565(r, 0, 0);
            s_line_dma_buf[x * 2] = (c >> 8) & 0xFF;
            s_line_dma_buf[x * 2 + 1] = c & 0xFF;
        }
        st7735_write_data(s_line_dma_buf, (s_width - 8) * 2);
    }
    st7735_draw_string(6, y + 4, "RED CHANNEL", ST7735_WHITE, ST7735_RED, 1);
    y += bar_h + 8;

    // Green Gradient (0 to 63)
    st7735_set_addr_window(4, y, s_width - 5, y + bar_h - 1);
    for (int row = 0; row < bar_h; row++) {
        for (int x = 0; x < (s_width - 8); x++) {
            uint8_t g = (x * 255) / (s_width - 9);
            uint16_t c = st7735_color565(0, g, 0);
            s_line_dma_buf[x * 2] = (c >> 8) & 0xFF;
            s_line_dma_buf[x * 2 + 1] = c & 0xFF;
        }
        st7735_write_data(s_line_dma_buf, (s_width - 8) * 2);
    }
    st7735_draw_string(6, y + 4, "GREEN CHANNEL", ST7735_BLACK, ST7735_GREEN, 1);
    y += bar_h + 8;

    // Blue Gradient (0 to 31)
    st7735_set_addr_window(4, y, s_width - 5, y + bar_h - 1);
    for (int row = 0; row < bar_h; row++) {
        for (int x = 0; x < (s_width - 8); x++) {
            uint8_t b = (x * 255) / (s_width - 9);
            uint16_t c = st7735_color565(0, 0, b);
            s_line_dma_buf[x * 2] = (c >> 8) & 0xFF;
            s_line_dma_buf[x * 2 + 1] = c & 0xFF;
        }
        st7735_write_data(s_line_dma_buf, (s_width - 8) * 2);
    }
    st7735_draw_string(6, y + 4, "BLUE CHANNEL", ST7735_WHITE, ST7735_BLUE, 1);
    y += bar_h + 8;

    // White / Grayscale Gradient
    st7735_set_addr_window(4, y, s_width - 5, y + bar_h - 1);
    for (int row = 0; row < bar_h; row++) {
        for (int x = 0; x < (s_width - 8); x++) {
            uint8_t val = (x * 255) / (s_width - 9);
            uint16_t c = st7735_color565(val, val, val);
            s_line_dma_buf[x * 2] = (c >> 8) & 0xFF;
            s_line_dma_buf[x * 2 + 1] = c & 0xFF;
        }
        st7735_write_data(s_line_dma_buf, (s_width - 8) * 2);
    }
    st7735_draw_string(6, y + 4, "GRAYSCALE", ST7735_RED, ST7735_WHITE, 1);
}

void st7735_draw_color_bars(void)
{
    const uint16_t colors[] = {
        ST7735_WHITE, ST7735_YELLOW, ST7735_CYAN, ST7735_GREEN,
        ST7735_MAGENTA, ST7735_RED, ST7735_BLUE, ST7735_BLACK
    };
    int num_bars = 8;
    int bar_w = s_width / num_bars;

    for (int i = 0; i < num_bars; i++) {
        int x = i * bar_w;
        int w = (i == num_bars - 1) ? (s_width - x) : bar_w;
        st7735_fill_rect(x, 0, w, s_height - 24, colors[i]);
    }

    // Bottom info band
    st7735_fill_rect(0, s_height - 24, s_width, 24, ST7735_DARKGRAY);
    st7735_draw_string(10, s_height - 18, "SMPTE COLOR BARS", ST7735_WHITE, ST7735_DARKGRAY, 1);
}

void st7735_draw_pixel_test_pattern(void)
{
    st7735_fill_screen(ST7735_BLACK);

    // 1-pixel outer boundary to check screen offset alignment
    st7735_draw_rect(0, 0, s_width, s_height, ST7735_RED);
    st7735_draw_rect(1, 1, s_width - 2, s_height - 2, ST7735_GREEN);
    st7735_draw_rect(2, 2, s_width - 4, s_height - 4, ST7735_BLUE);

    // Center Crosshair
    int cx = s_width / 2;
    int cy = s_height / 2;
    st7735_draw_fast_h_line(cx - 15, cy, 31, ST7735_YELLOW);
    st7735_draw_fast_v_line(cx, cy - 15, 31, ST7735_YELLOW);

    // 4 Corner 8x8 checkerboards
    for (int dy = 0; dy < 8; dy++) {
        for (int dx = 0; dx < 8; dx++) {
            uint16_t c = ((dx + dy) % 2 == 0) ? ST7735_WHITE : ST7735_BLACK;
            st7735_draw_pixel(3 + dx, 3 + dy, c);                           // Top-Left
            st7735_draw_pixel(s_width - 11 + dx, 3 + dy, c);               // Top-Right
            st7735_draw_pixel(3 + dx, s_height - 11 + dy, c);              // Bottom-Left
            st7735_draw_pixel(s_width - 11 + dx, s_height - 11 + dy, c);   // Bottom-Right
        }
    }

    // Text Information
    st7735_draw_string(24, 16, "ST7735 160x128", ST7735_WHITE, ST7735_BLACK, 1);
    st7735_draw_string(24, 28, "PIXEL ALIGNMENT", ST7735_CYAN, ST7735_BLACK, 1);
    st7735_draw_string(14, 75, "CS:11  DC:18  RST:17", ST7735_YELLOW, ST7735_BLACK, 1);
    st7735_draw_string(14, 87, "MOSI:15   SCLK:16", ST7735_YELLOW, ST7735_BLACK, 1);
    st7735_draw_string(24, 102, "ALL PIXELS OK", ST7735_GREEN, ST7735_BLACK, 1);
}

void st7735_draw_dead_pixel_test(uint16_t color, const char *name)
{
    st7735_fill_screen(color);
    uint16_t txt_color = (color == ST7735_WHITE || color == ST7735_YELLOW || color == ST7735_GREEN) ? ST7735_BLACK : ST7735_WHITE;
    st7735_draw_string(10, 10, name, txt_color, color, 1);
    st7735_draw_string(10, s_height - 20, "STUCK/DEAD PIXEL TEST", txt_color, color, 1);
}

void st7735_show_status(const char *header, const char *line1, const char *line2, uint16_t color)
{
    st7735_fill_screen(ST7735_BLACK);
    st7735_draw_rect(0, 0, s_width, s_height, color);
    st7735_draw_rect(1, 1, s_width - 2, s_height - 2, color);

    st7735_fill_rect(2, 2, s_width - 4, 22, color);
    uint16_t header_txt = (color == ST7735_WHITE || color == ST7735_YELLOW) ? ST7735_BLACK : ST7735_WHITE;
    st7735_draw_string(8, 8, header, header_txt, color, 1);

    if (line1) st7735_draw_string(8, 40, line1, ST7735_WHITE, ST7735_BLACK, 1);
    if (line2) st7735_draw_string(8, 60, line2, ST7735_CYAN, ST7735_BLACK, 1);
}

static void st7735_test_task_entry(void *pvParameters)
{
    ESP_LOGI(TAG, "Starting Screen Test Sequence...");

    while (1) {
        // 1. Pixel Alignment & Border Test (5 seconds)
        ESP_LOGI(TAG, "[Pattern 1/5] Pixel Alignment & Grid Border");
        st7735_draw_pixel_test_pattern();
        vTaskDelay(pdMS_TO_TICKS(4000));

        // 2. SMPTE Color Bars (4 seconds)
        ESP_LOGI(TAG, "[Pattern 2/5] SMPTE Color Bars");
        st7735_draw_color_bars();
        vTaskDelay(pdMS_TO_TICKS(3500));

        // 3. RGB Channel Gradients (4 seconds)
        ESP_LOGI(TAG, "[Pattern 3/5] Primary Channel Gradients");
        st7735_draw_rgb_gradients();
        vTaskDelay(pdMS_TO_TICKS(3500));

        // 4. Dead Pixel Inspection (Cycle pure Red, Green, Blue, White, Black)
        ESP_LOGI(TAG, "[Pattern 4/5] Solid Color Dead Pixel Checks");
        st7735_draw_dead_pixel_test(ST7735_RED, "SOLID RED (0xF800)");
        vTaskDelay(pdMS_TO_TICKS(1500));
        st7735_draw_dead_pixel_test(ST7735_GREEN, "SOLID GREEN (0x07E0)");
        vTaskDelay(pdMS_TO_TICKS(1500));
        st7735_draw_dead_pixel_test(ST7735_BLUE, "SOLID BLUE (0x001F)");
        vTaskDelay(pdMS_TO_TICKS(1500));
        st7735_draw_dead_pixel_test(ST7735_WHITE, "SOLID WHITE (0xFFFF)");
        vTaskDelay(pdMS_TO_TICKS(1500));
        st7735_draw_dead_pixel_test(ST7735_BLACK, "SOLID BLACK (0x0000)");
        vTaskDelay(pdMS_TO_TICKS(1500));

        // 5. Dynamic Moving Rainbow Gradient (Smooth continuous animation for ~8 seconds)
        ESP_LOGI(TAG, "[Pattern 5/5] Dynamic Flowing Rainbow Gradient");
        for (int frame = 0; frame < 120; frame++) {
            st7735_draw_gradient((uint8_t)(frame * 3));
            vTaskDelay(pdMS_TO_TICKS(40));
        }
    }
}

void st7735_start_test_task(void)
{
    xTaskCreatePinnedToCore(st7735_test_task_entry, "tft_test_task", 4096, NULL, 5, NULL, 1);
}

void st7735_set_offset(uint8_t colstart, uint8_t rowstart)
{
    s_colstart = colstart;
    s_rowstart = rowstart;
    st7735_set_rotation(s_rotation);
}

void st7735_clear_full_ram(uint16_t color)
{
    st7735_write_cmd(ST7735_CASET);
    st7735_write_data16(0);
    st7735_write_data16(131);

    st7735_write_cmd(ST7735_RASET);
    st7735_write_data16(0);
    st7735_write_data16(161);

    st7735_write_cmd(ST7735_RAMWR);

    uint8_t hi = (color >> 8) & 0xFF;
    uint8_t lo = color & 0xFF;
    for (int i = 0; i < ST7735_DMA_BUF_PIXELS; i++) {
        s_line_dma_buf[i * 2] = hi;
        s_line_dma_buf[i * 2 + 1] = lo;
    }
    size_t total_pixels = 132 * 162;
    while (total_pixels > 0) {
        size_t chunk = (total_pixels > ST7735_DMA_BUF_PIXELS) ? ST7735_DMA_BUF_PIXELS : total_pixels;
        st7735_write_data(s_line_dma_buf, chunk * 2);
        total_pixels -= chunk;
    }
}

esp_err_t st7735_init(const st7735_config_t *cfg)
{
    if (!cfg) return ESP_ERR_INVALID_ARG;
    memcpy(&s_cfg, cfg, sizeof(st7735_config_t));

    ESP_LOGI(TAG, "Initializing ST7735 TFT SPI Display...");
    ESP_LOGI(TAG, "Pins: CS=%d, RST=%d, DC=%d, MOSI=%d, SCLK=%d",
             s_cfg.cs_pin, s_cfg.rst_pin, s_cfg.dc_pin, s_cfg.mosi_pin, s_cfg.sclk_pin);

    // Allocate 16-line (5120 bytes) DMA buffer for high-throughput SPI transfers
    s_line_dma_buf = (uint8_t *)heap_caps_malloc(ST7735_DMA_BUF_SIZE, MALLOC_CAP_DMA | MALLOC_CAP_INTERNAL);
    if (!s_line_dma_buf) {
        ESP_LOGE(TAG, "Failed to allocate DMA buffer for ST7735");
        return ESP_ERR_NO_MEM;
    }

    // Configure DC and RST GPIOs
    gpio_config_t io_conf = {
        .intr_type = GPIO_INTR_DISABLE,
        .mode = GPIO_MODE_OUTPUT,
        .pin_bit_mask = (1ULL << s_cfg.dc_pin),
        .pull_down_en = 0,
        .pull_up_en = 0,
    };
    if (s_cfg.rst_pin >= 0) {
        io_conf.pin_bit_mask |= (1ULL << s_cfg.rst_pin);
    }
    gpio_config(&io_conf);

    // Initialize SPI Bus (SPI2_HOST)
    spi_bus_config_t buscfg = {
        .mosi_io_num = s_cfg.mosi_pin,
        .miso_io_num = -1,
        .sclk_io_num = s_cfg.sclk_pin,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = 160 * 128 * 2 + 16,
    };
    esp_err_t ret = spi_bus_initialize(SPI2_HOST, &buscfg, SPI_DMA_CH_AUTO);
    if (ret != ESP_OK && ret != ESP_ERR_INVALID_STATE) {
        ESP_LOGE(TAG, "spi_bus_initialize failed: %s", esp_err_to_name(ret));
        return ret;
    }

    // Attach ST7735 to SPI bus
    spi_device_interface_config_t devcfg = {
        .clock_speed_hz = 20 * 1000 * 1000, // 20 MHz
        .mode = 0,                           // SPI Mode 0
        .spics_io_num = s_cfg.cs_pin,
        .queue_size = 7,
        .flags = 0,
    };
    ret = spi_bus_add_device(SPI2_HOST, &devcfg, &s_spi);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "spi_bus_add_device failed: %s", esp_err_to_name(ret));
        return ret;
    }

    // Hardware Reset sequence
    if (s_cfg.rst_pin >= 0) {
        gpio_set_level((gpio_num_t)s_cfg.rst_pin, 1);
        vTaskDelay(pdMS_TO_TICKS(10));
        gpio_set_level((gpio_num_t)s_cfg.rst_pin, 0);
        vTaskDelay(pdMS_TO_TICKS(20));
        gpio_set_level((gpio_num_t)s_cfg.rst_pin, 1);
        vTaskDelay(pdMS_TO_TICKS(150));
    }

    // Run ST7735 Initialization command sequence
    execute_cmd_list(Rcmd1);

    if (s_cfg.tab_type == ST7735_INIT_GREENTAB) {
        execute_cmd_list(Rcmd2green);
        s_colstart = 2;
        s_rowstart = 1;
    } else if (s_cfg.tab_type == ST7735_INIT_144GREENTAB) {
        s_raw_height = s_height = 128;
        s_raw_width = s_width = 128;
        s_colstart = 2;
        s_rowstart = 3;
    } else {
        execute_cmd_list(Rcmd2red);
        s_colstart = 0;
        s_rowstart = 0;
    }

    // Custom offset override if specified in config
    if (s_cfg.colstart >= 0) s_colstart = (uint8_t)s_cfg.colstart;
    if (s_cfg.rowstart >= 0) s_rowstart = (uint8_t)s_cfg.rowstart;

    execute_cmd_list(Rcmd3);

    // Blank out entire 132x162 controller RAM to eliminate static noise/garbage border
    st7735_clear_full_ram(ST7735_BLACK);

    // Apply rotation and coordinate window
    st7735_set_rotation(s_cfg.rotation);

    // Initial clear screen
    st7735_fill_screen(ST7735_BLACK);

    ESP_LOGI(TAG, "ST7735 Initialized (%dx%d, colstart=%d, rowstart=%d)",
             s_width, s_height, s_colstart, s_rowstart);
    return ESP_OK;
}

/* ==================== HARDWARE BENCHMARK SUITE IMPLEMENTATION ==================== */

st7735_benchmark_results_t st7735_run_benchmark(void)
{
    st7735_benchmark_results_t res;
    memset(&res, 0, sizeof(res));

    res.spi_clock_hz = 20000000; // 20 MHz
    res.screen_width = (uint32_t)s_width;
    res.screen_height = (uint32_t)s_height;
    res.total_pixels = (uint32_t)(s_width * s_height);
    res.bytes_per_frame = res.total_pixels * 2;
    // Theoretical max FPS = SPI clock (bits/sec) / (bytes_per_frame * 8 bits)
    res.theoretical_max_fps = (float)res.spi_clock_hz / (float)(res.bytes_per_frame * 8);

    ESP_LOGI(TAG, "=======================================================");
    ESP_LOGI(TAG, " Starting ST7735 Hardware Benchmark Execution...");
    ESP_LOGI(TAG, " Target: %dx%d (%lu px) @ %.1f MHz SPI (Limit: %.1f FPS)",
             s_width, s_height, (unsigned long)res.total_pixels,
             (float)res.spi_clock_hz / 1e6f, res.theoretical_max_fps);
    ESP_LOGI(TAG, "=======================================================");

    // -------------------------------------------------------------
    // Test 1: Full-Screen Solid Color Fills (100 frames)
    // -------------------------------------------------------------
    const uint16_t test_colors[5] = { ST7735_RED, ST7735_GREEN, ST7735_BLUE, ST7735_WHITE, ST7735_BLACK };
    const int solid_frames = 100;
    int64_t t0 = esp_timer_get_time();
    for (int i = 0; i < solid_frames; i++) {
        st7735_fill_screen(test_colors[i % 5]);
    }
    int64_t t1 = esp_timer_get_time();
    int64_t solid_time_us = t1 - t0;

    res.solid_fill_fps = (float)(solid_frames * 1000000ULL) / (float)solid_time_us;
    res.solid_fill_ms = ((float)solid_time_us / (float)solid_frames) / 1000.0f;
    float total_mb = ((float)(solid_frames * res.bytes_per_frame)) / (1024.0f * 1024.0f);
    res.solid_fill_mb_s = total_mb / ((float)solid_time_us / 1000000.0f);
    res.solid_fill_mpx_s = ((float)(solid_frames * res.total_pixels) / 1e6f) / ((float)solid_time_us / 1000000.0f);
    res.bus_efficiency_pct = (res.solid_fill_fps / res.theoretical_max_fps) * 100.0f;
    vTaskDelay(pdMS_TO_TICKS(20));

    // -------------------------------------------------------------
    // Test 2: Dynamic Moving Rainbow Gradient (60 frames)
    // -------------------------------------------------------------
    const int grad_frames = 60;
    t0 = esp_timer_get_time();
    for (int i = 0; i < grad_frames; i++) {
        st7735_draw_gradient((uint8_t)(i * 4));
    }
    t1 = esp_timer_get_time();
    int64_t grad_time_us = t1 - t0;

    res.gradient_fps = (float)(grad_frames * 1000000ULL) / (float)grad_time_us;
    res.gradient_ms = ((float)grad_time_us / (float)grad_frames) / 1000.0f;
    float grad_mb = ((float)(grad_frames * res.bytes_per_frame)) / (1024.0f * 1024.0f);
    res.gradient_mb_s = grad_mb / ((float)grad_time_us / 1000000.0f);
    vTaskDelay(pdMS_TO_TICKS(20));

    // -------------------------------------------------------------
    // Test 3: Filled Rectangles (500 rects)
    // -------------------------------------------------------------
    st7735_fill_screen(ST7735_BLACK);
    const int rect_count = 500;
    t0 = esp_timer_get_time();
    for (int i = 0; i < rect_count; i++) {
        int16_t rx = (i * 37) % (s_width - 25);
        int16_t ry = (i * 23) % (s_height - 25);
        int16_t rw = 10 + (i * 13) % 35;
        int16_t rh = 10 + (i * 17) % 35;
        uint16_t rc = (uint16_t)(i * 31415);
        st7735_fill_rect(rx, ry, rw, rh, rc);
    }
    t1 = esp_timer_get_time();
    res.fill_rects_per_sec = (float)(rect_count * 1000000ULL) / (float)(t1 - t0);
    vTaskDelay(pdMS_TO_TICKS(20));

    // -------------------------------------------------------------
    // Test 4: Outline Rectangles (500 rects)
    // -------------------------------------------------------------
    st7735_fill_screen(ST7735_BLACK);
    t0 = esp_timer_get_time();
    for (int i = 0; i < rect_count; i++) {
        int16_t rx = (i * 37) % (s_width - 25);
        int16_t ry = (i * 23) % (s_height - 25);
        int16_t rw = 10 + (i * 13) % 35;
        int16_t rh = 10 + (i * 17) % 35;
        uint16_t rc = (uint16_t)(i * 14142);
        st7735_draw_rect(rx, ry, rw, rh, rc);
    }
    t1 = esp_timer_get_time();
    res.outline_rects_per_sec = (float)(rect_count * 1000000ULL) / (float)(t1 - t0);
    vTaskDelay(pdMS_TO_TICKS(20));

    // -------------------------------------------------------------
    // Test 5: Fast Horizontal & Vertical Lines (1000 lines)
    // -------------------------------------------------------------
    st7735_fill_screen(ST7735_BLACK);
    const int line_count = 1000;
    t0 = esp_timer_get_time();
    for (int i = 0; i < line_count; i++) {
        if (i % 2 == 0) {
            int16_t y = (i * 7) % s_height;
            st7735_draw_fast_h_line(0, y, s_width, (uint16_t)(i * 7919));
        } else {
            int16_t x = (i * 11) % s_width;
            st7735_draw_fast_v_line(x, 0, s_height, (uint16_t)(i * 6271));
        }
    }
    t1 = esp_timer_get_time();
    res.fast_lines_per_sec = (float)(line_count * 1000000ULL) / (float)(t1 - t0);
    vTaskDelay(pdMS_TO_TICKS(20));

    // -------------------------------------------------------------
    // Test 6: Arbitrary Angle Bresenham Lines (1000 lines)
    // -------------------------------------------------------------
    st7735_fill_screen(ST7735_BLACK);
    t0 = esp_timer_get_time();
    for (int i = 0; i < line_count; i++) {
        int16_t x0 = (i * 29) % s_width;
        int16_t y0 = (i * 41) % s_height;
        int16_t x1 = (s_width - 1) - ((i * 31) % s_width);
        int16_t y1 = (s_height - 1) - ((i * 43) % s_height);
        st7735_draw_line(x0, y0, x1, y1, (uint16_t)(i * 5437));
    }
    t1 = esp_timer_get_time();
    res.bresenham_lines_per_sec = (float)(line_count * 1000000ULL) / (float)(t1 - t0);
    vTaskDelay(pdMS_TO_TICKS(20));

    // -------------------------------------------------------------
    // Test 7: Text Rendering Throughput (1000 characters)
    // -------------------------------------------------------------
    st7735_fill_screen(ST7735_BLACK);
    const int char_count = 1000;
    t0 = esp_timer_get_time();
    for (int i = 0; i < char_count; i++) {
        int16_t cx = (i * 6) % (s_width - 6);
        int16_t cy = ((i * 6) / (s_width - 6) * 8) % (s_height - 8);
        char ch = (char)('!' + (i % 94));
        st7735_draw_char(cx, cy, ch, ST7735_YELLOW, ST7735_BLACK, 1);
    }
    t1 = esp_timer_get_time();
    res.chars_per_sec = (float)(char_count * 1000000ULL) / (float)(t1 - t0);
    vTaskDelay(pdMS_TO_TICKS(20));

    // -------------------------------------------------------------
    // Test 8: Single-Pixel Transactions (1000 pixels)
    // -------------------------------------------------------------
    const int px_count = 1000;
    t0 = esp_timer_get_time();
    for (int i = 0; i < px_count; i++) {
        int16_t px = (i * 47) % s_width;
        int16_t py = (i * 59) % s_height;
        st7735_draw_pixel(px, py, ST7735_WHITE);
    }
    t1 = esp_timer_get_time();
    res.single_pixels_per_sec = (float)(px_count * 1000000ULL) / (float)(t1 - t0);

    return res;
}

void st7735_display_benchmark_scorecard(const st7735_benchmark_results_t *res)
{
    st7735_fill_screen(ST7735_BLACK);

    // Outer border & header banner
    st7735_draw_rect(0, 0, s_width, s_height, ST7735_CYAN);
    st7735_fill_rect(2, 2, s_width - 4, 18, ST7735_DARKCYAN);
    st7735_draw_string(8, 7, "ST7735 BENCHMARK HUD", ST7735_WHITE, ST7735_DARKCYAN, 1);

    char buf[48];
    int y = 24;

    // Solid Fill Metric
    snprintf(buf, sizeof(buf), "Fill: %.1f FPS (%.1fms)", res->solid_fill_fps, res->solid_fill_ms);
    st7735_draw_string(6, y, buf, ST7735_GREEN, ST7735_BLACK, 1);
    y += 11;

    // Bus Throughput
    snprintf(buf, sizeof(buf), "SPI : %.2f MB/s (%.0f%%)", res->solid_fill_mb_s, res->bus_efficiency_pct);
    st7735_draw_string(6, y, buf, ST7735_YELLOW, ST7735_BLACK, 1);
    y += 11;

    // Gradient Metric
    snprintf(buf, sizeof(buf), "Grad: %.1f FPS (%.2fMB)", res->gradient_fps, res->gradient_mb_s);
    st7735_draw_string(6, y, buf, ST7735_CYAN, ST7735_BLACK, 1);
    y += 11;

    // FillRects Throughput
    snprintf(buf, sizeof(buf), "Rect: %.0f /sec", res->fill_rects_per_sec);
    st7735_draw_string(6, y, buf, ST7735_WHITE, ST7735_BLACK, 1);
    y += 11;

    // Lines Throughput
    snprintf(buf, sizeof(buf), "Line: %.0f /sec", res->fast_lines_per_sec);
    st7735_draw_string(6, y, buf, ST7735_WHITE, ST7735_BLACK, 1);
    y += 11;

    // Text Rate
    snprintf(buf, sizeof(buf), "Text: %.0f ch/s", res->chars_per_sec);
    st7735_draw_string(6, y, buf, ST7735_ORANGE, ST7735_BLACK, 1);
    y += 13;

    // Visual Efficiency Bar Chart
    st7735_draw_string(6, y, "BUS EFFICIENCY:", ST7735_GRAY, ST7735_BLACK, 1);
    y += 10;
    st7735_draw_rect(6, y, s_width - 12, 10, ST7735_WHITE);
    int bar_max_w = s_width - 16;
    int fill_w = (int)((res->bus_efficiency_pct / 100.0f) * bar_max_w);
    if (fill_w > bar_max_w) fill_w = bar_max_w;
    if (fill_w < 0) fill_w = 0;
    st7735_fill_rect(8, y + 2, fill_w, 6, ST7735_GREEN);
}

void st7735_print_benchmark_report(const st7735_benchmark_results_t *res)
{
    printf("\n"
        "================================================================================\n"
        "             ESP32-S3 ST7735 HARDWARE BENCHMARK & PERFORMANCE METRICS           \n"
        "================================================================================\n"
        "  Target Display        : ST7735 1.8\" SPI TFT (Red Tab / (0,0) Offset)\n"
        "  Active Resolution     : %lu x %lu (%lu pixels)\n"
        "  Color Depth           : 16-bit RGB565 (2 bytes / pixel)\n"
        "  Frame Size            : %lu Bytes (%.2f KB)\n"
        "  SPI Master Bus        : SPI2_HOST (FSPI) with DMA channel AUTO\n"
        "  SPI Bus Clock         : %.1f MHz (%lu Hz)\n"
        "  Theoretical Limit     : %.2f FPS (%.3f MB/s bandwidth cap)\n"
        "--------------------------------------------------------------------------------\n"
        "  TEST CASE                         METRIC              THROUGHPUT / RATE       \n"
        "--------------------------------------------------------------------------------\n"
        "  Full-Screen Solid Fill (100 fr) : %6.1f FPS          %5.2f ms/frame          \n"
        "  Hardware SPI Bus Throughput     : %6.3f MB/s         %5.2f Mpixels/sec       \n"
        "  Hardware Bus Saturation         : %6.1f %% of theoretical maximum limit       \n"
        "--------------------------------------------------------------------------------\n"
        "  Dynamic Rainbow Gradient (60 fr): %6.1f FPS          %5.2f ms/frame          \n"
        "  Real-time Compute + Transmit    : %6.3f MB/s         %5.2f Mpixels/sec       \n"
        "--------------------------------------------------------------------------------\n"
        "  Filled Rectangles (FillRect)    : %8.0f rects/sec                             \n"
        "  Outline Rectangles (DrawRect)   : %8.0f rects/sec                             \n"
        "  Fast Lines (H/V Polling Lines)  : %8.0f lines/sec                             \n"
        "  Arbitrary Lines (Bresenham)     : %8.0f lines/sec                             \n"
        "  Text Rendering (5x7 Font)       : %8.0f chars/sec                             \n"
        "  Single Pixel SPI Transactions   : %8.0f pixels/sec                            \n"
        "================================================================================\n\n",
        (unsigned long)res->screen_width, (unsigned long)res->screen_height, (unsigned long)res->total_pixels,
        (unsigned long)res->bytes_per_frame, (float)res->bytes_per_frame / 1024.0f,
        (float)res->spi_clock_hz / 1e6f, (unsigned long)res->spi_clock_hz,
        res->theoretical_max_fps, (float)res->spi_clock_hz / (8.0f * 1024.0f * 1024.0f),
        res->solid_fill_fps, res->solid_fill_ms,
        res->solid_fill_mb_s, res->solid_fill_mpx_s,
        res->bus_efficiency_pct,
        res->gradient_fps, res->gradient_ms,
        res->gradient_mb_s, res->gradient_fps * (float)res->total_pixels / 1e6f,
        res->fill_rects_per_sec,
        res->outline_rects_per_sec,
        res->fast_lines_per_sec,
        res->bresenham_lines_per_sec,
        res->chars_per_sec,
        res->single_pixels_per_sec
    );
}

static void st7735_benchmark_task_entry(void *pvParameters)
{
    // Small delay to let system initialize cleanly
    vTaskDelay(pdMS_TO_TICKS(500));

    while (1) {
        // 1. Run full benchmark suite
        st7735_benchmark_results_t res = st7735_run_benchmark();

        // 2. Output detailed report to Serial Monitor
        st7735_print_benchmark_report(&res);

        // 3. Display scorecard HUD on LCD for 7 seconds
        st7735_display_benchmark_scorecard(&res);
        vTaskDelay(pdMS_TO_TICKS(7000));

        // 4. Enter continuous live stress test loop with live FPS overlay (10 seconds)
        ESP_LOGI(TAG, "Running live FPS stress monitoring loop (10s)...");
        int64_t loop_start = esp_timer_get_time();
        uint32_t frame_count = 0;
        int64_t last_time = esp_timer_get_time();
        float live_fps = 0.0f;
        char fps_str[32];

        while ((esp_timer_get_time() - loop_start) < 10000000ULL) { // 10 seconds
            int64_t frame_start = esp_timer_get_time();

            // Render animated rainbow gradient
            st7735_draw_gradient((uint8_t)(frame_count * 3));

            int64_t frame_end = esp_timer_get_time();
            int64_t frame_us = frame_end - frame_start;
            frame_count++;

            // Update live FPS every 20 frames
            if (frame_count % 20 == 0) {
                int64_t now = esp_timer_get_time();
                live_fps = (20.0f * 1000000.0f) / (float)(now - last_time);
                last_time = now;
            }

            // Draw HUD overlay in corner: Live FPS and frame latency
            float current_fps = live_fps > 0 ? live_fps : (1000000.0f / (float)frame_us);
            snprintf(fps_str, sizeof(fps_str), "FPS:%.1f %dms", current_fps, (int)(frame_us / 1000));
            st7735_fill_rect(2, 2, 92, 12, ST7735_BLACK);
            st7735_draw_string(4, 4, fps_str, ST7735_GREEN, ST7735_BLACK, 1);

            vTaskDelay(pdMS_TO_TICKS(10)); // Cooperative yield to IDLE task
        }
    }
}

void st7735_start_benchmark_task(void)
{
    xTaskCreatePinnedToCore(st7735_benchmark_task_entry, "tft_bench_task", 6144, NULL, 1, NULL, 1);
}
