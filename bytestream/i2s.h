#ifndef __I2S_H__
#define __I2S_H__
#include "cycle-count.h"
#include "bit-support.h"

#define I2S_REGS_BASE 0x20203000

// see pg 102 of the BCM manual
enum {
    pcm_clk     = 18, // bclk
    pcm_fs      = 19, // lrc
    pcm_din     = 20, // din
    pcm_dout    = 21,
};


/*****************************************************************
 * clock related values.
 * for i2s lab we only use:
 *
 *  - BCM2835_CLK_SRC_OSC
 *  - CM_PCM_CTRL
 *  - CM_PCM_DIV1
 */

// stolen from linux
enum {
    BCM2835_CLK_MASH_0 = 0,
    BCM2835_CLK_MASH_1,
    BCM2835_CLK_MASH_2,
    BCM2835_CLK_MASH_3,
};

enum {
    BCM2835_CLK_SRC_GND = 0,
    BCM2835_CLK_SRC_OSC,
    BCM2835_CLK_SRC_DBG0,
    BCM2835_CLK_SRC_DBG1,
    BCM2835_CLK_SRC_PLLA,
    BCM2835_CLK_SRC_PLLC,
    BCM2835_CLK_SRC_PLLD,
    BCM2835_CLK_SRC_HDMI,
};

enum {
    CM_GP0CTL = 0x20101070,
    CM_DIV0 = 0x20101074,
    CM_GP1CTL = 0x20101078,
    CM_DIV1 = 0x2010107c,
    CM_GP2CTL = 0x20101080,
    CM_DIV2 = 0x20101084,

    // see broadcom errata
    CM_PCM_CTRL = 0x20101098,
    CM_PCM_DIV1 = 0x2010109C,
};

typedef struct clk_ctl {
    uint32_t
                src:4,  // 3:0
                enab:1, // 4
                kill:1, // 5
                    :1, // 6 unused
                busy:1, // 7
                flip:1, // 8
                mash:2, // 10:9
                    :13, // 23:11 unused
                passwd:8 // 24:31
    ;
} clkctl_t;
_Static_assert(sizeof(struct clk_ctl)==4, "invalid bitfields");

typedef struct clk_div {
    uint32_t divf:12,   // 0:11 fractional part of divisor
             divi:12,   // 0:11 integer part of divisor
             passwd:8;  // 0x5a
} clkdiv_t;

_Static_assert(sizeof(struct clk_div)==4, "invalid bitfields");


#include "helper-macros.h"

// helper to check bitfield offsets.
static inline void clk_check_offsets(void) {
    check_bitfield(clkctl_t, src, 0, 4);
    check_bitfield(clkctl_t, enab, 4, 1);
    check_bitfield(clkctl_t, kill, 5, 1);
    check_bitfield(clkctl_t, busy, 7, 1);
    check_bitfield(clkctl_t, mash, 9, 2);
    check_bitfield(clkctl_t, passwd, 24, 8);

    check_bitfield(clkdiv_t, divf, 0, 12);
    check_bitfield(clkdiv_t, divi, 12, 12);
    check_bitfield(clkdiv_t, passwd, 24, 8);
}

void pcm_clock_init(uint32_t clock);

/***********************************************************
 * i2s structs 
 */

// initialize the i2s driver.
void i2s_init(uint32_t clock);

typedef struct pcm {
    volatile uint32_t
        cs_a,       // pcm control and status
        fifo_a,     // fifo data
        mode_a,
        rxc_a,      // rx config
        txc_a,      // tx config
        dreg_a,     // dma req level
        inten_a,    // interrupt enables
        intstc_a,   // interrupt status and clear
        grey;
} pcm_t;

enum {
    PCM_EN = 1u << 0,
    PCM_RXON = 1u << 1,
    PCM_TXON = 1u << 2,
    PCM_TXCLR = 1u << 3,
    PCM_RXCLR = 1u << 4,
    PCM_SYNC = 1u << 24,

    PCM_TXD = 1u << 19,
    PCM_RXD = 1u << 20,
    PCM_BUSY = 1u << 7,
};

// return 32 bit value read from i2s.
uint32_t i2s_get32(void);
void i2s_tx_enable(void);
void i2s_rx_enable(void);

void i2s_put_frame(int64_t left, int64_t right);

// convert 32-bit val to signed 18-bit mic result
static inline long i2s_to_mic_val(uint32_t v) {
    uint32_t lo = bits_get(v, 0,13);
    if(lo != 0)
        panic("expected low 14 bits to be 0, have: %b\n", lo);
    return (long)v >> 14;
}
#endif
