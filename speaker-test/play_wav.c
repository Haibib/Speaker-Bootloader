#include "rpi.h"
#include "speaker.h"
#include "i2s.h"
#include "gpio-raw.h"

void notmain(void) {
    amplifier_enable();
    i2s_init(sample_rate);
    i2s_tx_enable();
    delay_ms(100);

    play_sine(220, 5000, 0x04000000, 44100);
    play_sine(440, 5000, 0x02000000, 44100);
    play_sine(880, 5000, 0x01000000, 44100);
}