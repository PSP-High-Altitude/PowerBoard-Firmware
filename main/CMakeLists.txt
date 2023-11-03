idf_component_register( SRCS "main.c"
                            "http_server.c"
                            "power_control.c"
                            "../lib/max17330.c"
                        INCLUDE_DIRS "."
                            "../lib")

set(WEB_SRC_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../front/website")
spiffs_create_partition_image(www ${WEB_SRC_DIR} FLASH_IN_PROJECT)