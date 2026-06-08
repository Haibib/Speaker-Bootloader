#ifndef __GPIO_RAW_H__
#define __GPIO_RAW_H__
#include "cycle-count.h"

#ifndef GPIO_BASE
#   define GPIO_BASE 0x20200000
#endif
#define GPIO_LEV0 (void*)(GPIO_BASE + 0x34)
#define GPIO_CLR0 (void*)(GPIO_BASE + 0x28)
#define GPIO_SET0 (void*)(GPIO_BASE + 0x1C)

// force code to be aligned to 32-bytes.  
// NOTE: 
//   - this can cause the compiler to insert up to 7 nops --- 
//     so don't blast it all through timing critical code.  
//   - common pattern: stick it before the first timing 
//     critical instruction so everything is aligned and deterministic.  
//     otherwise the timings will bounce around as you modify other 
//     code that is linked before it, since that will shift all 
//     subsequent code around.
//   - another pattern: use it to align loops so that the back-edge
//     jump fetches an entire prefetch buffer of code.  
// alignment can be less important with icache
#define code_align() asm volatile (".align 5")

static inline void nop_raw(void) {
    asm volatile("nop");
}

// we use these instead of raw pointer manipulations so that its
// still relatively easy to drop in logging version and you don't
// have to modify much about your original <gpio.c> code.  you could
// sensibly decide these advantages are not worth the hassle.
static inline void put32_raw(volatile void * addr, uint32_t val) {
    *(volatile uint32_t *)addr = val;
}
static inline void PUT32_raw(uint32_t addr, uint32_t val) {
    put32_raw((void*)addr,val);
}
static inline uint32_t get32_raw(volatile void * addr) {
    return *(volatile uint32_t *)addr;
}
static inline uint32_t GET32_raw(uint32_t addr) {
    return get32_raw((void*)addr);
}

// raw versions of some <gpio.c> routines.

// remove all error checking and use put32_raw to assign
// to SET0. note: only handle pins [0..31)
static inline void gpio_set_on_raw(unsigned pin) {
    *(volatile uint32_t *)GPIO_SET0 = 1u << pin;
}

// remove all error checking and use put32_raw to set
// to CLR0. note: only handle pins [0..31)
static inline void gpio_set_off_raw(unsigned pin) {
    *(volatile uint32_t *)GPIO_CLR0 = 1u << pin;
}

// remove all error checking and use get32_raw to get LEV0.
// note: only handle  pins [0..31)
static inline uint32_t gpio_read_raw(unsigned pin) {
    return ((*(volatile uint32_t *)GPIO_LEV0) >> pin) & 1u;
}

// useful delay routines.

// delay <n> cycles starting from starting cycle <s> --- using
// <s> lets you do more accurate waiting than just blindly using <n>
static inline uint32_t delay_ncycles(unsigned s, unsigned n) {
    uint32_t e;

    // this handles wrap-around, but doing so adds two instructions,
    // which makes the delay not as tight.  (note: we could cut this
    // down by setting the cycle counter, but that breaks any code
    // that uses the cycle counter and calls us.)
    do {
        e = cycle_cnt_read();
    } while((e - s) < n);
    return e;
}

// write 1 for <ncycles>: since reading the cycle counter itself takes 
// non-zero cycles you may need to add a compensation constant to correct 
// for this.  use <delay_ncycles>
//
// [good puzzle: why is the result of messing with compensation not 
// linear? if puzzles are fun: compute the histogram of effects!]
static inline void write_1(unsigned pin, unsigned ncycles) {
    gpio_set_on_raw(pin);
    delay_ncycles(cycle_cnt_read(), ncycles - 90);
}

// write 0 for <ncycles>: since reading the cycle counter takes non-zero
// cycles you may need to add a compensation constant to correct for it.  
// use <delay_ncycles>
static inline void write_0(unsigned pin, unsigned ncycles) {
    gpio_set_off_raw(pin);
    delay_ncycles(cycle_cnt_read(), ncycles - 90);
}

#endif
