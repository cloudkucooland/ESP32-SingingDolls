idf_component_register(SRCS "sd-idf.c"
                    INCLUDE_DIRS "."
		    PRIV_REQUIRES nvs_flash bt esp_event)
