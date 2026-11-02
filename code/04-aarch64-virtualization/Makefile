# Makefile — EL2 裸机虚拟化示例（顶层，委托给 src/bare-metal/Makefile）
#
# 目标：
#   make            # 编译 + 反汇编
#   make run        # qemu-system-aarch64 运行（virtualization=on）
#   make clean

BM_DIR := src/bare-metal

.PHONY: all run disasm clean

all:
	$(MAKE) -C $(BM_DIR) build disasm

run:
	$(MAKE) -C $(BM_DIR) run

disasm:
	$(MAKE) -C $(BM_DIR) disasm

clean:
	$(MAKE) -C $(BM_DIR) clean