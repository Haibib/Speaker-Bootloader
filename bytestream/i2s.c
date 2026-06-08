#include "rpi.h"
#include "bit-support.h"
#include "i2s.h"
#include "shared.h"
// #include "speaker.h"

struct pcm_div {
    uint32_t divi, divf, mash;
};

// lets us hardcode and be really sure we did the right calculation.
struct pcm_div clock_to_div(uint32_t clock) {
    switch(clock) {
    case 44100: return (struct pcm_div) { .divi = 6, .divf = 3288, .mash=3 };
    default: panic("add the div for clock=%d\n", clock);
    }
}

void pcm_clock_init(uint32_t clock) {
    dev_barrier();

    // 1.turn off the pcm clock.
    // 2.wait til not busy.
    // 3. set the control and divisor

    /*
     * We want the fractional divider for
     *   divider = 19,200,000 / (sample_rate_hz * 64)
     *   = 300,000 / sample_rate_hz
     *   in units of 1/4096, this would be 4096 * 300,000 / sample_rate_hz
     *   = 1228800000 / sample_rate_hz
     */
    uint32_t scaled_value = 1228800000 / clock;
    clkdiv_t div = {
        .passwd = 0x5a,
        .divi = scaled_value >> 12,
        .divf = scaled_value & 0xfff,
    };

    clkctl_t ctl = { .passwd = 0x5a, .enab = 0, .src = BCM2835_CLK_SRC_OSC };
    PUT32(CM_PCM_CTRL, *(uint32_t*)&ctl);
    while (((clkctl_t*)CM_PCM_CTRL)->busy);

    PUT32(CM_PCM_DIV1, *(uint32_t*)&div);


    clkctl_t enable = { .passwd = 0x5a, .enab = 1, .src = BCM2835_CLK_SRC_OSC, .mash = BCM2835_CLK_MASH_1 };
    PUT32(CM_PCM_CTRL, *(uint32_t*)&enable);

    dev_barrier();
}

static void pcm_check_initial(void) {
    dev_barrier();
    pcm_t *p = (void*)I2S_REGS_BASE;

    // from the datasheet: p134
    if(!bit_is_on(p->cs_a, 21))
        panic("default not set?\n");
    if(!bit_is_on(p->cs_a, 19))
        panic("default not set?\n");
    if(!bit_is_on(p->cs_a, 17))
        panic("default not set?\n");

    // from the datasheet: p134
    if(bits_get(p->dreg_a, 24,30) != 0x10)
        panic("default not set?\n");
    if(bits_get(p->dreg_a, 16,22) != 0x30)
        panic("default not set?\n");
    if(bits_get(p->dreg_a, 8,14) != 0x30)
        panic("default not set?\n");
    if(bits_get(p->dreg_a, 0,6) != 0x20)
        panic("default not set?\n");

    // p 133
    if(p->txc_a != 0)
        panic("default not set?\n");

    // p 135
    if(p->inten_a != 0)
        panic("default not set?\n");

    if(p->intstc_a != 0)
        panic("default not set?\n");
    if(p->grey != 0)
        panic("default not set?\n");
}

void i2s_init(uint32_t clock) {
    clk_check_offsets();
    dev_barrier();

    gpio_set_value(pcm_clk, GPIO_FSEL_ALT0);
    gpio_set_value(pcm_fs, GPIO_FSEL_ALT0);
    gpio_set_value(pcm_din, GPIO_FSEL_ALT0);
    gpio_set_value(pcm_dout, GPIO_FSEL_ALT0);

    // this checks the invial values from the datasheet.
    pcm_check_initial();

    pcm_clock_init(clock);

    // set cs_a
    //
    // we want:
    //  - master mode
    //  - 48000 clock
    //  - 32 bit frames (T)
    //  - sync low for T/2, sync high for T/2 (sync = 50% duty cycle)
    //  - RX
    //  - no interrupt.
    //  - no grey
    // * configure before you do enable [datasheet is confusing]
    pcm_t *pcm = (pcm_t *)I2S_REGS_BASE;
    pcm->cs_a = PCM_EN;
    dev_barrier();


    // page 129
    // mode bits:
    // these we don't set but are all 0s:
    //  - don't disable clock [28=0]
    //  - no decimation(?) [27=0]
    //  - disable pdm [26=0]
    //  - don't received packed [25=0]
    //  - don't xmit packed [24=0]
    //  - don't invert clock [22=0]
    //  - don't invert frame [20=0]
    //
    // 
    // these we do set [many 0s]:
    //  - FSM[23]: master mode [23=0]
    //  - FLEN[19:10]: frame length = 24 bits, one bit per clock.
    //    [23 << 10]
    //  - FSLEN[9:0]: frame sync length [one down or one up: so half FLEN]
    //    [12]
    pcm->mode_a = (63u << 10) | 32u;


    // page 131: receive config: rxc_a
    pcm->rxc_a = (1u << 31) | (1u << 30) | (1u << 20) | (8u << 16);
    pcm->txc_a = (1u  << 31) | (1u << 30) | (1u << 20) | (8u << 16) |(1u  << 15) | (1u << 14) | (33u << 4) | 8u;

    pcm->cs_a = PCM_EN | PCM_TXCLR | PCM_RXCLR | PCM_SYNC;
    while(!(pcm->cs_a & PCM_SYNC));
    dev_barrier();
}

uint32_t i2s_get32(void) {
    pcm_t *pointer = (pcm_t *)I2S_REGS_BASE;
    while(!(pointer->cs_a & PCM_RXD));
    return pointer->fifo_a;
}


void i2s_tx_enable(void) {
    pcm_t *pcm = (pcm_t *)I2S_REGS_BASE;

    // clear fifo
    pcm->cs_a = PCM_EN | PCM_TXCLR | PCM_SYNC;
    while(!(pcm->cs_a & PCM_SYNC));

    pcm->cs_a = PCM_EN | PCM_TXON;
    dev_barrier();
}

void i2s_rx_enable(void) {
    pcm_t *pcm = (pcm_t *)I2S_REGS_BASE;

    // clear fifo
    pcm->cs_a = PCM_EN | PCM_RXCLR | PCM_SYNC;
    while(!(pcm->cs_a & PCM_SYNC));

    pcm->cs_a = PCM_EN | PCM_RXON;
    dev_barrier();
}

void i2s_put_frame(int64_t left, int64_t right) {
    pcm_t *pcm = (pcm_t *)I2S_REGS_BASE;

    while(!(pcm->cs_a & PCM_TXD));
    pcm->fifo_a = (uint32_t)left;

    while(!(pcm->cs_a & PCM_TXD));
    pcm->fifo_a = (uint32_t)right;
}