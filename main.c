#include <stdio.h>
#include "pico/stdlib.h"
#include "pico/bootrom.h"
#include "hardware/spi.h"
#include "hardware/sync.h"
#include "hardware/structs/ioqspi.h"
#include "hardware/structs/sio.h"
#include "ili9488.h"
#include "pico/cyw43_arch.h"
#include "lwip/tcp.h"

bool __no_inline_not_in_flash_func(get_bootsel_button)(void) {
    const uint CS_PIN_INDEX = 1;
    uint32_t flags = save_and_disable_interrupts();
    hw_write_masked(&ioqspi_hw->io[CS_PIN_INDEX].ctrl,
                    GPIO_OVERRIDE_LOW << IO_QSPI_GPIO_QSPI_SS_CTRL_OEOVER_LSB,
                    IO_QSPI_GPIO_QSPI_SS_CTRL_OEOVER_BITS);
    for (volatile int i = 0; i < 1000; ++i);
#if PICO_RP2040
    #define CS_BIT (1u << 1)
#else
    #define CS_BIT SIO_GPIO_HI_IN_QSPI_CSN_BITS
#endif
    bool pressed = !(sio_hw->gpio_hi_in & CS_BIT);
    hw_write_masked(&ioqspi_hw->io[CS_PIN_INDEX].ctrl,
                    GPIO_OVERRIDE_NORMAL << IO_QSPI_GPIO_QSPI_SS_CTRL_OEOVER_LSB,
                    IO_QSPI_GPIO_QSPI_SS_CTRL_OEOVER_BITS);
    restore_interrupts(flags);
    return pressed;
}

#define SPI_PORT spi0
#define PIN_MISO 0  // Physical Pin 1
#define PIN_CS   1  // Physical Pin 2
#define PIN_SCK  2  // Physical Pin 4
#define PIN_TX   3  // Physical Pin 5

#define PIN_DC   20 // Choose any free GPIO for Data/Command
#define PIN_RST  21 // Choose any free GPIO for Reset

#define SERVER_IP "192.168.1.223"
#define SERVER_PORT 3000
#define IMG_SIZE (320 * 480 * 3)

uint8_t full_image_buffer[IMG_SIZE];
uint32_t total_received = 0;
bool header_found = false;

struct tcp_pcb *client_pcb;
static err_t pcb_recv(void *arg, struct tcp_pcb *tpcb, struct pbuf *p, err_t err) {
    if (p == NULL) {
        // --- CONNECTION CLOSED BY SERVER ---
        printf("Download complete. Received %u bytes. Drawing...\n", total_received);

        // Draw the image even if it's slightly incomplete
        if (total_received > 0) {
            draw_image_cpu(full_image_buffer, 320, 480);
        }

        // Reset for next time
        header_found = false;
        total_received = 0;
        tcp_close(tpcb);
        return ERR_OK;
    }

    // Use p->tot_len to get the size of the whole packet chain
    uint32_t packet_len = p->tot_len;

    if (!header_found) {
        // Find the header (\r\n\r\n)
        // We copy the start of the packet to a temp buffer to search it
        uint8_t temp[512];
        uint16_t search_len = packet_len > 512 ? 512 : packet_len;
        pbuf_copy_partial(p, temp, search_len, 0);

        for (uint32_t i = 0; i < search_len - 4; i++) {
            if (temp[i] == '\r' && temp[i+1] == '\n' &&
                temp[i+2] == '\r' && temp[i+3] == '\n') {

                header_found = true;
                uint32_t img_start_offset = i + 4;
                uint32_t img_len = packet_len - img_start_offset;

                // Copy the remainder of this packet into the global buffer
                if (img_len > 0) {
                    pbuf_copy_partial(p, full_image_buffer, img_len, img_start_offset);
                    total_received = img_len;
                }
                break;
            }
        }
    } else {
        // Append data to the buffer
        if (total_received + packet_len <= IMG_SIZE) {
            pbuf_copy_partial(p, full_image_buffer + total_received, packet_len, 0);
            total_received += packet_len;
        }
    }

    tcp_recved(tpcb, p->tot_len);
    pbuf_free(p);
    return ERR_OK;
}

static err_t pcb_connected(void *arg,struct tcp_pcb *tpcb, err_t err){
	char request[256];
	snprintf(request, sizeof(request),"GET / HTTP/1.1\r\nHost: %s\r\nConnection: close\r\n\r\n",SERVER_IP);
	tcp_write(tpcb,request,strlen(request),TCP_WRITE_FLAG_COPY);
	return ERR_OK;
}

void start_download(){
	ip_addr_t remote_addr;
	ip4addr_aton(SERVER_IP, &remote_addr);

	client_pcb = tcp_new();
	tcp_recv(client_pcb, pcb_recv);
	tcp_connect(client_pcb,&remote_addr,SERVER_PORT,pcb_connected);
}

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
	fill_screen(0xFF,0,0); // Red
    if(cyw43_arch_init()){ // returns 0 on success
	    // wifi failure
	fill_screen(0,0,0xFF); // Blue
    }
    cyw43_arch_enable_sta_mode();
    if(cyw43_arch_wifi_connect_blocking(WIFI_SSID, WIFI_PASSWORD, CYW43_AUTH_WPA2_AES_PSK)){ // returns 0 on success
	    // network failure
	fill_screen(0xFF,0,0); // Red
    }
	fill_screen(0,0xFF,0); // Green

			       /*
    start_download();
    */
    while (1) {
	    if (get_bootsel_button()) {
		    bool held = true;
		    for (int i = 0; i < 10; i++) {
			    sleep_ms(10);
			    if (!get_bootsel_button()) { held = false; break; }
		    }
		    if (held) reset_usb_boot(0, 0);
	    }
	    sleep_ms(20);
    }
}
