#ifndef __SHARED_H__
#define __SHARED_H__

#include "rpi.h"
#include "gpio-raw.h"

#define THRESHHOLD_SCALE 0.5
#define VARIANCE_GATE 0.005

enum { 
    NUM_BYTES_PER_PERIOD = 2,

    SAMPLE_RATE = 64000,
    NUM_FREQS = 8 * NUM_BYTES_PER_PERIOD,
    NUM_SAMPLES_VARIANCE = 10,
    // Most optimized I could get it with manual binary search while being reliable
    // (can just change to higher later/during demo if we want)
    SYMBOL_SAMPLES = 148,
    WINDOW_SAMPLES = SYMBOL_SAMPLES - NUM_SAMPLES_VARIANCE * 2,
    WINDOW_OFFSET  = (SYMBOL_SAMPLES - WINDOW_SAMPLES) / 2,

    MAX_AMPLITUDE = 0x40000000,
    AMPLITUDE = MAX_AMPLITUDE / NUM_FREQS,
    TABLE_BITS = 10,
    TABLE_SIZE = 1u << TABLE_BITS,

    SYNC_CALIBRATION_BYTE = 0xFF,
    NUM_CALIBRATION_SYMBOLS = 10,
    SYNC_MAGIC_BYTE = 0x0F,
    SYNC_END_BYTE = 0xF0,

    GPIO_FSEL_INPUT = 0,
    GPIO_FSEL_OUTPUT = 1,
    GPIO_FSEL_ALT0 = 4,
    AMPLITUDE_SCALE = 0,

    MIN_FREQ = 1000,
    MAX_FREQ = 16000,
    FREQ_SPACING = (MAX_FREQ - MIN_FREQ) / (NUM_FREQS - 1),

    PAYLOAD_MAX_BYTES = 4096,
    CHUNK_DELAY_US = 500,
};

struct payload {
    uint16_t size;
    uint32_t cksum;
    uint8_t data[PAYLOAD_MAX_BYTES];
};

typedef struct payload payload_t;

static void gpio_set_value(unsigned gpio, unsigned value) {
    unsigned reg = gpio / 10;
    unsigned shift = (gpio % 10) * 3;
    uint32_t val = GET32(GPIO_BASE + reg * 4);
    val &= ~(7u << shift);
    val |= (value & 7u) << shift;

    PUT32(GPIO_BASE + reg * 4, val);
}

static inline double get_tone(uint32_t index) {
    return (double)(MIN_FREQ + index * FREQ_SPACING);
}

static inline void bytes_to_bits(const uint8_t *bytes, uint8_t bits[NUM_FREQS]) {
    for (uint32_t i = 0; i < NUM_BYTES_PER_PERIOD; i++) {
        for (uint32_t j = 0; j < 8; j++) {
            bits[i * 8 + j] = (bytes[i] >> j) & 1;
        }
    }
}

enum {
    COMPRESS_STORED = 0,
    COMPRESS_LZSS = 1,
    COMPRESS_HEADER_BYTES = 5,

    LZSS_WINDOW_BITS = 12,
    LZSS_WINDOW_SIZE = 1 << LZSS_WINDOW_BITS,
    LZSS_LENGTH_BITS = 4,
    LZSS_MIN_MATCH = 3,
    LZSS_MAX_MATCH = (1 << LZSS_LENGTH_BITS) - 1 + LZSS_MIN_MATCH,
};

static inline uint32_t encode(const uint8_t *input, uint32_t input_length, uint8_t *output_buffer) {
    uint32_t input_index = 0, output_index = 0;
    while (input_index < input_length) {
        uint32_t flag_position = output_index++;
        uint8_t flag = 0;
        for (uint32_t bit = 0; bit < 8 && input_index < input_length; bit++) {
            uint32_t best_length = 0, best_distance = 0;
            uint32_t start = (input_index > LZSS_WINDOW_SIZE) ? (input_index - LZSS_WINDOW_SIZE) : 0;
            for (uint32_t j = start; j < input_index; j++) {
                uint32_t len = 0;
                while (len < LZSS_MAX_MATCH && input_index + len < input_length && input[j + len] == input[input_index + len]) {
                    len++;
                }
                if (len > best_length) {
                    best_length = len;
                    best_distance = input_index - j;
                }
            }
            if (best_length < LZSS_MIN_MATCH) {
                flag |= (1u << bit);
                output_buffer[output_index++] = input[input_index++];
            } else {
                uint32_t distance = best_distance - 1;
                uint32_t length = best_length - LZSS_MIN_MATCH;
                output_buffer[output_index++] = distance & 0xFF;
                output_buffer[output_index++] = ((distance >> 8) << 4) | length;
                input_index += best_length;
            }
        }
        output_buffer[flag_position] = flag;
    }
    return output_index;
}

static inline uint32_t compress(const uint8_t *input, uint32_t input_length, uint8_t *output_buffer) {
    uint32_t payload_len = encode(input, input_length, output_buffer + COMPRESS_HEADER_BYTES);
    if (payload_len < input_length) {
        output_buffer[0] = COMPRESS_LZSS;
    } else {
        output_buffer[0] = COMPRESS_STORED;
        memcpy(output_buffer + COMPRESS_HEADER_BYTES, input, input_length);
        payload_len = input_length;
    }
    output_buffer[1] = input_length & 0xFF;
    output_buffer[2] = (input_length >> 8) & 0xFF;
    output_buffer[3] = (input_length >> 16) & 0xFF;
    output_buffer[4] = (input_length >> 24) & 0xFF;

    // pad if odd
    uint32_t total = COMPRESS_HEADER_BYTES + payload_len;
    while (total % NUM_BYTES_PER_PERIOD != 0) {
        output_buffer[total++] = 0;
    }
    return total;
}

static inline void decode(const uint8_t *input, uint32_t input_length, uint8_t *output_buffer, uint32_t output_buffer_len) {
    uint32_t input_index = 0, output_index = 0;
    while (output_index < output_buffer_len && input_index < input_length) {
        uint8_t flag = input[input_index++];
        for (uint32_t bit = 0; bit < 8 && output_index < output_buffer_len && input_index < input_length; bit++) {
            if (flag & (1u << bit)) {
                output_buffer[output_index++] = input[input_index++];
            } else {
                uint8_t byte_zero = input[input_index++];
                uint8_t byte_one = input[input_index++];
                uint32_t distance = (((uint32_t)(byte_one >> 4) << 8) | byte_zero) + 1;
                uint32_t length = (uint32_t)(byte_one & 0x0F) + LZSS_MIN_MATCH;
                uint32_t source = output_index - distance;
                for (uint32_t i = 0; i < length; i++) {
                    output_buffer[output_index++] = output_buffer[source++];
                }
            }
        }
    }
}

static inline uint32_t decompress(const uint8_t *input, uint32_t input_length, uint8_t *output_buffer) {
    uint32_t original_length = (uint32_t)input[1] | ((uint32_t)input[2] << 8) | ((uint32_t)input[3] << 16) | ((uint32_t)input[4] << 24);
    if (input[0] == COMPRESS_STORED) {
        memcpy(output_buffer, input + COMPRESS_HEADER_BYTES, original_length);
    } else {
        decode(input + COMPRESS_HEADER_BYTES, input_length - COMPRESS_HEADER_BYTES, output_buffer, original_length);
    }
    return original_length;
}

#endif