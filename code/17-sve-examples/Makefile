# Makefile —— 交叉编译 SVE/SVE2 示例，生成反汇编 + 运行
#
# 目标：
#   make            # 编译所有示例（-march=armv8-a+sve2）
#   make disasm     # 反汇编（build/disasm/*.txt）
#   make run        # 用 qemu-aarch64 -cpu max 运行
#   make clean      # 清理 build/

CROSS       ?= aarch64-linux-gnu-
CC          := $(CROSS)gcc
OBJDUMP     := $(CROSS)objdump
QEMU        ?= qemu-aarch64

CFLAGS      := -O2 -march=armv8-a+sve2

SRCDIR      := src
BUILDDIR    := build
BINDIR      := $(BUILDDIR)/bin
DISASMDIR   := $(BUILDDIR)/disasm

SOURCES     := $(wildcard $(SRCDIR)/*.c)
PROGRAMS    := $(patsubst $(SRCDIR)/%.c,$(BINDIR)/%,$(SOURCES))

.PHONY: all disasm run clean

all: $(PROGRAMS)

$(BINDIR)/%: $(SRCDIR)/%.c | $(BINDIR)
	$(CC) $(CFLAGS) $< -o $@

disasm: all
	@mkdir -p $(DISASMDIR)
	@for src in $(SOURCES); do \
		name=$$(basename $$src .c); \
		$(CC) $(CFLAGS) -c $$src -o $(DISASMDIR)/$$name.o; \
		$(OBJDUMP) -d --no-show-raw-insn $(DISASMDIR)/$$name.o > $(DISASMDIR)/$$name.txt; \
		rm -f $(DISASMDIR)/$$name.o; \
	done

run: all
	@for p in $(PROGRAMS); do \
		echo "===== $$(basename $$p) ====="; \
		$(QEMU) -cpu max $$p; \
	done

$(BINDIR) $(DISASMDIR):
	mkdir -p $@

clean:
	rm -rf $(BUILDDIR)
