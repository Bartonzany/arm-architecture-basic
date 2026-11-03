# Makefile —— 交叉编译自动向量化验证示例，对比不同优化级别的反汇编
#
# 目标：
#   make            # 编译所有示例（-O1 无向量化 + -O3 向量化两个版本）
#   make disasm     # 反汇编各优化级别产物（build/disasm/*.txt）
#   make run        # 用 qemu-aarch64 运行（需安装 qemu-user）
#   make clean      # 清理 build/
#
# 说明：本目录用 GCC（aarch64-linux-gnu-gcc），等价于文档的 armclang。
#   文档 armclang 在 -O2 及以上默认开启自动向量化；GCC 对本例的循环在 -O3
#   才默认向量化（或显式加 -ftree-vectorize）。故本目录用 -O1 / -O3 对照，
#   效果与文档 -O1 / -O1 -fvectorize 一致：标量 FADD → 向量 FADD v0.4s。

CROSS       ?= aarch64-linux-gnu-
CC          := $(CROSS)gcc
OBJDUMP     := $(CROSS)objdump
QEMU        ?= qemu-aarch64

CFLAGS_O1   := -O1 -march=armv8-a+simd
CFLAGS_O3   := -O3 -march=armv8-a+simd

SRCDIR      := src
BUILDDIR    := build
DISASMDIR   := $(BUILDDIR)/disasm
BINDIR      := $(BUILDDIR)/bin

SOURCES     := $(wildcard $(SRCDIR)/*.c)
PROGS_O1    := $(patsubst $(SRCDIR)/%.c,$(BINDIR)/%_O1,$(SOURCES))
PROGS_O3    := $(patsubst $(SRCDIR)/%.c,$(BINDIR)/%_O3,$(SOURCES))

.PHONY: all disasm run clean

all: $(PROGS_O1) $(PROGS_O3)

$(BINDIR)/%_O1: $(SRCDIR)/%.c | $(BINDIR)
	$(CC) $(CFLAGS_O1) $< -o $@

$(BINDIR)/%_O3: $(SRCDIR)/%.c | $(BINDIR)
	$(CC) $(CFLAGS_O3) $< -o $@

disasm: all
	@mkdir -p $(DISASMDIR)
	@for o in O1 O3; do \
		for src in $(SOURCES); do \
			name=$$(basename $$src .c); \
			$(CC) $$([ $$o = O1 ] && echo $(CFLAGS_O1) || echo $(CFLAGS_O3)) -c $$src -o $(DISASMDIR)/$$name.o; \
			$(OBJDUMP) -d --no-show-raw-insn $(DISASMDIR)/$$name.o > $(DISASMDIR)/$${name}_$${o}.txt; \
			rm -f $(DISASMDIR)/$$name.o; \
		done; \
	done

run: all
	@for p in $(PROGS_O3); do \
		echo "===== $$(basename $$p) ====="; \
		$(QEMU) $$p; \
	done

$(BINDIR) $(DISASMDIR):
	mkdir -p $@

clean:
	rm -rf $(BUILDDIR)
