#ifndef __SPEAKER_H__
#define __SPEAKER_H__

#include "rpi.h"
#include "gpio-raw.h"

enum {
    gain_pin = 23,
    sd_pin = 25,
    GPIO_FSEL_INPUT = 0,
    GPIO_FSEL_OUTPUT = 1,
    GPIO_FSEL_ALT0 = 4,

    passwd = 0x5a,
};

static void gpio_set_value(unsigned gpio, unsigned value) {
    unsigned reg = gpio / 10;
    unsigned shift = (gpio % 10) * 3;
    uint32_t val = GET32(GPIO_BASE + reg * 4);
    val &= ~(7u << shift);
    val |= (value & 7u) << shift;

    PUT32(GPIO_BASE + reg * 4, val);
}

#endif