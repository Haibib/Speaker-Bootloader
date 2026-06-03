#ifndef __SPEAKER_H__
#define __SPEAKER_H__

#include "rpi.h"
#include "cycle-count.h"
#include "gpio-raw.h"
#include "rpi-math.h"
#include "i2s.h"
#include "math-protos.h"

enum {
    gain_pin = 23,
    sd_pin = 25,
    GPIO_FSEL_INPUT = 0,
    GPIO_FSEL_OUTPUT = 1,
    GPIO_FSEL_ALT0 = 4,

    passwd = 0x5a,
    sample_rate = 44100,
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


static inline void play_sine(uint32_t freq, uint32_t duration, int32_t amplitude, uint32_t sample_rate) {
    enum { TABLE_BITS = 10, TABLE_SIZE = 1u << TABLE_BITS };
    static int32_t sine_table[TABLE_SIZE];
    static int ready = 0;
    if (!ready) {
        for (unsigned i = 0; i < TABLE_SIZE; i++) {
            double theta = (2.0 * M_PI * (double)i) / (double)TABLE_SIZE;
            sine_table[i] = (int32_t)(sin(theta) * (double)0x7FFFFFFF);
        }
        ready = 1;
    }
    pcm_t *pcm = (pcm_t *)I2S_REGS_BASE;
    uint32_t phase_step = (uint32_t)((double)freq / (double)sample_rate * (double)(1ULL << 32));
    uint32_t phase = 0;
    uint32_t num_samples = (uint32_t)((uint64_t)duration * sample_rate / 1000);

    for (uint32_t i = 0; i < num_samples; i++) {
        uint32_t idx = phase >> (32 - TABLE_BITS);
        uint32_t sample = ((int64_t)sine_table[idx] * amplitude) >> 31;

        while(!(pcm->cs_a & PCM_TXD)); 
        pcm->fifo_a = sample;
        while(!(pcm->cs_a & PCM_TXD)); 
        pcm->fifo_a = sample;

        phase += phase_step;
    }
}


#endif