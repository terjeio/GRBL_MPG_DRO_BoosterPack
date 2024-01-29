add_library(msp430_flasher INTERFACE)

target_sources(msp430_flasher INTERFACE
 ${CMAKE_CURRENT_LIST_DIR}/Devices430.c
 ${CMAKE_CURRENT_LIST_DIR}/FlashErase.c
 ${CMAKE_CURRENT_LIST_DIR}/FlashWrite.c
 ${CMAKE_CURRENT_LIST_DIR}/JTAGfunc430.c
 ${CMAKE_CURRENT_LIST_DIR}/LowLevelFuncRP2040.c
)

target_include_directories(msp430_flasher INTERFACE ${CMAKE_CURRENT_LIST_DIR})
