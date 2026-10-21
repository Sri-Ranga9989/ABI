#include <stdio.h>
#include <string.h>
#include "button.h"
#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"

static const char *TAG = "BUTTON";

static button_callback_t s_on_press_cb = NULL;
static button_callback_t s_on_release_cb = NULL;
static int s_primary_gpio = BUTTON_DEFAULT_GPIO_PIN;
static bool s_last_pressed = false;

bool button_is_pressed(int gpio_pin)
{
    // Active LOW with pullup (0 = pressed, 1 = released)
    return gpio_get_level((gpio_num_t)gpio_pin) == 0;
}

static void button_task(void *pvParameters)
{
    int consecutive_press = 0;
    int consecutive_release = 0;
    const int debounce_threshold = 2; // 2 * 15ms = 30ms debounce

    ESP_LOGI(TAG, "Button monitor task active. Primary Button: GPIO %d | BOOT Button: GPIO %d",
             s_primary_gpio, BUTTON_BOOT_GPIO_PIN);

    while (1) {
        vTaskDelay(pdMS_TO_TICKS(15));

        // Either primary button (GPIO 7) OR onboard BOOT button (GPIO 0)
        bool pressed_now = button_is_pressed(s_primary_gpio) || button_is_pressed(BUTTON_BOOT_GPIO_PIN);

        if (pressed_now) {
            consecutive_press++;
            consecutive_release = 0;
            if (consecutive_press >= debounce_threshold && !s_last_pressed) {
                s_last_pressed = true;
                ESP_LOGI(TAG, ">>> Button PRESSED! <<<");
                if (s_on_press_cb) {
                    s_on_press_cb();
                }
            }
        } else {
            consecutive_release++;
            consecutive_press = 0;
            if (consecutive_release >= debounce_threshold && s_last_pressed) {
                s_last_pressed = false;
                ESP_LOGI(TAG, ">>> Button RELEASED! <<<");
                if (s_on_release_cb) {
                    s_on_release_cb();
                }
            }
        }
    }
}

esp_err_t button_init(int gpio_pin, button_callback_t on_press, button_callback_t on_release)
{
    s_primary_gpio = gpio_pin;
    s_on_press_cb = on_press;
    s_on_release_cb = on_release;

    uint64_t pin_mask = (1ULL << gpio_pin);
    if (gpio_pin != BUTTON_BOOT_GPIO_PIN) {
        pin_mask |= (1ULL << BUTTON_BOOT_GPIO_PIN);
    }

    gpio_config_t io_conf = {
        .pin_bit_mask = pin_mask,
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    esp_err_t ret = gpio_config(&io_conf);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to configure button GPIOs: %s", esp_err_to_name(ret));
        return ret;
    }

    xTaskCreate(&button_task, "btn_task", 3072, NULL, 10, NULL);
    return ESP_OK;
}
