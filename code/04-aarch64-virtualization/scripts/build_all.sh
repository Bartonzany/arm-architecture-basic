#!/usr/bin/env bash
# build_all.sh — 一键交叉编译 EL2 裸机虚拟化示例 + 反汇编 + 运行
#
# 用法：
#   ./scripts/build_all.sh            # 编译 + 反汇编
#   ./scripts/build_all.sh --run      # 再用 qemu 运行
#   ./scripts/build_all.sh clean      # 清理

set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BM="$ROOT/src/bare-metal"

if [[ "${1:-}" == "clean" ]]; then
    make -C "$BM" clean
    echo "已清理"
    exit 0
fi

echo "编译 EL2 裸机虚拟化示例（陷阱机制）..."
make -C "$BM" build disasm

echo
echo "产物："
echo "  裸机示例 : $ROOT/build/bare-metal/（kernel.elf + kernel.dis）"

if [[ "${1:-}" == "--run" ]]; then
    echo
    echo "===== EL2 陷阱机制（WFI trap）====="
    make -C "$BM" run || true
fi