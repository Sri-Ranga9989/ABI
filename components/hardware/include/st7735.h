#ifndef ST7735_H
#define ST7735_H

#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

// Default Hardware Pin Mapping for ESP32-S3
#define ST7735_PIN_CS    11
#define ST7735_PIN_RST   17
#define ST7735_PIN_DC    18
#define ST7735_PIN_MOSI  15
#define ST7735_PIN_SCLK  16

// Tab / Panel Types
#define ST7735_INIT_GREENTAB    0x00
#define ST7735_INIT_REDTAB      0x01
#define ST7735_INIT_BLACKTAB    0x02
#define ST7735_INIT_144GREENTAB 0x03
#define ST7735_INIT_MINI160x80  0x04

// 16-bit RGB565 Color Definitions
#define ST7735_BLACK       0x0000
#define ST7735_WHITE       0xFFFF
#define ST7735_RED         0xF800
#define ST7735_GREEN       0x07E0
#define ST7735_BLUE        0x001F
#define ST7735_CYAN        0x07FF
#define ST7735_MAGENTA     0xF81F
#define ST7735_YELLOW      0xFFE0
#define ST7735_ORANGE      0xFC00
#define ST7735_PURPLE      0x780F
#define ST7735_DARKGREEN   0x03E0
#define ST7735_DARKCYAN    0x03EF
#define ST7735_GRAY        0x8410
#define ST7735_DARKGRAY    0x4208

typedef struct {
    int8_t cs_pin;      // Chip select (Active Low)
    int8_t dc_pin;      // Data / Command select
    int8_t mosi_pin;    // SPI MOSI / SDA
    int8_t sclk_pin;    // SPI SCLK / SCL
    int8_t rst_pin;     // Reset pin (-1 if tied to 3.3V/EN)
    uint8_t tab_type;   // ST7735_INIT_REDTAB (0,0), ST7735_INIT_GREENTAB (2,1), etc.
    uint8_t rotation;   // 0=Portrait (128x160), 1=Landscape (160x128), 2=Inv Port, 3=Inv Land
    int8_t colstart;    // -1 for auto (default), or 0, 1, 2... for explicit column offset
    int8_t rowstart;    // -1 for auto (default), or 0, 1, 2... for explicit row offset
} st7735_config_t;

/**
 * @brief Initialize ST7735 TFT display with given configuration.
 */
esp_err_t st7735_init(const st7735_config_t *cfg);

/**
 * @brief Set display rotation (0 = 128x160 portrait, 1 = 160x128 landscape).
 */
void st7735_set_rotation(uint8_t r);

/**
 * @brief Set hardware column and row pixel offsets manually (e.g. 0, 0 for Red Tab).
 */
void st7735_set_offset(uint8_t colstart, uint8_t rowstart);

/**
 * @brief Blank the entire 132x162 internal ST7735 controller RAM to eliminate static noise.
 */
void st7735_clear_full_ram(uint16_t color);

/**
 * @brief Get current active screen width in pixels.
 */
int16_t st7735_get_width(void);

/**
 * @brief Get current active screen height in pixels.
 */
int16_t st7735_get_height(void);

/**
 * @brief Convert 8-bit RGB components to 16-bit RGB565.
 */
static inline uint16_t st7735_color565(uint8_t r, uint8_t g, uint8_t b) {
    return ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3);
}

/**
 * @brief Set the address window for drawing operations.
 */
void st7735_set_addr_window(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1);

/**
 * @brief Fill entire screen with a single solid RGB565 color.
 */
void st7735_fill_screen(uint16_t color);

/**
 * @brief Fill a rectangle with a single color.
 */
void st7735_fill_rect(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t color);

/**
 * @brief Draw an outline rectangle.
 */
void st7735_draw_rect(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t color);

/**
 * @brief Draw a single pixel.
 */
void st7735_draw_pixel(int16_t x, int16_t y, uint16_t color);

/**
 * @brief Draw horizontal line.
 */
