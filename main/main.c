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

static const char *TAG = "ABI_LLM";

#define WIFI_CONNECTED_BIT BIT0
#define WIFI_FAIL_BIT      BIT1
static EventGroupHandle_t s_wifi_event_group;
static int s_retry_num = 0;

/* LLM Configuration */
#define LLM_ENDPOINT_URL "http://192.168.1.7:20127/v1/chat/completions"
#define LLM_API_TOKEN    "YOUR_LLM_API_TOKEN"
#define LLM_POST_BODY    "{\"model\": \"oc/muse-spark-1.2-contributor-free\", \"messages\": [{\"role\": \"user\", \"content\": \"tell me a single line dad joke\"}]}"

static void event_handler(void *arg, esp_event_base_t event_base,
                          int32_t event_id, void *event_data)
{
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        ESP_LOGI(TAG, "Connecting to Wi-Fi SSID: \"%s\"...", ACTIVE_WIFI_SSID);
        esp_wifi_connect();
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        if (s_retry_num < CONFIG_ESP_MAXIMUM_RETRY) {
            s_retry_num++;
            ESP_LOGW(TAG, "Wi-Fi disconnected. Reconnecting (%d/%d)...", s_retry_num, CONFIG_ESP_MAXIMUM_RETRY);
            esp_wifi_connect();
        } else {
            ESP_LOGE(TAG, "Failed to connect to Wi-Fi \"%s\".", ACTIVE_WIFI_SSID);
            xEventGroupSetBits(s_wifi_event_group, WIFI_FAIL_BIT);
        }
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t *event = (ip_event_got_ip_t *)event_data;
        ESP_LOGI(TAG, "Wi-Fi Connected! Assigned IP: " IPSTR, IP2STR(&event->ip_info.ip));
        s_retry_num = 0;
        xEventGroupSetBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
    }
}

static void wifi_init_sta(void)
{
    s_wifi_event_group = xEventGroupCreate();

    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
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

    wifi_config_t wifi_config = {
        .sta = {
            .threshold.authmode = WIFI_AUTH_WPA2_PSK,
            .sae_pwe_h2e = WPA3_SAE_PWE_BOTH,
        },
    };
    strncpy((char *)wifi_config.sta.ssid, ACTIVE_WIFI_SSID, sizeof(wifi_config.sta.ssid) - 1);
    strncpy((char *)wifi_config.sta.password, ACTIVE_WIFI_PASS, sizeof(wifi_config.sta.password) - 1);

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());
}

static esp_err_t http_event_handler(esp_http_client_event_t *evt)
{
    switch (evt->event_id) {
    case HTTP_EVENT_ON_DATA:
        // Stream raw response chunk directly to serial monitor output
        printf("%.*s", evt->data_len, (char *)evt->data);
        fflush(stdout);
        break;
    default:
        break;
    }
    return ESP_OK;
}

static void send_chat_completion_request(void)
{
    ESP_LOGI(TAG, "\n=======================================================");
    ESP_LOGI(TAG, "Sending HTTP POST Request to LLM Endpoint");
    ESP_LOGI(TAG, "URL:     %s", LLM_ENDPOINT_URL);
    ESP_LOGI(TAG, "Payload: %s", LLM_POST_BODY);
    ESP_LOGI(TAG, "=======================================================\n");

    esp_http_client_config_t config = {
        .url = LLM_ENDPOINT_URL,
        .method = HTTP_METHOD_POST,
        .event_handler = http_event_handler,
        .timeout_ms = 30000,
        .buffer_size = 4096,
        .buffer_size_tx = 2048,
    };

    esp_http_client_handle_t client = esp_http_client_init(&config);
    if (!client) {
        ESP_LOGE(TAG, "Failed to initialize HTTP client");
        return;
    }

    char auth_header[128];
    snprintf(auth_header, sizeof(auth_header), "Bearer %s", LLM_API_TOKEN);

    esp_http_client_set_header(client, "Content-Type", "application/json");
    esp_http_client_set_header(client, "Authorization", auth_header);
    esp_http_client_set_post_field(client, LLM_POST_BODY, strlen(LLM_POST_BODY));

    printf("\n>>> RAW LLM RESPONSE START >>>\n");
    esp_err_t err = esp_http_client_perform(client);
    printf("\n<<< RAW LLM RESPONSE END <<<\n\n");

    if (err == ESP_OK) {
        int status = esp_http_client_get_status_code(client);
        int64_t len = esp_http_client_get_content_length(client);
        ESP_LOGI(TAG, "HTTP Request Succeeded! Status: %d, Content-Length: %lld", status, len);
    } else {
        ESP_LOGE(TAG, "HTTP Request Failed: %s", esp_err_to_name(err));
        ESP_LOGW(TAG, "Verify that 192.168.1.7:20127 is reachable and listening on your local network.");
    }

    esp_http_client_cleanup(client);
}

void app_main(void)
{
    ESP_LOGI(TAG, "Starting Abi AI Assistant - Simple LLM Completion Test");

    // Initialize NVS
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    // Initialize Wi-Fi
    wifi_init_sta();

    // Wait for Wi-Fi connection
    EventBits_t bits = xEventGroupWaitBits(s_wifi_event_group,
                                           WIFI_CONNECTED_BIT | WIFI_FAIL_BIT,
                                           pdFALSE,
                                           pdFALSE,
                                           portMAX_DELAY);

    if (bits & WIFI_CONNECTED_BIT) {
        ESP_LOGI(TAG, "Ready for network request.");
        // Give network stack a brief moment to settle
        vTaskDelay(pdMS_TO_TICKS(1000));
        // Perform the POST request and show raw response
        send_chat_completion_request();
    } else {
        ESP_LOGE(TAG, "Wi-Fi connection failed. Cannot send POST request.");
    }

    ESP_LOGI(TAG, "Done. System idle.");
}
