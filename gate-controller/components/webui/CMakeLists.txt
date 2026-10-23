idf_component_register(SRCS "webui.c"
                       INCLUDE_DIRS "include"
                       PRIV_REQUIRES sim motor_drv esp_http_server esp_wifi esp_netif
                                     storage_nvs gate_logic events_log nvs_flash
                       EMBED_FILE_TXT assets/index.html)
