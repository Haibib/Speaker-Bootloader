#include "rpi.h"
#include "i2s.h"
#include "mic-helpers.h"

#define DEBUG_INTERVAL 4

void notmain(void) {
    caches_enable();
    i2s_init(SAMPLE_RATE);
    i2s_rx_enable();

    const char *expected = "AABBCCDDEEFFGGHHIIJJKKLLMMNNOOPPQQRRSSTTUUVVWWXXYYZZ";
    uint32_t expected_length = strlen(expected);
    uint32_t num_periods  = (expected_length + NUM_BYTES_PER_PERIOD - 1) / NUM_BYTES_PER_PERIOD;
    uint8_t decoded[256];
    uint32_t attempt = 0;

    while (1) {
        uint32_t verbose = (attempt++ % DEBUG_INTERVAL == 0);
        if (!wait_for_sync(verbose)) {
            if (!verbose) printk(".");
            continue;
        }

        uint32_t out = 0;
        for (uint32_t p = 0; p < num_periods; p++) {
            uint8_t bytes[NUM_BYTES_PER_PERIOD];
            get_bytes(detect_mask(NULL), bytes);
            for (uint32_t j = 0; j < NUM_BYTES_PER_PERIOD && out < sizeof(decoded); j++) {
                decoded[out++] = bytes[j];
            }
        }

        uint32_t errors = 0;
        for (uint32_t i = 0; i < expected_length; i++) {
            if (decoded[i] != (uint8_t)expected[i]) {
                errors++;
                printk("byte %d: got %x, expected %x ('%c')\n",i, decoded[i], (uint8_t)expected[i], expected[i]);
            }
        }

        printk("decoded %d bytes, %d errors\n", expected_length, errors);
        for (uint32_t i = 0; i < expected_length; i++) {
            printk("%c", decoded[i]);
        }
        printk("\n");
    }
}