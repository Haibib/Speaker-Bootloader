#include "rpi.h"

void notmain(void) {
    gpio_set_output(27);
    while(1) {
        gpio_set_on(27);
        delay_cycles(1400000000);
        gpio_set_off(27);
        delay_cycles(1400000000);
    }
}