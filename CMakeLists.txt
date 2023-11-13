cmake_minimum_required(VERSION 3.14)

idf_component_register(SRC_DIRS src
    INCLUDE_DIRS src third-party/ArduinoJson/src
    REQUIRES log nvs_flash mdns wpa_supplicant lwip esp_http_server app_update json)

