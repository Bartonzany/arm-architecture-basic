# Makefile — SMMUv3 裸机验证示例（顶层，委托给 src/Makefile）
BM_DIR := src
.PHONY: all run disasm clean
all: ; $(MAKE) -C $(BM_DIR) build disasm
run: ; $(MAKE) -C $(BM_DIR) run
disasm: ; $(MAKE) -C $(BM_DIR) disasm
clean: ; $(MAKE) -C $(BM_DIR) clean