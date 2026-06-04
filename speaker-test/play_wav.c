#include "rpi.h"
#include "speaker.h"
#include "i2s.h"
#include "gpio-raw.h"

static uint32_t verbose = 1;

void notmain(void) {
    amplifier_enable();
    i2s_init(SAMPLE_RATE);
    i2s_tx_enable();
    delay_ms(100);
    setup();
    
    char* test = "ABCDEFGHIJKLMNOPQRSTUVWXYZ";
    for (uint32_t i = 0; i < 1000000; i++) {
        initial_synchronization();
        send_string(test, strlen(test), verbose);
        delay_ms(1000);
    }

    // for (uint32_t i = 0; i < 10000; i++) {
    //     play_sine(3000);
    // }
}