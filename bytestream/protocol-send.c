#include "rpi.h"
#include "i2s.h"
#include "speaker-helpers.h"
#include "boot-crc32.h"
#include "cpyjmp.h"

static uint32_t verbose = 1;

static inline int boot_has_data(void) {
    return uart_has_data();
}

static inline uint8_t boot_get8(void) {
    return uart_get8();
}

static void boot_put8(uint8_t x) {
    uart_put8(x);
}

#include "get-code.h"

#define MAX_PROGRAM_SIZE 1024*1024

void notmain(void) {
    uint8_t program[MAX_PROGRAM_SIZE];
    uint32_t program_length;
    get_code(program, &program_length, MAX_PROGRAM_SIZE);

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
    return;
}