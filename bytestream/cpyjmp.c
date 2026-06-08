// "bootloader" code that moves itself out of the way of incoming code so that
// we don't have to make an external mess of special makefiles, relinking, etc.
#include "rpi.h"
#include "cpyjmp.h"

#define cpy_out(args...) do { } while(0)

// copy the memory [<code>, <nbytes>] to <code_dst>.
// must be compiled to position independent code (you will check).
// 
// note:
//  - cannot call an external routine directly since it will be at a 
//    different location.  
//  - similarly cannot use global variables since the pc relative load 
//    won't work
//  - most likely should (but we don't) switch stacks to be safe during 
//    the copy.  this is probably the biggest possible source of bugs atm.
static inline void 
cpyjmp_internal(uint32_t code_dst, const void *code, unsigned nbytes) {
    // copy [code, code+nbytes) to <code_dst> and jump to <code_dst>.
    for (int i = 0; i < nbytes; i++)
        *(uint8_t *)(code_dst + i) = *((uint8_t *)code + i);
    ((void (*)(void))code_dst)();
    // the jump to <code_dst> should never return.
    // need a better safety net.
    asm volatile ("swi 1");
}


// make two routines that call <cpyjmp_internal> so can check 
// if position independent.  get the size by declaring empty
// non-static routines after.

void cpyjmp(uint32_t code_dst, const void *code, unsigned nbytes) {
    cpyjmp_internal(code_dst,code,nbytes);
}
void cpyjmp_end(void){}

void cpyjmp2(uint32_t code_dst, const void *code, unsigned nbytes) {
    cpyjmp_internal(code_dst,code,nbytes);
}
void cpyjmp2_end(void){}

// do some simple sanity checks that the generated binary looks as we
// expect.
void cpyjmp_check(void) {
    // check 1: make sure the two routines are the same size.
    // check 2: make sure gcc laid the routines out in order
    //          (the first should have a smaller address than second, etc)
    // check 3: make sure there is "enough" code in the routines (not just
    //          a few instructions.
    // check 4: the final important check: make sure both routines are 
    //          identical (memcpy==0).
    unsigned * f1_start = (unsigned *)cpyjmp;
    unsigned * f1_end = (unsigned *)cpyjmp_end;
    unsigned * f2_start = (unsigned *)cpyjmp2;
    unsigned * f2_end = (unsigned *)cpyjmp2_end;

    assert((f1_end - f1_start) == (f2_end - f2_start));
    assert(f1_start < f2_start);

    for (int i = 0; i < (f2_end - f2_start); i++) {
        assert(f1_start[i] == f2_start[i]);
    }
}

// copy <cpyjmp> to location <addr> and return a function pointer to the 
// new location.
cpyjmp_t cpyjmp_relocate(uint32_t addr) {
    assert(addr >= cpyjmp_lowest_reloc_addr);
    cpyjmp_check();
    unsigned * f1_start = (unsigned *)cpyjmp;
    unsigned * f1_end = (unsigned *)cpyjmp_end;
    for (int i = 0; i < (f1_end - f1_start); i++) {
        *((unsigned *)addr + i) = *(f1_start + i);
    }

    return (cpyjmp_t)addr;
}
