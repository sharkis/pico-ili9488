#include <stdio.h>
#include "pico/stdlib.h"
#include "hardware/spi.h"

// Use GPIO numbers, which correspond to your requested physical pins
#define SPI_PORT spi0
#define PIN_MISO 0  // Physical Pin 1
#define PIN_CS   1  // Physical Pin 2
#define PIN_SCK  2  // Physical Pin 4
#define PIN_TX   3  // Physical Pin 5

// You will also need these for the ILI9488
#define PIN_DC   20 // Choose any free GPIO for Data/Command
#define PIN_RST  21 // Choose any free GPIO for Reset

// ILI9488 Commands
#define CMD_SOFT_RESET  0x01
#define CMD_SLEEP_OUT   0x11
#define CMD_DISPLAY_ON  0x29
#define CMD_COLUMN_ADDR 0x2A
#define CMD_PAGE_ADDR   0x2B
#define CMD_RAM_WRITE   0x2C
#define CMD_PIXEL_FMT   0x3A

// Helper to send a command
void lcd_write_cmd(uint8_t cmd) {
    gpio_put(PIN_DC, 0); // DC low for Command
    gpio_put(PIN_CS, 0);
    spi_write_blocking(SPI_PORT, &cmd, 1);
    gpio_put(PIN_CS, 1);
}

// Helper to send data
void lcd_write_data(uint8_t data) {
    gpio_put(PIN_DC, 1); // DC high for Data
    gpio_put(PIN_CS, 0);
    spi_write_blocking(SPI_PORT, &data, 1);
    gpio_put(PIN_CS, 1);
}

void lcd_init() {
    // Hardware Reset
    gpio_put(PIN_RST, 1);
    sleep_ms(5);
    gpio_put(PIN_RST, 0);
    sleep_ms(20);
    gpio_put(PIN_RST, 1);
    sleep_ms(150);

    lcd_write_cmd(CMD_SOFT_RESET);
    sleep_ms(150);

    lcd_write_cmd(0xE0); // Positive Gamma Control
    uint8_t pgam[] = {0x00, 0x03, 0x09, 0x08, 0x16, 0x0A, 0x3F, 0x78, 0x4C, 0x09, 0x0A, 0x08, 0x16, 0x1A, 0x0F};
    for(int i=0; i<15; i++) lcd_write_data(pgam[i]);

    lcd_write_cmd(0xE1); // Negative Gamma Control
    uint8_t ngam[] = {0x00, 0x16, 0x19, 0x03, 0x0F, 0x05, 0x32, 0x45, 0x46, 0x04, 0x0E, 0x0D, 0x35, 0x37, 0x0F};
    for(int i=0; i<15; i++) lcd_write_data(ngam[i]);

    lcd_write_cmd(0xC0); // Power Control 1
    lcd_write_data(0x17);
    lcd_write_data(0x15);

    lcd_write_cmd(0xC1); // Power Control 2
    lcd_write_data(0x41);

    lcd_write_cmd(0xC5); // VCOM Control
    lcd_write_data(0x00);
    lcd_write_data(0x12);
    lcd_write_data(0x80);

    lcd_write_cmd(CMD_PIXEL_FMT); // Interface Pixel Format
    lcd_write_data(0x66); // 18 bit color

    lcd_write_cmd(0xB1); // Frame rate
    lcd_write_data(0xA0);

    lcd_write_cmd(0xB4); // Display Inversion Control
    lcd_write_data(0x02);

    lcd_write_cmd(0xB6); // Display Function Control
    lcd_write_data(0x02);
    lcd_write_data(0x02);

    lcd_write_cmd(0xE9); // Set Image Function
    lcd_write_data(0x00);

    lcd_write_cmd(0xF7); // Adjust Control 3
    lcd_write_data(0xA9);
    lcd_write_data(0x51);
    lcd_write_data(0x2C);
    lcd_write_data(0x82);

    lcd_write_cmd(CMD_SLEEP_OUT);
    sleep_ms(120);
    lcd_write_cmd(CMD_DISPLAY_ON);
}

void fill_screen(uint8_t r, uint8_t g, uint8_t b) {
    // Set column range 0-319, page range 0-479
    lcd_write_cmd(CMD_COLUMN_ADDR);
    lcd_write_data(0x00); lcd_write_data(0x00);
    lcd_write_data(0x01); lcd_write_data(0x3F);

    lcd_write_cmd(CMD_PAGE_ADDR);
    lcd_write_data(0x00); lcd_write_data(0x00);
    lcd_write_data(0x01); lcd_write_data(0xDF);

    lcd_write_cmd(CMD_RAM_WRITE);
    
    gpio_put(PIN_DC, 1);
    gpio_put(PIN_CS, 0);
    for (int i = 0; i < 320 * 480; i++) {
        spi_write_blocking(SPI_PORT, &r, 1);
        spi_write_blocking(SPI_PORT, &g, 1);
        spi_write_blocking(SPI_PORT, &b, 1);
    }
    gpio_put(PIN_CS, 1);
}

int main() {
    stdio_init_all();

    // 1. Initialize SPI0 at 20MHz (ILI9488 can usually handle up to 40MHz)
    spi_init(SPI_PORT, 20 * 1000 * 1000);

    // 2. Map the GPIOs to the SPI function
    gpio_set_function(PIN_MISO, GPIO_FUNC_SPI);
    gpio_set_function(PIN_SCK,  GPIO_FUNC_SPI);
    gpio_set_function(PIN_TX,   GPIO_FUNC_SPI);

    // 3. Initialize Chip Select (CS) manually for better control
    gpio_init(PIN_CS);
    gpio_set_dir(PIN_CS, GPIO_OUT);
    gpio_put(PIN_CS, 1); // Default high (deselected)

    // 4. Initialize DC and Reset pins
    gpio_init(PIN_DC);
    gpio_set_dir(PIN_DC, GPIO_OUT);
    gpio_init(PIN_RST);
    gpio_set_dir(PIN_RST, GPIO_OUT);

    lcd_init();

    while (1) {
        // Cycle colors to test
        fill_screen(0xFF, 0x00, 0x00); // Red
        sleep_ms(1000);
        fill_screen(0x00, 0xFF, 0x00); // Green
        sleep_ms(1000);
        fill_screen(0x00, 0x00, 0xFF); // Blue
        sleep_ms(1000);
    }
}
