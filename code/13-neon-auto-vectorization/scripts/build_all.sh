#!/usr/bin/env bash
# build_all.sh —— 交叉编译自动向量化示例，对比 -O1（标量）与 -O3（向量）反汇编
#
# 依赖：aarch64-linux-gnu-gcc、aarch64-linux-gnu-objdump
#       （apt 安装 gcc-aarch64-linux-gnu binutils-aarch64-linux-gnu）
# 可选：qemu-aarch64（apt install qemu-user），用于运行示例
#
# 用法：
#   ./scripts/build_all.sh              # 编译（-O1/-O3）+ 反汇编
#   ./scripts/build_all.sh --run        # 额外用 qemu 运行示例
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

CFLAGS_O1="-O1 -march=armv8-a+simd"
CFLAGS_O3="-O3 -march=armv8-a+simd"

echo "交叉编译并反汇编各示例（-O1 无向量化 vs -O3 向量化）..."

for src in "$SRCDIR"/*.c; do
    name="$(basename "$src" .c)"

    "$CC" $CFLAGS_O1 "$src" -o "$BINDIR/${name}_O1"
    "$CC" $CFLAGS_O3 "$src" -o "$BINDIR/${name}_O3"

    "$CC" $CFLAGS_O1 -c "$src" -o "$DISASMDIR/${name}.o"
    "$OBJDUMP" -d --no-show-raw-insn "$DISASMDIR/${name}.o" > "$DISASMDIR/${name}_O1.txt"
    rm -f "$DISASMDIR/${name}.o"

    "$CC" $CFLAGS_O3 -c "$src" -o "$DISASMDIR/${name}.o"
    "$OBJDUMP" -d --no-show-raw-insn "$DISASMDIR/${name}.o" > "$DISASMDIR/${name}_O3.txt"
    rm -f "$DISASMDIR/${name}.o"

    echo "  ✅ ${name}: 反汇编已生成 _O1（标量）与 _O3（向量）"
done

echo ""
echo "关键指令对照（vec_add）："
echo "  -O1 应该出现  FADD s0, s0, s1          （标量，一次加一个）"
echo "  -O3 应该出现  FADD v0.4s, v0.4s, v1.4s  （向量，一次加四个）"
grep -hE "fadd" "$DISASMDIR"/vec_add_O1.txt "$DISASMDIR"/vec_add_O3.txt 2>/dev/null || true
echo ""
echo "关键指令对照（cubed）："
echo "  -O1 应该出现  FMUL d0, d0, d0          （标量，cubed 未内联）"
echo "  -O3 应该出现  FMUL v0.2d, ...          （向量，cubed_i 已内联）"

if [[ "$DO_RUN" -eq 1 ]]; then
    if command -v "$QEMU" >/dev/null 2>&1; then
        echo ""
        echo "===== 运行结果（qemu-aarch64，-O3 版）====="
        for p in "$BINDIR"/*_O3; do
            echo "===== $(basename "$p") ====="
            "$QEMU" "$p"
        done
    else
        echo "提示: 未找到 $QEMU，跳过运行（apt install qemu-user）"
    fi
fi

echo "全部完成 ✔"
