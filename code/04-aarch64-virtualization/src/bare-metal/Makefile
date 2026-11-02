# Makefile — EL2 裸机虚拟化陷阱示例
# 产物输出到 ../../build/bare-metal/

CROSS   ?= aarch64-linux-gnu-
CC      := $(CROSS)gcc
LD      := $(CROSS)ld
OBJDUMP := $(CROSS)objdump
QEMU    ?= qemu-system-aarch64

# -mgeneral-regs-only：本示例不使能 CPACR 的 FP/SIMD，禁止编译器用 FP 寄存器
CFLAGS  := -O2 -ffreestanding -nostdlib -nostartfiles -fno-stack-protector \
           -fno-pic -fno-pie -march=armv8-a -mgeneral-regs-only

SRCDIR  := .
OUTDIR  := ../../build/bare-metal

OBJS    := $(OUTDIR)/boot.o $(OUTDIR)/vector_table.o $(OUTDIR)/guest.o
TARGET  := $(OUTDIR)/kernel.elf

.PHONY: all build run disasm clean

all: build

build: $(TARGET)

$(TARGET): $(OBJS)
	$(LD) -T $(SRCDIR)/linker.ld -o $@ $(OBJS)

$(OUTDIR)/%.o: $(SRCDIR)/%.S | $(OUTDIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(OUTDIR)/%.o: $(SRCDIR)/%.c | $(OUTDIR)
	$(CC) $(CFLAGS) -c $< -o $@

# virtualization=on 使 PE 从 EL2 启动；-semihosting 让程序末尾退出，无需 Ctrl-C
run: $(TARGET)
	$(QEMU) -machine virt,virtualization=on -cpu cortex-a53 -nographic \
	        -monitor none -net none -semihosting -kernel $(TARGET)

disasm: $(TARGET)
	$(OBJDUMP) -d --no-show-raw-insn $(TARGET) > $(OUTDIR)/kernel.dis

$(OUTDIR):
	mkdir -p $@

clean:
	rm -rf $(OUTDIR)