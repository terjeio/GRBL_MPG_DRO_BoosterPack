cmake_minimum_required(VERSION 3.12)

include(pico_sdk_import.cmake)

project(grblDRO)

pico_sdk_init()

add_executable(grblDRO
 src/main.c
 src/grbl/parser.c
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
 src/canvas/threading.c
 src/msp430.c
)

include_directories(${PICO_SDK_PATH}/src/rp2_common/cmsis/stub/CMSIS/Core/Include)

include(hal_rp2040/CMakeLists.txt)
include(src/UILib/CMakeLists.txt)
include(src/LCD/CMakeLists.txt)
include(msp430/CMakeLists.txt)

target_sources(grblDRO PRIVATE src/main.c)

# Pull in our pico_stdlib which pulls in commonly used features
target_link_libraries(grblDRO PRIVATE lcd_panel uilib mpg_dro_driver msp430_flasher)

# target_compile_definitions(grblDRO PUBLIC PARSER_I2C_ENABLE)
target_compile_definitions(grblDRO PUBLIC UART_MODE=0)
target_compile_definitions(grblDRO PUBLIC PARSER_SERIAL_ENABLE)
target_compile_definitions(grblDRO PUBLIC UILIB_NAVIGATOR_ENABLE=1)
target_compile_definitions(grblDRO PUBLIC UILIB_KEYPAD_ENABLE=1)

# create map/bin/hex file etc.
pico_add_extra_outputs(grblDRO)
