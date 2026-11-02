# Makefile — EL1 裸机通用定时器示例（顶层，委托给 src/bare-metal/Makefile）
BM_DIR := src/bare-metal
.PHONY: all run disasm clean
all: ; $(MAKE) -C $(BM_DIR) build disasm
run: ; $(MAKE) -C $(BM_DIR) run
disasm: ; $(MAKE) -C $(BM_DIR) disasm
clean: ; $(MAKE) -C $(BM_DIR) clean
