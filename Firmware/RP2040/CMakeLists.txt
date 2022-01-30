cmake_minimum_required(VERSION 3.12)

include(pico_sdk_import.cmake)

project(grblDRO)

pico_sdk_init()

add_executable(grblDRO
 src/main.c
 src/grblcomm.c
 src/fonts.c
 src/keypad.c
 src/interface.c
 src/canvas/boot.c
 src/canvas/common.c
 src/canvas/confirm.c
 src/canvas/dro.c
 src/canvas/grblutils.c
 src/canvas/menu.c
 src/canvas/sdcard.c
 src/canvas/sender.c
 src/canvas/threading.c)

add_subdirectory(src/LCD)
add_subdirectory(src/UILib)
add_subdirectory(hal_rp2040)

target_sources(grblDRO PRIVATE src/main.c)

# Pull in our pico_stdlib which pulls in commonly used features
target_link_libraries(grblDRO PRIVATE lcd_panel uilib mpg_dro_driver)

# create map/bin/hex file etc.
pico_add_extra_outputs(grblDRO)
