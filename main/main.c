#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"

#include "esp_system.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "esp_netif.h"
#include "esp_http_client.h"

#include "wifi_config.h"
#include "audio_recorder.h"
#include "st7735.h"

static const char *TAG = "ABI_APP";

#define WIFI_CONNECTED_BIT BIT0
#define WIFI_FAIL_BIT      BIT1
static EventGroupHandle_t s_wifi_event_group;
static int s_retry_num = 0;

static void event_handler(void *arg, esp_event_base_t event_base,
                          int32_t event_id, void *event_data)
{
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        ESP_LOGI(TAG, "Connecting to Home Wi-Fi SSID: \"%s\"...", ACTIVE_WIFI_SSID);
        esp_wifi_connect();
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        if (s_retry_num < CONFIG_ESP_MAXIMUM_RETRY) {
            s_retry_num++;
            ESP_LOGW(TAG, "Wi-Fi disconnected. Reconnecting (%d/%d)...", s_retry_num, CONFIG_ESP_MAXIMUM_RETRY);
            esp_wifi_connect();
        } else {
            ESP_LOGW(TAG, "Home Wi-Fi connection failed. SoftAP hotspot is still active!");
            xEventGroupSetBits(s_wifi_event_group, WIFI_FAIL_BIT);
        }
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t *event = (ip_event_got_ip_t *)event_data;
        ESP_LOGI(TAG, "\n=======================================================");
        ESP_LOGI(TAG, " Home Wi-Fi Connected! IP: " IPSTR, IP2STR(&event->ip_info.ip));
        ESP_LOGI(TAG, " Browser URL: http://" IPSTR "/", IP2STR(&event->ip_info.ip));
        ESP_LOGI(TAG, "=======================================================\n");
        s_retry_num = 0;
        xEventGroupSetBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_AP_STACONNECTED) {
        ESP_LOGI(TAG, "Device connected to ABI-RECORDER hotspot!");
    }
}

static void wifi_init_dual_mode(void)
{
    s_wifi_event_group = xEventGroupCreate();

    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    // Create both Access Point and Station network interfaces
    esp_netif_create_default_wifi_ap();
    esp_netif_create_default_wifi_sta();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    esp_event_handler_instance_t instance_any_id;
    esp_event_handler_instance_t instance_got_ip;
    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT,
                                                        ESP_EVENT_ANY_ID,
                                                        &event_handler,
                                                        NULL,
                                                        &instance_any_id));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT,
                                                        IP_EVENT_STA_GOT_IP,
                                                        &event_handler,
                                                        NULL,
                                                        &instance_got_ip));

    // Access Point (Hotspot) Configuration
    wifi_config_t ap_config = {
        .ap = {
            .ssid = "ABI-RECORDER",
            .ssid_len = strlen("ABI-RECORDER"),
            .channel = 1,
            .password = "", // Open network - no password needed!
            .max_connection = 4,
            .authmode = WIFI_AUTH_OPEN,
        },
    };

    bool has_home_wifi = (strcmp(ACTIVE_WIFI_SSID, "YOUR_WIFI_SSID") != 0 && strlen(ACTIVE_WIFI_SSID) > 0);

    if (has_home_wifi) {
        wifi_config_t sta_config = {
            .sta = {
                .threshold.authmode = WIFI_AUTH_WPA2_PSK,
                .sae_pwe_h2e = WPA3_SAE_PWE_BOTH,
            },
        };
        strncpy((char *)sta_config.sta.ssid, ACTIVE_WIFI_SSID, sizeof(sta_config.sta.ssid) - 1);
        strncpy((char *)sta_config.sta.password, ACTIVE_WIFI_PASS, sizeof(sta_config.sta.password) - 1);

        ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_APSTA));
        ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &ap_config));
        ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &sta_config));
    } else {
        ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_AP));
        ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &ap_config));
    }

    ESP_ERROR_CHECK(esp_wifi_start());

    // Start HTTP Web Server on port 80 immediately
    audio_recorder_start_webserver();
}

void app_main(void)
{
    ESP_LOGI(TAG, "Starting Abi AI Assistant Firmware...");

    // 1. Initialize NVS
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    // 2. Initialize Hardware Subsystem (INMP441 Mic + Button + SPIFFS Storage)
    ESP_ERROR_CHECK(audio_recorder_init());

    // 3. Initialize ST7735 TFT Display (GPIO 11, 15, 16, 17, 18)
    st7735_config_t tft_cfg = {
        .cs_pin = ST7735_PIN_CS,      // GPIO 11
        .rst_pin = ST7735_PIN_RST,    // GPIO 17
        .dc_pin = ST7735_PIN_DC,      // GPIO 18
        .mosi_pin = ST7735_PIN_MOSI,  // GPIO 15
        .sclk_pin = ST7735_PIN_SCLK,  // GPIO 16
        .tab_type = ST7735_INIT_REDTAB, // Red Tab (0,0 offset - eliminates edge noise)
        .rotation = 1,                 // 160x128 Landscape
        .colstart = 0,                 // Explicit 0 column offset
        .rowstart = 0,                 // Explicit 0 row offset
    };
    esp_err_t tft_ret = st7735_init(&tft_cfg);
    if (tft_ret == ESP_OK) {
        ESP_LOGI(TAG, "ST7735 initialized successfully! Launching Screen Test Suite...");
        st7735_start_test_task();
    } else {
        ESP_LOGW(TAG, "ST7735 initialization returned: %s", esp_err_to_name(tft_ret));
    }

    // 4. Start Wi-Fi (SoftAP Hotspot "ABI-RECORDER" + Home Wi-Fi STA if configured)
    wifi_init_dual_mode();

    ESP_LOGI(TAG, "\n"
        "******************************************************************\n"
        "             ABI VOICE RECORDER SYSTEM INITIALIZED                \n"
        "******************************************************************\n"
        "  - Microphone: INMP441 (SCK=GPIO 4, WS=GPIO 5, SD=GPIO 6)\n"
        "  - Push Button: GPIO 7 (also on-board BOOT button GPIO 0)\n"
        "  - Display:     ST7735 1.8\" SPI TFT (160x128 Landscape)\n"
        "                 CS: GPIO 11, RST: GPIO 17, DC: GPIO 18\n"
        "                 MOSI: GPIO 15, SCLK: GPIO 16, BL: 3.3V\n"
        "  - Storage:     8 MB SPIFFS partition mounted at /storage\n"
        "  - Audio:       16 kHz, 16-bit Mono PCM (.WAV)\n"
        "\n"
        "  🌐 BROWSER ACCESS:\n"
        "  1. Connect PC or Phone Wi-Fi to: \"ABI-RECORDER\"\n"
        "  2. Open in Chrome: http://192.168.4.1/\n"
        "******************************************************************"
    );
}
