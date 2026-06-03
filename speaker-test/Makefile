# PROGS += speaker-test.c
PROGS += play_wav.c

# if you need to override the install
# BOOTLOADER = my-install

COMMON_SRC += i2s.c

OPT_LEVEL = -Og

# automatically run the code
RUN = 1

LIBS += $(LIBM)
CFLAGS += $(LIBM_INC)
CFLAGS += -DPRINTF_DISABLE_SUPPORT_FLOAT=1 -DPRINTF_DISABLE_SUPPORT_EXPONENTIAL=1
CFLAGS += -DPRINTF_DISABLE_SUPPORT_PTRDIFF_T=1

include $(SPEAKER_BOOTLOADER_PATH)/libpi/mk/Makefile.robust-v3

.PHONY: FORCE
riscv_demo_hello.bin: FORCE
	riscv64-elf-gcc -mabi=ilp32 -march=rv32ia_zicsr_zifencei riscv_demo_hello.S -nostdlib -ffreestanding -o riscv_demo_hello.elf
	riscv64-elf-objcopy riscv_demo_hello.elf -O binary $@
