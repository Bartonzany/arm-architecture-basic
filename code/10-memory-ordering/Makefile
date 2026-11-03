# Makefile — AArch64 内存排序与屏障验证裸机示例（顶层，委托给 src/Makefile）
#
# 目标：
#   make            # 编译 + 反汇编
#   make run        # qemu-system-aarch64 运行
#   make clean

BM_DIR := src

.PHONY: all run disasm clean

all:
	$(MAKE) -C $(BM_DIR) build disasm

run:
	$(MAKE) -C $(BM_DIR) run

disasm:
	$(MAKE) -C $(BM_DIR) disasm

clean:
	$(MAKE) -C $(BM_DIR) clean