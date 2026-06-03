#include "rpi.h"
#include "speaker.h"
#include "i2s.h"
#include "gpio-raw.h"

void notmain(void) {
    amplifier_enable();
    i2s_init(44100); // 44.1kHz
    i2s_tx_enable();
    delay_ms(100);

    uint32_t target_freq = 10000; 
    const uint32_t phase_step = (uint32_t)(((uint64_t)target_freq << 32) / 44100);
    const int32_t amp = 0x2000000;
    uint32_t phase = 0;

    while(1) {
        phase += phase_step;
        int32_t sample = (phase & 0x800000u) ? amp : -amp;
        i2s_put_frame(sample, sample);
    }
}