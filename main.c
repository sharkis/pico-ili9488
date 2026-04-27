#include <stdio.h>
#include "pico/stdlib.h"
#include "hardware/spi.h"
#include "ili9488.h"
#include "pico/cyw43_arch.h"
#include "lwip/tcp.h"

#define SPI_PORT spi0
#define PIN_MISO 0  // Physical Pin 1
#define PIN_CS   1  // Physical Pin 2
#define PIN_SCK  2  // Physical Pin 4
#define PIN_TX   3  // Physical Pin 5

#define PIN_DC   20 // Choose any free GPIO for Data/Command
#define PIN_RST  21 // Choose any free GPIO for Reset

#define SERVER_IP "192.168.1.223"
#define SERVER_PORT 3000
#define IMG_SIZE (320 * 480 * 2)

uint8_t full_image_buffer[IMG_SIZE];
uint32_t total_received = 0;
bool header_found = false;

struct tcp_pcb *client_pcb;

static err_t pcb_recv(void *arg, struct tcp_pcb *tpcb, struct pbuf *p, err_t err){
	if(p==NULL){
		// download finished
		draw_image_cpu(full_image_buffer,320,480);
		tcp_close(tpcb);
		return ERR_OK;
	}

	uint8_t *payload = (uint8_t *)p->payload;
	uint32_t len = p->len;
	uint32_t offset;
	if (!header_found) {
        // Search for the HTTP header end \r\n\r\n
        for (uint32_t i = 0; i < len - 4; i++) {
            if (payload[i] == '\r' && payload[i+1] == '\n' &&
                payload[i+2] == '\r' && payload[i+3] == '\n') {

                header_found = true;
                uint32_t img_data_start_offset = i + 4;
                uint32_t first_chunk_len = len - img_data_start_offset;

                // Copy first chunk into our big buffer
                if (first_chunk_len > 0) {
                    memcpy(full_image_buffer, payload + img_data_start_offset, first_chunk_len);
                    total_received = first_chunk_len;
                }
                break;
            }
        }
    } else {
        // Append data to the buffer, checking for overflow
        if (total_received + len <= IMG_SIZE) {
            memcpy(full_image_buffer + total_received, payload, len);
            total_received += len;
        } else {
            // Buffer safety: handle cases where server sends more than expected
            uint32_t remaining = IMG_SIZE - total_received;
            if (remaining > 0) {
                memcpy(full_image_buffer + total_received, payload, remaining);
                total_received += remaining;
            }
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
	fill_screen(0xFF, 0xFF, 0x00); // Red
    if(cyw43_arch_init()){ // returns 0 on success
	    // wifi failure
        fill_screen(0xFF, 0x00, 0x00); // Red
    }
    cyw43_arch_enable_sta_mode();
    if(cyw43_arch_wifi_connect_blocking(WIFI_SSID, WIFI_PASSWORD, CYW43_AUTH_WPA2_AES_PSK)){ // returns 0 on success
	    // network failure
        fill_screen(0xFF, 0x00, 0x00); // Red
    }

    start_download();
    while (1) {
	    tight_loop_contents();
    }
}
