idf_component_register(SRCS "main.c"
                       INCLUDE_DIRS "."
                       REQUIRES esp_wifi esp_event esp_netif nvs_flash esp_http_client esp_psram esp_hw_support spi_flash mbedtls esp_timer lwip hardware)
