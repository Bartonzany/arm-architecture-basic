#!/usr/bin/env bash
# build_all.sh —— 一键交叉编译所有 Neon 内建函数验证 demo，并生成汇编与反汇编
#
# 依赖：aarch64-linux-gnu-g++、aarch64-linux-gnu-objdump
#       （apt 安装 g++-aarch64-linux-gnu binutils-aarch64-linux-gnu）
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

CXXFLAGS=(-O2 -march=armv8-a+simd -std=gnu++14 \
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
    echo "✅ 编译成功"

    # 汇编
    "$CXX" "${CXXFLAGS[@]}" -S "$src" -o "$ASMDIR/$name.s" 2>/dev/null || true

    # 反汇编
    "$CXX" "${CXXFLAGS[@]}" -c "$src" -o "$DISASMDIR/$name.o" 2>/dev/null || true
    "$OBJDUMP" -d --no-show-raw-insn "$DISASMDIR/$name.o" > "$DISASMDIR/$name.txt" 2>/dev/null || true
    rm -f "$DISASMDIR/$name.o" "$DISASMDIR/$name.err"
done

if [[ "$DO_RUN" -eq 1 ]]; then
    if command -v "$QEMU" >/dev/null 2>&1; then
        echo ""
        echo "===== 运行结果（qemu-aarch64）====="
        for p in "$BINDIR"/*; do
            echo "===== $(basename "$p") ====="
            "$QEMU" "$p"
        done
    else
        echo "提示: 未找到 $QEMU，跳过运行（apt install qemu-user）"
    fi
fi

[[ "$fail" -eq 0 ]] && echo "全部完成 ✔" || exit 1
