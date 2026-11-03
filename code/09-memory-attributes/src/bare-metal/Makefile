# Makefile — MMU 内存属性验证裸机示例
# 产物输出到 ../../build/bare-metal/

CROSS   ?= aarch64-linux-gnu-
CC      := $(CROSS)gcc
LD      := $(CROSS)ld
OBJDUMP := $(CROSS)objdump
QEMU    ?= qemu-system-aarch64

# -mgeneral-regs-only：本示例不使能 FP/SIMD，禁止编译器用 FP 寄存器
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
# -semihosting 使程序末尾的 SYS_EXIT 能让 qemu 正常退出，无需 Ctrl-C。
run: $(TARGET)
	$(QEMU) -machine virt -cpu cortex-a53 -nographic \
	        -monitor none -net none -semihosting -kernel $(TARGET)

disasm: $(TARGET)
	$(OBJDUMP) -d --no-show-raw-insn $(TARGET) > $(OUTDIR)/kernel.dis

$(OUTDIR):
	mkdir -p $@

clean:
	rm -rf $(OUTDIR)