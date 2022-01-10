add_library(mpg_dro_driver INTERFACE)

pico_generate_pio_header(mpg_dro_driver ${CMAKE_CURRENT_LIST_DIR}/encoder.pio)

target_sources(mpg_dro_driver INTERFACE
 ${CMAKE_CURRENT_LIST_DIR}/serial.c
 ${CMAKE_CURRENT_LIST_DIR}/driver.c
 ${CMAKE_CURRENT_LIST_DIR}/i2c_nb.c
 ${CMAKE_CURRENT_LIST_DIR}/lcd_driver/driver.c
)

target_include_directories(mpg_dro_driver INTERFACE ${CMAKE_CURRENT_LIST_DIR})
target_link_libraries(mpg_dro_driver INTERFACE pico_stdlib hardware_uart hardware_pio hardware_i2c hardware_spi hardware_gpio hardware_pwm hardware_clocks)
