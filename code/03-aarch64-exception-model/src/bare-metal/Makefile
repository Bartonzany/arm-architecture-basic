# Makefile — 裸机向量表 + 异常处理器示例
# 源码在本目录，构建产物统一输出到 ../../build/bare-metal/

CROSS   ?= aarch64-linux-gnu-
CC      := $(CROSS)gcc
LD      := $(CROSS)ld
OBJDUMP := $(CROSS)objdump
QEMU    ?= qemu-system-aarch64

# -mgeneral-regs-only 禁用 FP/SIMD 寄存器：本示例不改 CPACR_EL1 使能 FP，
# 若编译器为打包常量使用 d0/v0 会触发 FP/SIMD 陷阱（EC=0x07）导致死循环。
CFLAGS  := -O2 -ffreestanding -nostdlib -nostartfiles -fno-stack-protector \
           -fno-pic -fno-pie -march=armv8-a -mgeneral-regs-only

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

# QEMU -machine virt 默认 EL1 启动；-kernel 加载 ELF；-net none 避免缺 rom 报错。
# gic-version=2 使本示例用 GICv2 的 MMIO 寄存器（GICD/GICC）。
# -semihosting 使程序末尾的 SYS_EXIT 能让 qemu 正常退出，无需 Ctrl-C。
run: $(TARGET)
	$(QEMU) -machine virt,gic-version=2 -cpu cortex-a53 -nographic \
	        -monitor none -net none -semihosting -kernel $(TARGET)

disasm: $(TARGET)
	$(OBJDUMP) -d --no-show-raw-insn $(TARGET) > $(OUTDIR)/kernel.dis

$(OUTDIR):
	mkdir -p $@

clean:
	rm -rf $(OUTDIR)