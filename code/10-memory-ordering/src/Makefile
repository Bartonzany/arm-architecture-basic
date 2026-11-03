# Makefile — 内存排序与屏障验证裸机示例
# 产物输出到 ../build/

CROSS   ?= aarch64-linux-gnu-
CC      := $(CROSS)gcc
LD      := $(CROSS)ld
OBJDUMP := $(CROSS)objdump
QEMU    ?= qemu-system-aarch64

# -mgeneral-regs-only 禁止编译器用 FP/SIMD；FP 指令由 [E] 测试用内联汇编显式触发。
CFLAGS  := -O2 -ffreestanding -nostdlib -nostartfiles -fno-stack-protector \
           -fno-pic -fno-pie -march=armv8-a -mgeneral-regs-only

SRCDIR  := .
OUTDIR  := ../build

OBJS    := $(OUTDIR)/boot.o $(OUTDIR)/vector_table.o $(OUTDIR)/main.o
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

run: $(TARGET)
	$(QEMU) -machine virt -cpu cortex-a53 -nographic \
	        -monitor none -net none -semihosting -kernel $(TARGET)

disasm: $(TARGET)
	$(OBJDUMP) -d --no-show-raw-insn $(TARGET) > $(OUTDIR)/kernel.dis

$(OUTDIR):
	mkdir -p $@

clean:
	rm -rf $(OUTDIR)