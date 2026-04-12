void lcd_write_cmd(uint8_t cmd);
void lcd_write_data(uint8_t data);
void lcd_init();
void fill_screen(uint8_t r, uint8_t g, uint8_t b);
void draw_image_cpu(const uint8_t *image_data, uint16_t w, uint16_t h);
