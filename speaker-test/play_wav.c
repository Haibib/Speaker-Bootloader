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

    for (uint32_t i = 0; i < 100000; i++) {
        play_random_start();
    }


    // for (uint32_t i = 0; i < NUM_FREQS; i++) {
    //     play_sine(MIN_FREQ + i * FREQ_BUCKET, 2000, MAX_AMPLITUDE, SAMPLE_RATE);
    // }

    //play_sine(MIN_FREQ, MAX_AMPLITUDE, SAMPLE_RATE);
    // play_combined(200000);
}