void st7735_draw_fast_h_line(int16_t x, int16_t y, int16_t w, uint16_t color);

/**
 * @brief Draw vertical line.
 */
void st7735_draw_fast_v_line(int16_t x, int16_t y, int16_t h, uint16_t color);

/**
 * @brief Draw arbitrary line between two points.
 */
void st7735_draw_line(int16_t x0, int16_t y0, int16_t x1, int16_t y1, uint16_t color);

/**
 * @brief Draw a single character using standard 5x7 font.
 */
void st7735_draw_char(int16_t x, int16_t y, char c, uint16_t color, uint16_t bg, uint8_t size);

/**
 * @brief Draw a text string.
 */
void st7735_draw_string(int16_t x, int16_t y, const char *str, uint16_t color, uint16_t bg, uint8_t size);

/* ==================== SCREEN TEST & GRADIENT SUITE ==================== */

/**
 * @brief Draw a smooth animated multi-color rainbow gradient across all 160x128 pixels.
 * @param step Phase shift to animate the gradient smoothly.
 */
void st7735_draw_gradient(uint8_t step);

/**
 * @brief Draw primary RGB gradient bars (Black-to-Red, Black-to-Green, Black-to-Blue).
 */
void st7735_draw_rgb_gradients(void);

/**
 * @brief Draw SMPTE-style 8 primary color bars (White, Yellow, Cyan, Green, Magenta, Red, Blue, Black).
 */
void st7735_draw_color_bars(void);

/**
 * @brief Draw pixel alignment grid, 1-pixel screen border, center crosshair, and pinout banner.
 */
void st7735_draw_pixel_test_pattern(void);

/**
 * @brief Draw solid color dead/stuck pixel test screen with title.
 */
void st7735_draw_dead_pixel_test(uint16_t color, const char *name);

/**
 * @brief Display status banner on screen (e.g., Wi-Fi IP, audio recording).
 */
void st7735_show_status(const char *header, const char *line1, const char *line2, uint16_t color);

/**
 * @brief Start background task that runs animated rainbow gradient and diagnostic test cycles.
 */
void st7735_start_test_task(void);

/* ==================== HARDWARE BENCHMARK & METRICS SUITE ==================== */

typedef struct {
    uint32_t spi_clock_hz;
    uint32_t screen_width;
    uint32_t screen_height;
    uint32_t total_pixels;
    uint32_t bytes_per_frame;
    float theoretical_max_fps;

    // Full-Screen Solid Color Fill (Raw SPI hardware bus throughput)
    float solid_fill_fps;
    float solid_fill_ms;
    float solid_fill_mb_s;
    float solid_fill_mpx_s;
    float bus_efficiency_pct;

    // Real-Time Dynamic Rainbow Gradient (Real-time HSV compute + DMA transmit)
    float gradient_fps;
    float gradient_ms;
    float gradient_mb_s;

    // Graphic primitives throughput
    float fill_rects_per_sec;
    float outline_rects_per_sec;
    float fast_lines_per_sec;
    float bresenham_lines_per_sec;
    float single_pixels_per_sec;
    float chars_per_sec;
} st7735_benchmark_results_t;

/**
 * @brief Run the complete hardware benchmark suite (100 solid fills, 60 gradient frames, 1000s of primitives).
 * @return st7735_benchmark_results_t Exact microsecond-calibrated metrics.
 */
st7735_benchmark_results_t st7735_run_benchmark(void);

/**
 * @brief Render a high-tech visual benchmark HUD scorecard directly on the ST7735 display.
 */
void st7735_display_benchmark_scorecard(const st7735_benchmark_results_t *res);

/**
 * @brief Print a structured ASCII performance report table to the serial console.
 */
void st7735_print_benchmark_report(const st7735_benchmark_results_t *res);

/**
 * @brief Start dedicated benchmark task that executes the benchmark suite and enters live stress monitoring.
 */
void st7735_start_benchmark_task(void);

#ifdef __cplusplus
}
#endif

#endif // ST7735_H
