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

    printk("received %d bytes, decompressed: %d\n", received_length, decompressed_length);


    caches_disable();

    uint32_t addr = put_code(destination, decompressed_length);
    not_reached();
}
