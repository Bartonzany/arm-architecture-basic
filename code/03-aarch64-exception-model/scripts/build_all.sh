#!/usr/bin/env bash
# build_all.sh — 一键交叉编译所有 A64 验证 demo，并生成汇编与反汇编
#
# 依赖：aarch64-linux-gnu-g++、aarch64-linux-gnu-objdump（apt 安装 g++-aarch64-linux-gnu binutils-aarch64-linux-gnu）
# 可选：qemu-aarch64（apt install qemu-user），用于在 x86 主机上运行 demo
#
# 用法：
#   ./scripts/build_all.sh              # 编译 + 生成汇编 + 反汇编
#   ./scripts/build_all.sh --run        # 额外用 qemu 运行所有 demo
#   ./scripts/build_all.sh clean        # 清理 build/

set -euo pipefail

CROSS="${CROSS:-aarch64-linux-gnu-}"
CXX="${CROSS}g++"
OBJDUMP="${CROSS}objdump"
QEMU="${QEMU:-qemu-aarch64}"

CXXFLAGS=(-O2 -march=armv8-a+simd -std=c++14 \
          -fno-stack-protector -fno-stack-clash-protection)

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
SRCDIR="$ROOT/src"
BUILDDIR="$ROOT/build"
BINDIR="$BUILDDIR/bin"
ASMDIR="$BUILDDIR/asm"
DISASMDIR="$BUILDDIR/disasm"

DO_RUN=0
if [[ "${1:-}" == "--run" ]]; then DO_RUN=1; fi
if [[ "${1:-}" == "clean" ]]; then rm -rf "$BUILDDIR"; echo "已清理 $BUILDDIR"; exit 0; fi

command -v "$CXX" >/dev/null 2>&1 || { echo "错误: 找不到 $CXX（apt install g++-aarch64-linux-gnu）"; exit 1; }
command -v "$OBJDUMP" >/dev/null 2>&1 || { echo "错误: 找不到 $OBJDUMP"; exit 1; }

mkdir -p "$BINDIR" "$ASMDIR" "$DISASMDIR"

sources=("$SRCDIR"/*.cpp)
echo "发现 ${#sources[@]} 个 demo，开始编译..."

fail=0
for src in "${sources[@]}"; do
    name="$(basename "$src" .cpp)"
    printf '  - %-28s' "$name"
    if ! "$CXX" "${CXXFLAGS[@]}" "$src" -o "$BINDIR/$name" 2>"$DISASMDIR/$name.err"; then
        echo "❌ 编译失败"
        cat "$DISASMDIR/$name.err"
        fail=1
        continue
    fi
    # 生成汇编
    "$CXX" "${CXXFLAGS[@]}" -S "$src" -o "$ASMDIR/$name.s"
    # 反汇编目标文件
    "$CXX" "${CXXFLAGS[@]}" -c "$src" -o "$DISASMDIR/$name.o"
    "$OBJDUMP" -d --no-show-raw-insn "$DISASMDIR/$name.o" > "$DISASMDIR/$name.txt"
    rm -f "$DISASMDIR/$name.o" "$DISASMDIR/$name.err"
    echo "✅"
done

if [[ "$fail" -ne 0 ]]; then
    echo "存在编译失败，请检查上面输出。"
    exit 1
fi

# ---- 裸机示例：向量表 + 异常处理器（产物 -> build/bare-metal/）----
BMDIR="$ROOT/src/bare-metal"
if [[ -f "$BMDIR/Makefile" ]] && command -v make >/dev/null 2>&1; then
    echo
    echo "编译裸机示例（向量表 + 异常处理器）..."
    make -C "$BMDIR" build disasm
fi

echo
echo "产物："
echo "  可执行文件 : $BINDIR/"
echo "  汇编(.s)   : $ASMDIR/"
echo "  反汇编(.txt): $DISASMDIR/"
if [[ -d "$BUILDDIR/bare-metal" ]]; then
    echo "  裸机示例   : $BUILDDIR/bare-metal/（kernel.elf + kernel.dis）"
fi

if [[ "$DO_RUN" -eq 1 ]]; then
    if ! command -v "$QEMU" >/dev/null 2>&1; then
        echo "未找到 $QEMU，跳过运行（可 apt install qemu-user）"
        exit 0
    fi
    echo
    for bin in "$BINDIR"/*; do
        echo "===== $(basename "$bin") ====="
        "$QEMU" "$bin" || true
    done

    # 裸机示例用 qemu-system-aarch64 运行（semihosting 让程序自己退出，无需 timeout）
    if [[ -f "$BUILDDIR/bare-metal/kernel.elf" ]] && command -v qemu-system-aarch64 >/dev/null 2>&1; then
        echo
        echo "===== bare-metal 向量表 + 异常处理器 ====="
        qemu-system-aarch64 -machine virt,gic-version=2 -cpu cortex-a53 \
            -nographic -monitor none -net none -semihosting \
            -kernel "$BUILDDIR/bare-metal/kernel.elf" || true
    fi
fi