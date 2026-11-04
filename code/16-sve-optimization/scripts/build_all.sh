#!/usr/bin/env bash
# build_all.sh —— 交叉编译 SVE SAXPY + 多级优化反汇编 + 性能对比运行
#
# 依赖：aarch64-linux-gnu-gcc、aarch64-linux-gnu-objdump
# 可选：qemu-aarch64（apt install qemu-user），用 -cpu max 运行 SVE
#
# 用法：
#   ./scripts/build_all.sh              # 编译 + 反汇编（含 -O1/-O2/-O3/-Ofast 对照）
#   ./scripts/build_all.sh --run        # 额外用 qemu 运行（正确性 + 性能对比）
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

CFLAGS="-O3 -march=armv8-a+sve"

echo "== 1/3 编译 SAXPY 主程序（-O3，含基准） =="
"$CC" $CFLAGS "$SRCDIR/saxpy_example.c" "$SRCDIR/saxpy_asm.S" -o "$BINDIR/saxpy"
"$CC" $CFLAGS -c "$SRCDIR/saxpy_example.c" -o "$DISASMDIR/saxpy_example.o"
"$OBJDUMP" -d --no-show-raw-insn "$DISASMDIR/saxpy_example.o" > "$DISASMDIR/saxpy_example.txt"
rm -f "$DISASMDIR/saxpy_example.o"
"$CC" $CFLAGS -c "$SRCDIR/saxpy_asm.S" -o "$DISASMDIR/saxpy_asm.o"
"$OBJDUMP" -d --no-show-raw-insn "$DISASMDIR/saxpy_asm.o" > "$DISASMDIR/saxpy_asm.txt"
rm -f "$DISASMDIR/saxpy_asm.o"

echo "== 2/3 各优化级别反汇编对照（saxpy_c_only.c） =="
for o in O1 O2 O3 Ofast; do
    "$CC" "-$o" -march=armv8-a+sve -c "$SRCDIR/saxpy_c_only.c" -o "$DISASMDIR/saxpy_c.o"
    "$OBJDUMP" -d --no-show-raw-insn "$DISASMDIR/saxpy_c.o" > "$DISASMDIR/saxpy_c_$o.txt"
    rm -f "$DISASMDIR/saxpy_c.o"
    printf '  %-6s 核心指令: ' "$o"
    grep -oE "fmul|fadd|fmla|fmad|whilelo|ld1w|st1w|ptrue" "$DISASMDIR/saxpy_c_$o.txt" 2>/dev/null | sort -u | tr '\n' ' '
    echo
done

echo ""
echo "== 3/3 反汇编对照（关键指令） =="
echo "  saxpy_c_O1（基准，标量）:"
grep -hE "fmul|fadd" "$DISASMDIR/saxpy_example.txt" 2>/dev/null | head -2
echo "  saxpy_sve / saxpy_asm（SVE 向量）:"
grep -hE "whilelt|whilelo|fmla|ld1w|st1w" "$DISASMDIR/saxpy_example.txt" "$DISASMDIR/saxpy_asm.txt" 2>/dev/null | head -6

if [[ "$DO_RUN" -eq 1 ]]; then
    if command -v "$QEMU" >/dev/null 2>&1; then
        echo ""
        echo "===== 运行结果（qemu-aarch64 -cpu max）====="
        "$QEMU" -cpu max "$BINDIR/saxpy"
    else
        echo "提示: 未找到 $QEMU，跳过运行（apt install qemu-user）"
    fi
fi

echo "全部完成 ✔"
