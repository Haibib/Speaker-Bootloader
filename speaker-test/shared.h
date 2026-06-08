#ifndef __SHARED_H__
#define __SHARED_H__

#include "rpi.h"
#include "gpio-raw.h"


#define THRESHHOLD_SCALE 0.35
#define VARIANCE_GATE 0.001

enum { 
    NUM_BYTES_PER_PERIOD = 1,

    SAMPLE_RATE = 59000,
    NUM_FREQS = 8 * NUM_BYTES_PER_PERIOD,
    SYMBOL_SAMPLES = 1024,
    WINDOW_SAMPLES = 512,
    WINDOW_OFFSET  = (SYMBOL_SAMPLES - WINDOW_SAMPLES) / 2,


    RX_WORDS_PER_FRAME = 1,
    SPEAKER_FRAMES_PER_SYMBOL = SYMBOL_SAMPLES / RX_WORDS_PER_FRAME,


    TONE_BASE_BIN = 9,
    TONE_BIN_SPACING = 8,

    MAX_AMPLITUDE = 0x40000000,
    AMPLITUDE = MAX_AMPLITUDE / NUM_FREQS,
    TABLE_BITS = 10,
    TABLE_SIZE = 1u << TABLE_BITS,


    SYNC_CALIBRATION_BYTE = 0xFF,
    SYNC_MAGIC_BYTE = 0x0F,

    GPIO_FSEL_INPUT = 0,
    GPIO_FSEL_OUTPUT = 1,
    GPIO_FSEL_ALT0 = 4,

    AMPLITUDE_SCALE = 0,
};

static void gpio_set_value(unsigned gpio, unsigned value) {
    unsigned reg = gpio / 10;
    unsigned shift = (gpio % 10) * 3;
    uint32_t val = GET32(GPIO_BASE + reg * 4);
    val &= ~(7u << shift);
    val |= (value & 7u) << shift;

    PUT32(GPIO_BASE + reg * 4, val);
}

static inline double get_tone(uint32_t index) {
    uint32_t bin = TONE_BASE_BIN + index * TONE_BIN_SPACING;
    return (double)bin * (double)SAMPLE_RATE / (double)WINDOW_SAMPLES;
}

static inline void bytes_to_bits(const uint8_t *bytes, uint8_t bits[NUM_FREQS]) {
    for (uint32_t i = 0; i < NUM_BYTES_PER_PERIOD; i++) {
        for (uint32_t j = 0; j < 8; j++) {
            bits[i * 8 + j] = (bytes[i] >> j) & 1;
        }
    }
}

#endif