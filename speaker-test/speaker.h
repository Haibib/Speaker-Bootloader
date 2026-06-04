#ifndef __SPEAKER_H__
#define __SPEAKER_H__

#include "rpi.h"
#include "cycle-count.h"
#include "gpio-raw.h"
#include "rpi-math.h"
#include "i2s.h"
#include "math-protos.h"
#include "pi-random.h"

#define SYNC_LENGTH 2

enum {
    gain_pin = 23,
    sd_pin = 25,
    GPIO_FSEL_INPUT = 0,
    GPIO_FSEL_OUTPUT = 1,
    GPIO_FSEL_ALT0 = 4,

    passwd = 0x5a,
    TABLE_BITS = 10, 
    TABLE_SIZE = 1u << TABLE_BITS,

    NUM_BYTES_PER_PERIOD = 1,
    SAMPLE_RATE = 59000,
    NUM_FREQS = 8 * NUM_BYTES_PER_PERIOD,
    MAX_AMPLITUDE = 0x40000000,
    AMPLITUDE = MAX_AMPLITUDE / NUM_FREQS,

    MIN_FREQ = 2000,
    MAX_FREQ = 16000,
    FREQ_BUCKET = (MAX_FREQ - MIN_FREQ) / (NUM_FREQS - 1),

    DURATION_us = 8678,
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

static int64_t sine_table[TABLE_SIZE];
static int ready = 0;
static uint32_t freqs[NUM_FREQS];
static uint32_t phase_steps[NUM_FREQS];
static uint32_t sine_phase = 0;
static uint32_t start_time = 0;
static uint32_t period_count = 0;

static inline void bytes_to_bits(const uint8_t *bytes, uint8_t bits[NUM_FREQS], uint32_t length) {
    for (uint32_t i = 0; i < length; i++) {
        for (uint32_t j = 0; j < 8; j++) {
            bits[i * 8 + j] = (bytes[i] >> j) & 1;
        }
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
            sine_table[i] = (int64_t)(sin(theta) * (double)0x7FFFFFFF);
        }
        ready = 1;
    }
    for (uint32_t i = 0; i < NUM_FREQS; i++) {
        freqs[i] = MIN_FREQ + i * FREQ_BUCKET;
        phase_steps[i] = (uint32_t)((double)freqs[i] / (double)SAMPLE_RATE * (double)(1ULL << 32));
    }
}

static inline void play_sine(uint32_t freq) {
    pcm_t *pcm = (pcm_t *)I2S_REGS_BASE;
    uint32_t num_samples = (uint32_t)((uint64_t)DURATION_us * SAMPLE_RATE / 1000000);
    uint32_t phase_step = (uint32_t)((double)freq / (double)SAMPLE_RATE * (double)(1ULL << 32));

    for (uint32_t i = 0; i < num_samples; i++) {
        uint32_t idx = sine_phase >> (32 - TABLE_BITS);
        uint32_t sample = ((int64_t)sine_table[idx] * MAX_AMPLITUDE) >> 31;

        while(!(pcm->cs_a & PCM_TXD)); 
        pcm->fifo_a = sample;
        while(!(pcm->cs_a & PCM_TXD)); 
        pcm->fifo_a = sample;

        sine_phase += phase_step;
    }
}

static inline void play_combined(uint8_t* bits) {
    pcm_t *pcm = (pcm_t *)I2S_REGS_BASE;
    uint32_t phases[NUM_FREQS] = {0};
    uint32_t deadline = start_time + (++period_count) * DURATION_us;

    while (timer_get_usec() < deadline) {
        int64_t sample = 0;
        for (uint32_t j = 0; j < NUM_FREQS; j++) {
            if (bits[j]) {
                uint32_t idx = phases[j] >> (32 - TABLE_BITS);
                sample += sine_table[idx];
            }
            phases[j] += phase_steps[j];
        }
        sample = (sample * (int64_t)AMPLITUDE) >> 31;

        while (!(pcm->cs_a & PCM_TXD));
        pcm->fifo_a = (uint32_t)sample;
        while (!(pcm->cs_a & PCM_TXD));
        pcm->fifo_a = (uint32_t)sample;
    }
}

static void play_random_start() {
    pcm_t *pcm = (pcm_t *)I2S_REGS_BASE;
    uint32_t deadline = start_time + (++period_count) * DURATION_us;
    uint32_t phase_step = (uint32_t)((double)MIN_FREQ / (double)SAMPLE_RATE * (double)(1ULL << 32));

    while (timer_get_usec() < deadline) {
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
    period_count = 0;
    start_time = timer_get_usec();
    play_random_start();
    uint8_t bits[NUM_FREQS];
    uint8_t bytes[SYNC_LENGTH] = { 0xFF, 0x0F };
    char debug_print[NUM_FREQS + 1];

    for (int i = 0; i < 1; i++) {
        bytes_to_bits(bytes, bits, SYNC_LENGTH);
        play_combined(bits);
    }
}

static inline void send_string(const char *str, uint32_t length, uint32_t verbose) {
    uint8_t bits[NUM_FREQS];
    uint8_t chunk[NUM_BYTES_PER_PERIOD];
    uint32_t num_periods = (length + NUM_BYTES_PER_PERIOD - 1) / NUM_BYTES_PER_PERIOD;
    char debug_print[length][NUM_FREQS + 1];
    uint32_t differences[length];

    for (uint32_t i = 0; i < num_periods; i++) {
        for (uint32_t j = 0; j < NUM_BYTES_PER_PERIOD; j++) {
            uint32_t index = i * NUM_BYTES_PER_PERIOD + j;
            chunk[j] = (index < length) ? (uint8_t)str[index] : 0;
        }
        bytes_to_bits(chunk, bits, NUM_BYTES_PER_PERIOD);
        uint32_t start = timer_get_usec();
        play_combined(bits);
        uint32_t end = timer_get_usec();
        differences[i] = end - start;
        if (verbose) {
            bits_to_str(bits, debug_print[i]);
        }
    }
    if (verbose) {
        for (uint32_t i = 0; i < length; i++) {
            printk("sending '%c': %s difference: %d\n", str[i], debug_print[i], differences[i]);
        }
    }
}

#endif