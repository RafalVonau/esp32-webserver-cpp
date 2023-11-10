if(IDF_VERSION_MAJOR GREATER_EQUAL 4)
    idf_component_register(SRC_DIRS src
        INCLUDE_DIRS src
        REQUIRES log nvs_flash mdns wpa_supplicant lwip esp_http_server app_update json)
else()
    set(COMPONENT_SRCDIRS src)
    set(COMPONENT_ADD_INCLUDEDIRS src)
    register_component()
endif()
