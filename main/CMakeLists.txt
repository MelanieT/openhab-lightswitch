file(GLOB_RECURSE EXTRA_SRCS ui *.c)
idf_component_register(
    SRCS
        "main.cpp"
        "lvgl_port.c"
        "OpenHabLvglObject.cpp"
        "OpenHabLvglObject.h"
        "Factory.cpp"
        "Factory.h"
        "Frame.cpp"
        "Frame.h"
        "Switch.cpp"
        "Switch.h"
        "Flyout.cpp"
        "Flyout.h"
        "Slider.cpp"
        "Slider.h"
        "ColorWheel.cpp"
        "ColorWheel.h"
        "TextDisplay.cpp"
        "TextDisplay.h"
        "back.h"
        "lcd.c"
        "lcd.h"
        "EventHandler.cpp"
        "EventHandler.h"
        "captdns.c"
        "captdns.h"
        "SimpleWebServer.cpp"
        "SimpleWebServer.h"
        "SetupApi.cpp"
        "SetupApi.h"
    ${EXTRA_SRCS}
    INCLUDE_DIRS
        "."
    )

idf_component_get_property(lvgl_lib lvgl__lvgl COMPONENT_LIB)
target_compile_options(${lvgl_lib} PRIVATE -Wno-format)
