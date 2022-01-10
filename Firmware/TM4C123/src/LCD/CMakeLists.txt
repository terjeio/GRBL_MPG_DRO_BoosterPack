add_library(lcd_panel INTERFACE)

target_sources(lcd_panel INTERFACE
 ${CMAKE_CURRENT_LIST_DIR}/graphics.c
 ${CMAKE_CURRENT_LIST_DIR}/lcd.c
 ${CMAKE_CURRENT_LIST_DIR}/interface.c
)
