#include <stdio.h>
#include <string.h>
#include "inmp441.h"
#include "driver/i2s_std.h"
#include "esp_log.h"

static const char *TAG = "INMP441";
static i2s_chan_handle_t s_rx_handle = NULL;
static bool s_is_running = false;

esp_err_t inmp441_init(int sck_pin, int ws_pin, int sd_pin, uint32_t sample_rate)
{
    if (s_rx_handle != NULL) {
        ESP_LOGW(TAG, "INMP441 already initialized");
        return ESP_OK;
    }

    ESP_LOGI(TAG, "Initializing INMP441: SCK(BCLK)=%d, WS(LRCLK)=%d, SD(DIN)=%d, Rate=%lu Hz",
             sck_pin, ws_pin, sd_pin, (unsigned long)sample_rate);

    i2s_chan_config_t chan_cfg = I2S_CHANNEL_DEFAULT_CONFIG(I2S_NUM_0, I2S_ROLE_MASTER);
    esp_err_t ret = i2s_new_channel(&chan_cfg, NULL, &s_rx_handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to allocate I2S channel: %s", esp_err_to_name(ret));
        return ret;
    }

    i2s_std_config_t std_cfg = {
        .clk_cfg = I2S_STD_CLK_DEFAULT_CONFIG(sample_rate),
        .slot_cfg = I2S_STD_PHILIPS_SLOT_DEFAULT_CONFIG(I2S_DATA_BIT_WIDTH_32BIT, I2S_SLOT_MODE_MONO),
        .gpio_cfg = {
            .mclk = I2S_GPIO_UNUSED,
            .bclk = (gpio_num_t)sck_pin,
            .ws   = (gpio_num_t)ws_pin,
            .dout = I2S_GPIO_UNUSED,
            .din  = (gpio_num_t)sd_pin,
            .invert_flags = {
                .mclk_inv = false,
                .bclk_inv = false,
                .ws_inv   = false,
            },
        },
    };
    // INMP441 L/R pin tied to GND sends left channel data
    std_cfg.slot_cfg.slot_mask = I2S_STD_SLOT_LEFT;

    ret = i2s_channel_init_std_mode(s_rx_handle, &std_cfg);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize I2S standard mode: %s", esp_err_to_name(ret));
        i2s_del_channel(s_rx_handle);
        s_rx_handle = NULL;
        return ret;
    }

    ESP_LOGI(TAG, "INMP441 I2S driver successfully initialized!");
    return ESP_OK;
}

esp_err_t inmp441_start(void)
{
    if (!s_rx_handle) {
        return ESP_ERR_INVALID_STATE;
    }
    if (s_is_running) {
        return ESP_OK;
    }
    esp_err_t ret = i2s_channel_enable(s_rx_handle);
    if (ret == ESP_OK) {
        s_is_running = true;
    }
    return ret;
}

esp_err_t inmp441_stop(void)
{
    if (!s_rx_handle) {
        return ESP_ERR_INVALID_STATE;
    }
    if (!s_is_running) {
        return ESP_OK;
    }
    esp_err_t ret = i2s_channel_disable(s_rx_handle);
    if (ret == ESP_OK) {
        s_is_running = false;
    }
    return ret;
}

esp_err_t inmp441_read_pcm16(int16_t *dest_buffer, size_t samples_to_read, size_t *samples_read, TickType_t timeout)
{
    if (!s_rx_handle || !dest_buffer || !samples_read) {
        return ESP_ERR_INVALID_ARG;
    }

    #define CHUNK_SAMPLES 64
    int32_t raw_chunk[CHUNK_SAMPLES];
    size_t total_samples = 0;

    while (total_samples < samples_to_read) {
        size_t needed = samples_to_read - total_samples;
        if (needed > CHUNK_SAMPLES) {
            needed = CHUNK_SAMPLES;
        }

        size_t bytes_to_read = needed * sizeof(int32_t);
        size_t bytes_read = 0;
        esp_err_t ret = i2s_channel_read(s_rx_handle, raw_chunk, bytes_to_read, &bytes_read, pdTICKS_TO_MS(timeout));
        if (ret != ESP_OK) {
            *samples_read = total_samples;
            return ret;
        }

        size_t count = bytes_read / sizeof(int32_t);
        for (size_t i = 0; i < count; i++) {
            // INMP441 24-bit data in 32-bit slot: shift right by 14 with saturation clipping
            int32_t sample = raw_chunk[i] >> 14;
            if (sample > 32767) sample = 32767;
            else if (sample < -32768) sample = -32768;
            dest_buffer[total_samples + i] = (int16_t)sample;
        }
        total_samples += count;
        if (count == 0) {
            break;
        }
    }

    *samples_read = total_samples;
    return ESP_OK;
}

void inmp441_deinit(void)
{
    if (s_rx_handle) {
        if (s_is_running) {
            i2s_channel_disable(s_rx_handle);
            s_is_running = false;
        }
        i2s_del_channel(s_rx_handle);
        s_rx_handle = NULL;
    }
}
