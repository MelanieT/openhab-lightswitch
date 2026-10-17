# For more information about build system see
# https://docs.espressif.com/projects/esp-idf/en/latest/api-guides/build-system.html
# The following five lines of boilerplate have to be in your project's
# CMakeLists in this exact order for cmake to work correctly
cmake_minimum_required(VERSION 3.5)
include($ENV{IDF_PATH}/tools/cmake/project.cmake)
# Only proven with ESP-IDF 5.3.1. Other versions (5.5.1) build fine but the display misbehaves.
if(NOT "${IDF_VERSION_MAJOR}.${IDF_VERSION_MINOR}.${IDF_VERSION_PATCH}" STREQUAL "5.3.1")
    message(FATAL_ERROR "Build with ESP-IDF 5.3.1, not ${IDF_VERSION_MAJOR}.${IDF_VERSION_MINOR}.${IDF_VERSION_PATCH} (IDF_PATH=$ENV{IDF_PATH})")
endif()
add_compile_options("-Wno-attributes")
idf_build_set_property(COMPILE_OPTIONS "-w" APPEND)
idf_build_set_property(COMPILE_OPTIONS "-fpermissive" APPEND)
project(lightswitch)

# The web interface, CONFIG_WEB_INTERFACE_DIR (wifi-configurator's dist-lightswitch)
if(CONFIG_USE_SETUP_WEB_INTERFACE OR CONFIG_START_WEBSERVER_IN_STATION_MODE)
    spiffs_create_partition_image(${CONFIG_WEB_INTERFACE_SPIFFS_PARTITION_NAME} ${CONFIG_WEB_INTERFACE_DIR} FLASH_IN_PROJECT)
endif()
