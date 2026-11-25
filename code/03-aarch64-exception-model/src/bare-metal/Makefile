# Makefile — 裸机 AArch64 异常模型验证示例（EL0–EL3）
# 源码在本目录，构建产物统一输出到 ../../build/bare-metal/

CROSS   ?= aarch64-linux-gnu-
CC      := $(CROSS)gcc
LD      := $(CROSS)ld
OBJDUMP := $(CROSS)objdump
QEMU    ?= qemu-system-aarch64

# -mgeneral-regs-only 禁用 FP/SIMD 寄存器：本示例不改 CPACR 使能 FP。
# NO_COLOR=1 编译时定义 -DNO_COLOR，关闭串口日志的 ANSI 颜色（重定向到文件时有用）。
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

# 关键：-machine virt,secure=on,virtualization=on 使 CPU 实现 EL3/EL2 并复位进入
# EL3；-kernel 加载 ELF 到 0x40000000。程序末尾死循环，按 Ctrl-C 退出。
run: $(TARGET)
	$(QEMU) -machine virt,secure=on,virtualization=on,gic-version=2 \
	        -cpu cortex-a53 -nographic -monitor none -net none \
	        -kernel $(TARGET)

disasm: $(TARGET)
	$(OBJDUMP) -d --no-show-raw-insn $(TARGET) > $(OUTDIR)/kernel.dis

$(OUTDIR):
	mkdir -p $@

clean:
	rm -rf $(OUTDIR)