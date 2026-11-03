#!/usr/bin/env bash
# build_all.sh —— 交叉编译 SVE2 示例 + 反汇编 + 运行
#
# 依赖：aarch64-linux-gnu-gcc（GCC 10+ 支持 SVE2）、aarch64-linux-gnu-objdump
#       （apt 安装 gcc-aarch64-linux-gnu binutils-aarch64-linux-gnu）
# 可选：qemu-aarch64（apt install qemu-user），用 -cpu max 运行 SVE2
#
# 用法：
#   ./scripts/build_all.sh              # 编译 + 反汇编
#   ./scripts/build_all.sh --run        # 额外用 qemu-aarch64 -cpu max 运行
#   ./scripts/build_all.sh clean        # 清理 build/

set -euo pipefail

CROSS="${CROSS:-aarch64-linux-gnu-}"
CC="${CROSS}gcc"
OBJDUMP="${CROSS}objdump"
QEMU="${QEMU:-qemu-aarch64}"

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
SRCDIR="$ROOT/src"
BUILDDIR="$ROOT/build"
BINDIR="$BUILDDIR/bin"
DISASMDIR="$BUILDDIR/disasm"

DO_RUN=0
if [[ "${1:-}" == "--run" ]]; then DO_RUN=1; fi
if [[ "${1:-}" == "clean" ]]; then rm -rf "$BUILDDIR"; echo "已清理 $BUILDDIR"; exit 0; fi

command -v "$CC" >/dev/null 2>&1 || { echo "错误: 找不到 $CC（apt install gcc-aarch64-linux-gnu）"; exit 1; }
command -v "$OBJDUMP" >/dev/null 2>&1 || { echo "错误: 找不到 $OBJDUMP"; exit 1; }

mkdir -p "$BINDIR" "$DISASMDIR"

CFLAGS="-O2 -march=armv8-a+sve2"

echo "交叉编译 SVE2 示例（-march=armv8-a+sve2）..."

for src in "$SRCDIR"/*.c; do
    name="$(basename "$src" .c)"
    "$CC" $CFLAGS "$src" -o "$BINDIR/$name"
    "$CC" $CFLAGS -c "$src" -o "$DISASMDIR/$name.o"
    "$OBJDUMP" -d --no-show-raw-insn "$DISASMDIR/$name.o" > "$DISASMDIR/$name.txt"
    rm -f "$DISASMDIR/$name.o"
    echo "  ✅ ${name}: 编译 + 反汇编完成"
done

echo ""
echo "关键 SVE2 指令对照："
echo "  01_vector_add 应该出现 whilelo / ld1w / add z0.s, p0/m / st1w"
echo "  02_uaddlb     应该出现 uaddlb z0.d, z0.s, z1.s"
grep -hE "whilelo|uaddlb|add\s+z0|ld1w|st1w" "$DISASMDIR"/*.txt 2>/dev/null | head -20

if [[ "$DO_RUN" -eq 1 ]]; then
    if command -v "$QEMU" >/dev/null 2>&1; then
        echo ""
        echo "===== 运行结果（qemu-aarch64 -cpu max）====="
        for p in "$BINDIR"/*; do
            echo "===== $(basename "$p") ====="
            "$QEMU" -cpu max "$p"
        done
    else
        echo "提示: 未找到 $QEMU，跳过运行（apt install qemu-user）"
    fi
fi

echo "全部完成 ✔"
