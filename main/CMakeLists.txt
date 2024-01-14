idf_component_register( SRCS "main.c"
                            "http_server.c"
                            "power_control.c"
                            "../lib/max17330.c"
                        INCLUDE_DIRS "."
                            "../lib")

spiffs_create_partition_image(www ${CMAKE_CURRENT_SOURCE_DIR}/../front/website FLASH_IN_PROJECT)