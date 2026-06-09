#include "rpi.h"
#include "i2s.h"
#include "speaker-helpers.h"
#include "boot-crc32.h"
#include "hello-world/hello-world.h"
#include "gpio-blink/gpio-blink.h"

static uint32_t verbose = 1;

extern unsigned int gpio_blink_bin_len; 
#define program_length gpio_blink_bin_len
extern unsigned char gpio_blink_bin[]; 
#define program gpio_blink_bin

void notmain(void) {
    amplifier_enable();
    i2s_init(SAMPLE_RATE);
    i2s_tx_enable();
    delay_ms(100);
    setup();

    // added space for headers if not compressible
    uint32_t max_length = program_length + (program_length / PAYLOAD_MAX_BYTES) * COMPRESS_HEADER_BYTES;
    uint8_t compressed_data[max_length];
    int compressed_length = compress(program, program_length, compressed_data);
    printk("Original length: %d, compressed length: %d\n", program_length, compressed_length);
    send_data(compressed_data, compressed_length);
}