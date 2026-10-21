idf_component_register(
    SRCS "inmp441.c" "button.c" "audio_recorder.c" "st7735.c"
    INCLUDE_DIRS "include"
    REQUIRES esp_driver_i2s esp_driver_gpio spiffs esp_psram esp_timer esp_http_server esp_netif esp_driver_spi driver
)
