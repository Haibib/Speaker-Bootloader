#include "rpi.h"
#include "i2s.h"
#include "speaker-helpers.h"

static uint32_t verbose = 1;

void notmain(void) {
    amplifier_enable();
    i2s_init(SAMPLE_RATE);
    i2s_tx_enable();
    delay_ms(100);
    setup();

    const char *msg = "AABBCCDDEEFFGGHHIIJJKKLLMMNNOOPPQQRRSSTTUUVVWWXXYYZZ";

    for (uint32_t i = 0; i < 2000; i++) {
        initial_synchronization();
        send_data((const uint8_t *)msg, strlen(msg), verbose);
        delay_ms(1000);
    }
}