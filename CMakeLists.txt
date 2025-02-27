# For more information about build system see
# https://docs.espressif.com/projects/esp-idf/en/latest/api-guides/build-system.html
# The following five lines of boilerplate have to be in your project's
# CMakeLists in this exact order for cmake to work correctly
cmake_minimum_required(VERSION 3.5)
include($ENV{IDF_PATH}/tools/cmake/project.cmake)
add_compile_options("-Wno-attributes")
idf_build_set_property(COMPILE_OPTIONS "-w" APPEND)
idf_build_set_property(COMPILE_OPTIONS "-fpermissive" APPEND)
project(lightswitch)

spiffs_create_partition_image(spiffs /Users/melanie/Projects/wifi-configurator/dist FLASH_IN_PROJECT)
