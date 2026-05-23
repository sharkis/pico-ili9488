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

#define SERVER_IP "192.168.12.212"
#define SERVER_PORT 3000
#define IMG_WIDTH  320
#define IMG_HEIGHT 480
#define IMG_SIZE   (IMG_WIDTH * IMG_HEIGHT * 3)
#define POLL_INTERVAL_MS 60000

static uint32_t total_received = 0;
static bool header_found = false;
static bool draw_started = false;
static bool download_in_progress = false;
static uint32_t last_download_ms = 0;

// Stream a pbuf chain to the LCD starting at byte offset within the chain.
static void stream_pbuf_to_lcd(struct pbuf *p, uint16_t offset) {
    struct pbuf *q = p;
    uint16_t skip = offset;

    // Skip whole pbufs that fall before the offset
    while (q != NULL && skip >= q->len) {
        skip -= q->len;
        q = q->next;
    }

    // Stream from each remaining pbuf in the chain
    while (q != NULL) {
        uint32_t remaining = IMG_SIZE - total_received;
        if (remaining == 0) break;

        uint16_t chunk_len = q->len - skip;
        if (chunk_len > remaining) chunk_len = (uint16_t)remaining;

        lcd_stream_data((const uint8_t *)q->payload + skip, chunk_len);
        total_received += chunk_len;
        skip = 0;
        q = q->next;
    }
}

struct tcp_pcb *client_pcb;
static err_t pcb_recv(void *arg, struct tcp_pcb *tpcb, struct pbuf *p, err_t err) {
    if (p == NULL) {
        // --- CONNECTION CLOSED BY SERVER ---
        printf("Download complete. Streamed %u bytes.\n", total_received);

        if (draw_started) {
            lcd_end_draw();
        }

        // Reset for next time
        header_found = false;
        draw_started = false;
        total_received = 0;
        download_in_progress = false;
        tcp_close(tpcb);
        return ERR_OK;
    }

    uint16_t packet_len = p->tot_len;

    if (!header_found) {
        // Find the HTTP header terminator (\r\n\r\n). Assume it fits in the
        // first pbuf chain prefix (typical HTTP response headers are small).
        uint8_t temp[512];
        uint16_t search_len = packet_len > 512 ? 512 : packet_len;
        pbuf_copy_partial(p, temp, search_len, 0);

        for (uint16_t i = 0; i + 3 < search_len; i++) {
            if (temp[i] == '\r' && temp[i+1] == '\n' &&
                temp[i+2] == '\r' && temp[i+3] == '\n') {

                header_found = true;
                uint16_t img_start_offset = i + 4;

                // Begin the LCD write window once; data will be streamed in
                lcd_begin_draw(0, 0, IMG_WIDTH - 1, IMG_HEIGHT - 1);
                draw_started = true;

                // Stream any image bytes that arrived after the header in this packet
                if (img_start_offset < packet_len) {
                    stream_pbuf_to_lcd(p, img_start_offset);
                }
                break;
            }
        }
    } else {
        stream_pbuf_to_lcd(p, 0);
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
	download_in_progress = true;
	last_download_ms = to_ms_since_boot(get_absolute_time());
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

    start_download();

    while (1) {
	    if (get_bootsel_button()) {
		    bool held = true;
		    for (int i = 0; i < 10; i++) {
			    sleep_ms(10);
			    if (!get_bootsel_button()) { held = false; break; }
		    }
		    if (held) reset_usb_boot(0, 0);
	    }
	    if (!download_in_progress &&
	        (to_ms_since_boot(get_absolute_time()) - last_download_ms) >= POLL_INTERVAL_MS) {
		    start_download();
	    }
	    sleep_ms(20);
    }
}
