#include "rpi.h"
#include "speaker.h"
#include "i2s.h"
#include "gpio-raw.h"

static void amplifier_enable(void)
{
    gpio_set_value(gain_pin, GPIO_FSEL_OUTPUT);
    gpio_set_value(sd_pin, GPIO_FSEL_OUTPUT);

    put32(GPIO_CLR0, 1u << gain_pin); // default 9bD
    put32(GPIO_SET0, 1u << sd_pin);
}

void notmain(void) {
    amplifier_enable();
    i2s_init(44100); // 44.1kHz
    i2s_tx_enable();
    delay_ms(100);

    // 440 * 2^32 / 441000
    const uint32_t phase_step = 42852281;
    const int32_t amp = 0x2000000;
    uint32_t phase = 0;

    while(1) {
        phase += phase_step;
        int32_t sample = (phase & 0x800000u) ? amp : -amp;
        i2s_put_frame(sample, sample);
    }
}