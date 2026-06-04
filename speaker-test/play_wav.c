#include "rpi.h"
#include "speaker.h"
#include "i2s.h"
#include "gpio-raw.h"

void notmain(void) {
    amplifier_enable();
    i2s_init(SAMPLE_RATE);
    i2s_tx_enable();
    delay_ms(100);
    setup();

    // for (uint32_t i = 0; i < 100000; i++) {
    //     play_random_start();
    // }

    uint8_t bits[NUM_FREQS] = {1, 0, 1, 0, 0, 0, 0, 0, };
    for (uint32_t i = 0; i < 10000; i++) {
        play_combined(bits);
    }
}