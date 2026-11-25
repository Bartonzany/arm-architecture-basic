# Makefile — 裸机 Arm 通用定时器验证示例（EL3，参考 ARM 官方 generic_timer）
# 源码在本目录，构建产物统一输出到 ../../build/bare-metal/

CROSS   ?= aarch64-linux-gnu-
CC      := $(CROSS)gcc
LD      := $(CROSS)ld
OBJDUMP := $(CROSS)objdump
QEMU    ?= qemu-system-aarch64

# -mgeneral-regs-only 禁用 FP/SIMD 寄存器（本示例不改 CPTR_EL3 使能 FP）。
CFLAGS  := -O2 -ffreestanding -nostdlib -nostartfiles -fno-stack-protector \
           -fno-pic -fno-pie -march=armv8-a -mgeneral-regs-only
ifneq ($(NO_COLOR),)
CFLAGS  += -DNO_COLOR
endif

SRCDIR  := .
OUTDIR  := ../../build/bare-metal

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

# 关键：-machine virt,secure=on 使 CPU 复位进入 EL3（Secure 世界），全程 EL3
# 运行；GIcv2 让示例用 GICD/GICC MMIO；定时器中断配成 Group0（FIQ）路由到 EL3。
# 程序末尾死循环，按 Ctrl-C 退出。
run: $(TARGET)
	$(QEMU) -machine virt,secure=on,gic-version=2 -cpu cortex-a53 -nographic \
	        -monitor none -net none -kernel $(TARGET)

disasm: $(TARGET)
	$(OBJDUMP) -d --no-show-raw-insn $(TARGET) > $(OUTDIR)/kernel.dis

$(OUTDIR):
	mkdir -p $@

clean:
	rm -rf $(OUTDIR)