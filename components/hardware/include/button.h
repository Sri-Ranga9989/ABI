#pragma once

#include <stdbool.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

#define BUTTON_DEFAULT_GPIO_PIN      7 // Main push button (connected between GPIO 7 and GND)
#define BUTTON_BOOT_GPIO_PIN         0 // Optional onboard BOOT button (GPIO 0 and GND)

typedef void (*button_callback_t)(void);

/**
 * @brief Initialize a push button with internal pull-up and debounce monitor.
 *
 * @param gpio_pin    GPIO pin number (e.g. GPIO 7)
 * @param on_press    Callback triggered when button is pressed (can be NULL)
 * @param on_release  Callback triggered when button is released (can be NULL)
 * @return ESP_OK on success
 */
esp_err_t button_init(int gpio_pin, button_callback_t on_press, button_callback_t on_release);

/**
 * @brief Check if the button is currently pressed (active low).
 *
 * @param gpio_pin GPIO pin number
 * @return true if pressed, false otherwise
 */
bool button_is_pressed(int gpio_pin);

#ifdef __cplusplus
}
#endif
