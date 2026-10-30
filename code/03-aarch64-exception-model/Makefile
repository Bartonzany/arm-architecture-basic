# Makefile — 交叉编译异常模型验证 demo + 裸机示例，并生成汇编/反汇编
#
# 目标：
#   make              # 编译所有 demo 为可执行文件（build/bin/）
#   make asm          # 生成所有 demo 的 -O2 汇编（build/asm/*.s）
#   make disasm       # 反汇编所有目标文件（build/disasm/*.txt）
#   make bare-metal   # 编译裸机示例（build/bare-metal/kernel.elf + kernel.dis）
#   make run          # 用 qemu-aarch64 运行所有 demo（需安装 qemu-user）
#   make clean        # 清理 build/

CROSS       ?= aarch64-linux-gnu-
CXX         := $(CROSS)g++
OBJDUMP     := $(CROSS)objdump
QEMU        ?= qemu-aarch64

CXXFLAGS    := -O2 -march=armv8-a+simd -std=c++14 \
               -fno-stack-protector -fno-stack-clash-protection

SRCDIR      := src
BUILDDIR    := build
BINDIR      := $(BUILDDIR)/bin
ASMDIR      := $(BUILDDIR)/asm
DISASMDIR   := $(BUILDDIR)/disasm
BMDIR       := $(SRCDIR)/bare-metal

SOURCES     := $(wildcard $(SRCDIR)/*.cpp)
PROGRAMS    := $(patsubst $(SRCDIR)/%.cpp,$(BINDIR)/%,$(SOURCES))
ASMS        := $(patsubst $(SRCDIR)/%.cpp,$(ASMDIR)/%.s,$(SOURCES))
DISASMS     := $(patsubst $(SRCDIR)/%.cpp,$(DISASMDIR)/%.txt,$(SOURCES))

.PHONY: all asm disasm run bare-metal clean

all: $(PROGRAMS)

# 编译为可执行（AArch64，需真机或 qemu 运行）
$(BINDIR)/%: $(SRCDIR)/%.cpp | $(BINDIR)
	$(CXX) $(CXXFLAGS) $< -o $@

# 生成汇编
$(ASMDIR)/%.s: $(SRCDIR)/%.cpp | $(ASMDIR)
	$(CXX) $(CXXFLAGS) -S $< -o $@

# 反汇编目标文件
$(DISASMDIR)/%.txt: $(SRCDIR)/%.cpp | $(DISASMDIR)
	$(CXX) $(CXXFLAGS) -c $< -o $(DISASMDIR)/$*.o
	$(OBJDUMP) -d --no-show-raw-insn $(DISASMDIR)/$*.o > $@
	rm -f $(DISASMDIR)/$*.o

asm: $(ASMS)

disasm: $(DISASMS)

# 裸机示例（向量表 + 异常处理器），产物 -> build/bare-metal/
bare-metal:
	$(MAKE) -C $(BMDIR) build disasm

run: all
	@for p in $(PROGRAMS); do \
		echo "===== $$(basename $$p) ====="; \
		$(QEMU) $$p; \
	done

$(BINDIR) $(ASMDIR) $(DISASMDIR):
	mkdir -p $@

clean:
	rm -rf $(BUILDDIR)