# Makefile —— 交叉编译 SVE SAXPY 三种实现 + 反汇编 + 运行
#
# 目标：
#   make            # 编译 C + 汇编，链接成可执行
#   make disasm     # 反汇编（build/disasm/*.txt）
#   make run        # 用 qemu-aarch64 -cpu max 运行
#   make clean      # 清理 build/

CROSS       ?= aarch64-linux-gnu-
CC          := $(CROSS)gcc
OBJDUMP     := $(CROSS)objdump
QEMU        ?= qemu-aarch64

CFLAGS      := -O3 -march=armv8-a+sve

SRCDIR      := src
BUILDDIR    := build
BINDIR      := $(BUILDDIR)/bin
DISASMDIR   := $(BUILDDIR)/disasm

C_SRC       := $(SRCDIR)/saxpy_example.c
ASM_SRC     := $(SRCDIR)/saxpy_asm.S
TARGET      := $(BINDIR)/saxpy

.PHONY: all disasm run clean

all: $(TARGET)

$(TARGET): $(C_SRC) $(ASM_SRC) | $(BINDIR)
	$(CC) $(CFLAGS) $(C_SRC) $(ASM_SRC) -o $@

disasm: all
	@mkdir -p $(DISASMDIR)
	$(CC) $(CFLAGS) -c $(C_SRC) -o $(DISASMDIR)/saxpy_example.o
	$(OBJDUMP) -d --no-show-raw-insn $(DISASMDIR)/saxpy_example.o > $(DISASMDIR)/saxpy_example.txt
	$(CC) $(CFLAGS) -c $(ASM_SRC) -o $(DISASMDIR)/saxpy_asm.o
	$(OBJDUMP) -d --no-show-raw-insn $(DISASMDIR)/saxpy_asm.o > $(DISASMDIR)/saxpy_asm.txt
	rm -f $(DISASMDIR)/*.o

run: all
	$(QEMU) -cpu max $(TARGET)

$(BINDIR) $(DISASMDIR):
	mkdir -p $@

clean:
	rm -rf $(BUILDDIR)
