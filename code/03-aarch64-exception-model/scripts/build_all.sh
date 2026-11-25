#!/usr/bin/env bash
# build_all.sh — 一键构建 AArch64 异常模型裸机验证代码，并生成反汇编
#
# 依赖：aarch64-linux-gnu-gcc、aarch64-linux-gnu-objdump
#       （apt install gcc-aarch64-linux-gnu binutils-aarch64-linux-gnu）
# 可选：qemu-system-aarch64（apt install qemu-system-arm），用于 --run
#
# 用法：
#   ./scripts/build_all.sh              # 编译（产物进 build/bare-metal/）
#   ./scripts/build_all.sh --run        # 额外用 qemu 运行
#   ./scripts/build_all.sh clean        # 清理 build/

set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BMDIR="$ROOT/src/bare-metal"

DO_RUN=0
if [[ "${1:-}" == "--run" ]]; then DO_RUN=1; fi
if [[ "${1:-}" == "clean" ]]; then
    make -C "$BMDIR" clean
    echo "已清理 build/bare-metal/"
    exit 0
fi

echo "构建裸机示例（EL0–EL3 异常模型）..."
make -C "$BMDIR" build disasm

echo
echo "产物："
echo "  可执行文件 : $ROOT/build/bare-metal/kernel.elf"
echo "  反汇编     : $ROOT/build/bare-metal/kernel.dis"

if [[ "$DO_RUN" -eq 1 ]]; then
    if ! command -v qemu-system-aarch64 >/dev/null 2>&1; then
        echo "未找到 qemu-system-aarch64，跳过运行（apt install qemu-system-arm）"
        exit 0
    fi
    echo
    echo "===== bare-metal EL0-EL3 异常模型 ====="
    make -C "$BMDIR" run
fi