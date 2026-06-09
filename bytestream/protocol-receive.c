#include "rpi.h"
#include "cpyjmp.h"

/*******************************************************
 * UART implementation of our routines.
 */

// non-blocking: returns 1 if there is data, 0 otherwise.
static inline int boot_has_data(void) {
    return uart_has_data();
}

// returns 8-bits from the network connection.
//
// should probably allow this to return a failure code
// (timeout, network error)
//
// can check by making sure you get a get_code() < 0 
// (not reboot, not lockup) if there is an error.
static inline uint8_t boot_get8(void) {
    return uart_get8();
}

// sends 8-bits on the network connection.
//
// should probably allow this to return a failure code
// (timeout, network error)
// 
// can check by making sure you get a get_code() < 0 
// (not reboot, not lockup) if there is an error.
static void boot_put8(uint8_t x) {
    uart_put8(x);
}


#include "get-code.h"
#include "hello-world/hello-world.h"
#include "gpio-blink/gpio-blink.h"

static int verbose = 1;

void notmain(void) {
    caches_enable();
    i2s_init(SAMPLE_RATE);
    i2s_rx_enable();
    uint8_t *destination = (uint8_t *)(HIGHEST_USED_ADDR + 0x100000);
    uint32_t received_length = receive_data(destination, verbose);
    printk("received %d bytes, expected %d\n", received_length, hello_world_bin_len);

    uint32_t num_errors = 0;
    if (verbose) {
        for (uint32_t i = 0; i < received_length; i++) {
            if (hello_world_bin[i] != destination[i]) {
                num_errors++;
            }
        }
    }
    printk("num errors: %d\n", num_errors);

    if (num_errors == 0) {
        uint32_t addr = put_code(destination, received_length);
        // if(!addr)
        //     rpi_reboot();

        // blx to addr.  
        // could also call it as a function pointer.
        //BRANCHTO(addr);
        not_reached();
    }
}
