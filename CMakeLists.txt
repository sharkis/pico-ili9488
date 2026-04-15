cmake_minimum_required(VERSION 4.0)
set(WIFI_SSID "${WIFI_SSID}" CACHE INTERNAL "WiFi SSID")
set(WIFI_PASSWORD "${WIFI_PASSWORD}" CACHE INTERNAL "WiFi password")

include(pico_sdk_import.cmake)
project(charger)
set(CMAKE_CSTANDARD 11)
pico_sdk_init()
add_executable(charger
	main.c
	ili9488.c
	testimage.h
	lwipopts.h
)
target_include_directories(charger PRIVATE ${CMAKE_CURRENT_LIST_DIR})
target_compile_definitions(${PROJECT_NAME} PRIVATE
    WIFI_SSID=\"${WIFI_SSID}\"
    WIFI_PASSWORD=\"${WIFI_PASSWORD}\"
)

target_link_libraries(charger
	pico_stdlib
	hardware_spi
	pico_cyw43_arch_lwip_threadsafe_background
)
pico_add_extra_outputs(charger)
