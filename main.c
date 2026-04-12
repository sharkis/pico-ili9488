#include <stdio.h>
#include "pico/stdlib.h"
#include "hardware/spi.h"
#include "ili9488.h"
#include "testimage.h"

#define SPI_PORT spi0
#define PIN_MISO 0  // Physical Pin 1
#define PIN_CS   1  // Physical Pin 2
#define PIN_SCK  2  // Physical Pin 4
#define PIN_TX   3  // Physical Pin 5

#define PIN_DC   20 // Choose any free GPIO for Data/Command
#define PIN_RST  21 // Choose any free GPIO for Reset

int main() {
    stdio_init_all();

    // Initialize SPI0 at 20MHz (ILI9488 can usually handle up to 40MHz)
    spi_init(SPI_PORT, 20 * 1000 * 1000);

    // Map the GPIOs to the SPI function
    gpio_set_function(PIN_MISO, GPIO_FUNC_SPI);
    gpio_set_function(PIN_SCK,  GPIO_FUNC_SPI);
    gpio_set_function(PIN_TX,   GPIO_FUNC_SPI);

    // Initialize Chip Select (CS) manually for better control
    gpio_init(PIN_CS);
    gpio_set_dir(PIN_CS, GPIO_OUT);
    gpio_put(PIN_CS, 1); // Default high (deselected)

    // Initialize DC and Reset pins
    gpio_init(PIN_DC);
    gpio_set_dir(PIN_DC, GPIO_OUT);
    gpio_init(PIN_RST);
    gpio_set_dir(PIN_RST, GPIO_OUT);

    lcd_init();
    draw_image_cpu(test_image_data,320,480);;

    while (1) {
        // Cycle colors to test
//        fill_screen(0xFF, 0x00, 0x00); // Red
//        sleep_ms(1000);
//        fill_screen(0x00, 0xFF, 0x00); // Green
//        sleep_ms(1000);
//        fill_screen(0x00, 0x00, 0xFF); // Blue
//        sleep_ms(1000);
    }
}
