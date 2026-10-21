#pragma once

#include <stdint.h>
#include <stddef.h>
#include "esp_err.h"
#include "freertos/FreeRTOS.h"

#ifdef __cplusplus
extern "C" {
#endif

#define INMP441_DEFAULT_SCK_PIN   4  // BCLK / SCK
#define INMP441_DEFAULT_WS_PIN    5  // LRCLK / WS
#define INMP441_DEFAULT_SD_PIN    6  // DOUT / SD
#define INMP441_DEFAULT_RATE      16000 // 16 kHz sample rate (Standard for Speech / AI)

/**
 * @brief Initialize the INMP441 MEMS microphone via ESP-IDF I2S standard driver.
 *
 * @param sck_pin     I2S Bit Clock (SCK / BCLK) pin
 * @param ws_pin      I2S Word Select (WS / LRCLK) pin
 * @param sd_pin      I2S Serial Data (SD / DIN) pin
 * @param sample_rate Audio sampling rate in Hz (e.g. 16000)
 * @return ESP_OK on success
 */
esp_err_t inmp441_init(int sck_pin, int ws_pin, int sd_pin, uint32_t sample_rate);

/**
 * @brief Enable/start I2S RX channel.
 */
esp_err_t inmp441_start(void);

/**
 * @brief Disable/stop I2S RX channel.
 */
esp_err_t inmp441_stop(void);

/**
 * @brief Read 16-bit mono PCM samples from INMP441 microphone.
 *        Converts raw 24-bit I2S data to 16-bit PCM with digital gain.
 *
 * @param dest_buffer      Buffer to store 16-bit signed PCM samples
 * @param samples_to_read  Number of 16-bit samples requested
 * @param samples_read     Pointer to store actual number of samples read
 * @param timeout          FreeRTOS ticks timeout
 * @return ESP_OK on success
 */
esp_err_t inmp441_read_pcm16(int16_t *dest_buffer, size_t samples_to_read, size_t *samples_read, TickType_t timeout);

/**
 * @brief Deinitialize and release I2S channel resources.
 */
void inmp441_deinit(void);

#ifdef __cplusplus
}
#endif
