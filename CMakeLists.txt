cmake_minimum_required(VERSION 4.0)

include(pico_sdk_import.cmake)
project(charger)
set(CMAKE_CSTANDARD 11)
pico_sdk_init()
add_executable(charger
	main.c
	ili9488.c
	testimage.h
)

target_link_libraries(charger pico_stdlib hardware_spi)
pico_add_extra_outputs(charger)
