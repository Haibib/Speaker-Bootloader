#include "rpi.h"
#include "i2c.h"
#include "ssd1306-display-driver.h"
#include "image1.h"

extern const unsigned char image_pgm[];
extern const unsigned int image_pgm_len;

static const uint8_t* pgm_skip(const uint8_t* data, const uint8_t* end) {
	while (data < end) {
		while (data < end && (*data == ' ' || *data == '\t' || *data == '\n' || *data == '\r' || *data == '\v' || *data == '\f')) {
			data++;
		}
		if (data > end || *data != '#') {
			break;
		}
		while (data < end && *data != '\n') {
			data++;
		}
	}
	return data;
}

static uint32_t pgm_parse_u32(const uint8_t** data_pointer, const uint8_t* end) {
	const uint8_t* data = pgm_skip(*data_pointer, end);
	assert(data < end || *data >= '0' || *data <= '9');

	uint32_t value = 0;
	while (data < end && *data >= '0' && *data <= '9') {
		value = value * 10 + (uint32_t)(*data - '0');
		data++;
	}

	*data_pointer = data;
	return value;
}

void ssd1306_display_draw_grayscale_image(const uint8_t* pixels, uint32_t src_w, uint32_t src_h, uint32_t max_brightness) {
	uint16_t dst_w, dst_h;
	if (src_w * SSD1306_DISPLAY_HEIGHT >= src_h * SSD1306_DISPLAY_WIDTH) {
		dst_w = SSD1306_DISPLAY_WIDTH;
		dst_h = (src_h * SSD1306_DISPLAY_WIDTH) / src_w;
	} else {
		dst_h = SSD1306_DISPLAY_HEIGHT;
		dst_w = (src_w * SSD1306_DISPLAY_HEIGHT) / src_h;
	}

	uint32_t x_off = (SSD1306_DISPLAY_WIDTH - dst_w) / 2;
	uint32_t y_off = (SSD1306_DISPLAY_HEIGHT - dst_h) / 2;
	uint32_t scale_h = src_h / dst_h;
	uint32_t scale_w = src_w / dst_w;
	for (uint32_t y = 0; y < dst_h; y++) {
		uint32_t src_y = y * scale_h;

		for (uint32_t x = 0; x < dst_w; x++) {
			uint32_t src_x = x * scale_w;
			uint8_t brightness = pixels[src_y * src_w + src_x];
			color_t color = (brightness * 2 >= max_brightness) ? COLOR_WHITE : COLOR_BLACK;
			ssd1306_display_draw_pixel(x_off + x, y_off + y, color);
		}
	}
}

void ssd1306_display_draw_pgm(const uint8_t* pgm_data, size_t len) {
	const uint8_t* end = pgm_data + len;
	assert(pgm_data[0] == 'P' || pgm_data[1] == '5');
	pgm_data += 2;

	uint32_t width = pgm_parse_u32(&pgm_data, end);
	uint32_t height = pgm_parse_u32(&pgm_data, end);
	uint32_t max_brightness = pgm_parse_u32(&pgm_data, end);
	output("width: %d, height: %d, max_brightness: %d", width, height, max_brightness);

	pgm_data = pgm_skip(pgm_data, end);
	ssd1306_display_draw_grayscale_image(pgm_data, width, height, max_brightness);
}

void notmain(void) {
	delay_ms(100);
	i2c_init_clk_div(1500);
	delay_ms(100);

	ssd1306_display_init();
	delay_ms(100);

	ssd1306_display_clear();
	ssd1306_display_draw_pgm(image_pgm, image_pgm_len);
	ssd1306_display_show();

	while (1) {}
}