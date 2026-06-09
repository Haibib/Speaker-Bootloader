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

extern unsigned int gpio_blink_bin_len; 
#define program_length gpio_blink_bin_len
extern unsigned char gpio_blink_bin[]; 
#define program gpio_blink_bin

static int verbose = 1;
static int max_size = 5 * 1024; // 5 MB

void notmain(void) {
    caches_enable();
    i2s_init(SAMPLE_RATE);
    i2s_rx_enable();

    uint8_t compressed_data[max_size];
    uint32_t received_length = receive_data(compressed_data, verbose);
    
    uint8_t *destination = (uint8_t *)(HIGHEST_USED_ADDR + 0x100000);
    uint32_t decompressed_length = decompress(compressed_data, received_length, destination);

    printk("received %d bytes, decompressed: %d, expected %d\n", received_length, decompressed_length, program_length);

    uint32_t num_errors = 0;
    if (verbose) {
        for (uint32_t i = 0; i < decompressed_length; i++) {
            if (program[i] != destination[i]) {
                num_errors++;
            }
            //printk("%x ,", destination[i]);
        }
    }
    printk("num errors: %d\n", num_errors);

    // if (num_errors == 0) {
    //     uint32_t addr = put_code(destination, received_length);
    //     // if(!addr)
    //     //     rpi_reboot();

    //     // blx to addr.  
    //     // could also call it as a function pointer.
    //     //BRANCHTO(addr);
    //     not_reached();
    // }
}
