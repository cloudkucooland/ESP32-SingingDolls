idf_component_register(SRCS "main.c;leaf.c;audio.c"
                    INCLUDE_DIRS "."
		    PRIV_REQUIRES esp_event nvs_flash bt esp32-LEAF)
