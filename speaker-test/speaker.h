#ifndef __SPEAKER_H__
#define __SPEAKER_H__

#include "rpi.h"
#include "cycle-count.h"
#include "gpio-raw.h"
#include "rpi-math.h"
#include "i2s.h"
#include "math-protos.h"
#include "pi-random.h"

enum {
    gain_pin = 23,
    sd_pin = 25,
    GPIO_FSEL_INPUT = 0,
    GPIO_FSEL_OUTPUT = 1,
    GPIO_FSEL_ALT0 = 4,

    passwd = 0x5a,
    TABLE_BITS = 10, 
    TABLE_SIZE = 1u << TABLE_BITS,

    SAMPLE_RATE = 96000,
    NUM_FREQS = 8,
    MAX_AMPLITUDE = 0x10000000,
    AMPLITUDE = MAX_AMPLITUDE / NUM_FREQS,

    MIN_FREQ = 1000,
    MAX_FREQ = 8000,
    FREQ_BUCKET = (MAX_FREQ - MIN_FREQ) / NUM_FREQS,

    DURATION_us = 17066,
};

static void gpio_set_value(unsigned gpio, unsigned value) {
    unsigned reg = gpio / 10;
    unsigned shift = (gpio % 10) * 3;
    uint32_t val = GET32(GPIO_BASE + reg * 4);
    val &= ~(7u << shift);
    val |= (value & 7u) << shift;

    PUT32(GPIO_BASE + reg * 4, val);
}

static void amplifier_enable(void) {
    gpio_set_value(gain_pin, GPIO_FSEL_OUTPUT);
    gpio_set_value(sd_pin, GPIO_FSEL_OUTPUT);

    put32(GPIO_CLR0, 1u << gain_pin); // default 9bD
    put32(GPIO_SET0, 1u << sd_pin);
}

static int32_t sine_table[TABLE_SIZE];
static int ready = 0;
static uint32_t freqs[NUM_FREQS];
static uint32_t phase_steps[NUM_FREQS];
static uint32_t sine_phase = 0;
static uint32_t phases[NUM_FREQS] = {0};

static inline void bytes_to_bits(uint8_t first_byte, uint8_t second_byte, uint8_t bits[NUM_FREQS]) {
    for (uint32_t i = 0; i < 8; i++) {
        bits[i] = (first_byte >> i) & 1;
        bits[i + 8] = (second_byte >> i) & 1;
    }
}

static inline void bits_to_str(const uint8_t bits[NUM_FREQS], char out[NUM_FREQS + 1]) {
    for (uint32_t i = 0; i < NUM_FREQS; i++) {
        out[i] = bits[i] ? '1' : '0';
    }
    out[NUM_FREQS] = '\0';
}

static inline void setup(){
    if (!ready) {
        for (unsigned i = 0; i < TABLE_SIZE; i++) {
            double theta = (2.0 * M_PI * (double)i) / (double)TABLE_SIZE;
            sine_table[i] = (int32_t)(sin(theta) * (double)0x7FFFFFFF);
        }
        ready = 1;
    }
    for (uint32_t i = 0; i < NUM_FREQS; i++) {
        freqs[i] = MIN_FREQ + i * FREQ_BUCKET;
        phase_steps[i] = (uint32_t)((double)freqs[i] / (double)SAMPLE_RATE * (double)(1ULL << 32));
    }
}

static inline void play_sine(uint32_t freq, int32_t amplitude, uint32_t sample_rate) {
    pcm_t *pcm = (pcm_t *)I2S_REGS_BASE;
    uint32_t num_samples = (uint32_t)((uint64_t)DURATION_us * sample_rate / 1000000);
    uint32_t phase_step = (uint32_t)((double)freq / (double)sample_rate * (double)(1ULL << 32));

    for (uint32_t i = 0; i < num_samples; i++) {
        uint32_t idx = sine_phase >> (32 - TABLE_BITS);
        uint32_t sample = ((int64_t)sine_table[idx] * amplitude) >> 31;

        while(!(pcm->cs_a & PCM_TXD)); 
        pcm->fifo_a = sample;
        while(!(pcm->cs_a & PCM_TXD)); 
        pcm->fifo_a = sample;

        sine_phase += phase_step;
    }
}

static inline void play_combined(uint8_t* bits) {
    pcm_t *pcm = (pcm_t *)I2S_REGS_BASE;
    uint32_t num_samples = (uint32_t)((uint64_t)DURATION_us * SAMPLE_RATE / 1000000);

    for (uint32_t i = 0; i < num_samples; i++) {
        int64_t accum = 0;
        for (uint32_t j = 0; j < NUM_FREQS; j++) {
            if (bits[j]) {
                uint32_t idx = phases[j] >> (32 - TABLE_BITS);
                accum += sine_table[idx];
            }
            phases[j] += phase_steps[j];
        }
        int32_t sample = (int32_t)((accum * (int64_t)AMPLITUDE) >> 31);

        while (!(pcm->cs_a & PCM_TXD));
        pcm->fifo_a = (uint32_t)sample;
        while (!(pcm->cs_a & PCM_TXD));
        pcm->fifo_a = (uint32_t)sample;
    }
}

static void play_random_start() {
    pcm_t *pcm = (pcm_t *)I2S_REGS_BASE;
    uint32_t num_samples = (uint32_t)((uint64_t)DURATION_us * SAMPLE_RATE / 1000000);
    uint32_t phase_step = (uint32_t)((double)MIN_FREQ / (double)SAMPLE_RATE * (double)(1ULL << 32));

    for (uint32_t i = 0; i < num_samples; i++) {
        uint32_t random_amplitude = pi_random();
        uint32_t idx = sine_phase >> (32 - TABLE_BITS);
        uint32_t sample = ((int64_t)sine_table[idx] * random_amplitude) >> 31;

        while(!(pcm->cs_a & PCM_TXD)); 
        pcm->fifo_a = sample;
        while(!(pcm->cs_a & PCM_TXD)); 
        pcm->fifo_a = sample;

        sine_phase += phase_step;
    }
}

static inline void initial_synchronization() {
    // char debug_print[NUM_FREQS + 1];
    // bits_to_str(bits, debug_print);
    // printk("sending '%c' '%c': %s\n", first_byte ? second_byte : '?', debug_print);
    play_random_start();
    uint8_t bits[NUM_FREQS];
    for (uint32_t i = NUM_FREQS - 1; i > 0; i--) {
        for (uint32_t j = 0; j < NUM_FREQS; j++) {
            bits[j] = j < i ? 1 : 0;
        }
        play_combined(bits);
    }
}

static inline void send_string(const char *str) {
    uint8_t bits[NUM_FREQS];
    char debug_print[NUM_FREQS + 1];

    while (*str) {
        uint8_t first_byte = (uint8_t)*str++;
        uint8_t second_byte = str ? (uint8_t)*str++ : 0;
        bytes_to_bits(first_byte, second_byte, bits);
        bits_to_str(bits, debug_print);
        printk("sending '%c' '%c': %s\n", first_byte, second_byte ? second_byte : '?', debug_print);

        play_combined(bits);
    }
}

#endif