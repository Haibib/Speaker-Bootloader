#include "rpi.h"
#include "i2s.h"
#include "speaker-helpers.h"
#include "boot-crc32.h"
#include "hello-world.h"

static uint32_t verbose = 1;

void notmain(void) {
    amplifier_enable();
    i2s_init(SAMPLE_RATE);
    i2s_tx_enable();
    delay_ms(100);
    setup();

    send_data(hello_world_bin, hello_world_bin_len);
}