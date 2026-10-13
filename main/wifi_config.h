#pragma once

#include "sdkconfig.h"

/*
 * ============================================================================
 * HOME WI-FI CONFIGURATION
 * ============================================================================
 * You can set your Wi-Fi credentials in two ways:
 * 1. Uncomment and fill in WIFI_SSID_OVERRIDE & WIFI_PASS_OVERRIDE below, OR
 * 2. Run 'idf.py menuconfig' -> 'Wi-Fi Configuration' -> enter SSID and Password.
 * ============================================================================
 */

// Uncomment and set your home Wi-Fi SSID and Password here if you prefer editing C code:
// #define WIFI_SSID_OVERRIDE      "YOUR_WIFI_SSID"
// #define WIFI_PASS_OVERRIDE      "YOUR_WIFI_PASSWORD"

#ifdef WIFI_SSID_OVERRIDE
    #define ACTIVE_WIFI_SSID     WIFI_SSID_OVERRIDE
#elif defined(CONFIG_ESP_WIFI_SSID)
    #define ACTIVE_WIFI_SSID     CONFIG_ESP_WIFI_SSID
#else
    #define ACTIVE_WIFI_SSID     "YOUR_WIFI_SSID"
#endif

#ifdef WIFI_PASS_OVERRIDE
    #define ACTIVE_WIFI_PASS     WIFI_PASS_OVERRIDE
#elif defined(CONFIG_ESP_WIFI_PASSWORD)
    #define ACTIVE_WIFI_PASS     CONFIG_ESP_WIFI_PASSWORD
#else
    #define ACTIVE_WIFI_PASS     "YOUR_WIFI_PASSWORD"
#endif

// Maximum reconnection retries
#ifndef CONFIG_ESP_MAXIMUM_RETRY
    #define CONFIG_ESP_MAXIMUM_RETRY 5
#endif

// Connectivity test endpoints
#ifndef CONFIG_CONNECTIVITY_TEST_URL
    #define CONFIG_CONNECTIVITY_TEST_URL "http://connectivitycheck.gstatic.com/generate_204"
#endif

#ifndef CONFIG_PUBLIC_IP_TEST_URL
    #define CONFIG_PUBLIC_IP_TEST_URL "http://api.ipify.org"
#endif

#ifndef CONFIG_HTTPS_TEST_URL
    #define CONFIG_HTTPS_TEST_URL "https://httpbin.org/get"
#endif
