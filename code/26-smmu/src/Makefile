# Makefile — SMMUv3 裸机验证示例
# 源码在本目录，构建产物统一输出到 ../build/

CROSS   ?= aarch64-linux-gnu-
CC      := $(CROSS)gcc
LD      := $(CROSS)ld
OBJDUMP := $(CROSS)objdump
QEMU    ?= qemu-system-aarch64

# -mgeneral-regs-only 禁用 FP/SIMD：本示例不使能 FP，避免 FP 陷阱。
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

# QEMU -machine virt,iommu=smmuv3：启用 SMMUv3（映射在 0x09050000）。
# -semihosting 使程序末尾的 SYS_EXIT 能让 qemu 正常退出。
run: $(TARGET)
	$(QEMU) -machine virt,iommu=smmuv3 -cpu cortex-a53 -smp 1 -nographic \
	        -monitor none -net none -semihosting -kernel $(TARGET)

disasm: $(TARGET)
	$(OBJDUMP) -d --no-show-raw-insn $(TARGET) > $(OUTDIR)/kernel.dis

$(OUTDIR):
	mkdir -p $@

clean:
	rm -rf $(OUTDIR)