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

    // uint8_t bits[NUM_FREQS];
    // uint8_t bytes[SYNC_LENGTH] = { 0xFF };
    // initial_synchronization();
    // for (uint32_t i = 0; i < 13; i++) {
    //     bytes_to_bits(bytes, bits, SYNC_LENGTH);
    //     play_combined(bits);
    // }

    char* test = "AABBCCDDEEFFGGHHIIJJKKLLMMNNOOPPQQRRSSTTUUVVWWXXYYZZ";
    for (uint32_t i = 0; i < 2000; i++) {
        initial_synchronization();
        send_data((uint8_t*)test, strlen(test), verbose);
        delay_ms(1000);
    }

    
    // char* test = "A";
    // for (uint32_t i = 0; i < 20; i++) {
    //     initial_synchronization();
    //     send_string(test, strlen(test), verbose);
    //     delay_ms(1000);
    // }

    // for (uint32_t i = 0; i < 10000; i++) {
    //     play_sine(3000);
    // }
}