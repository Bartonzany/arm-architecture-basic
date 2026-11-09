#!/usr/bin/env bash
set -euo pipefail
CROSS="${CROSS:-aarch64-linux-gnu-}"
CC="${CROSS}gcc"; OBJDUMP="${CROSS}objdump"; QEMU="${QEMU:-qemu-aarch64}"
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
SRCDIR="$ROOT/src"; BUILDDIR="$ROOT/build"; BINDIR="$BUILDDIR/bin"; DISASMDIR="$BUILDDIR/disasm"
DO_RUN=0; [[ "${1:-}" == "--run" ]] && DO_RUN=1
[[ "${1:-}" == "clean" ]] && { rm -rf "$BUILDDIR"; echo "已清理"; exit 0; }
mkdir -p "$BINDIR" "$DISASMDIR"
CFLAGS="-O3 -march=armv8-a+sve"
echo "交叉编译矩阵乘法（C/Neon/SVE，-march=armv8-a+sve）..."
for src in "$SRCDIR"/*.c; do
    n="$(basename "$src" .c)"
    "$CC" $CFLAGS "$src" -o "$BINDIR/$n"
    "$CC" $CFLAGS -c "$src" -o "$DISASMDIR/$n.o"
    "$OBJDUMP" -d --no-show-raw-insn "$DISASMDIR/$n.o" > "$DISASMDIR/$n.txt"
    rm -f "$DISASMDIR/$n.o"
    echo "  ✅ $n"
done
echo ""
echo "关键指令对照："
echo "  Neon 应该出现 fmla v0.4s（128 位）"
echo "  SVE  应该出现 whilelt + ld1w + fmla z0.s"
grep -hE "fmla|whilelt|ld1w|ld1rqw" "$DISASMDIR"/matrix_multiply.txt 2>/dev/null | head -12
if [[ "$DO_RUN" -eq 1 ]]; then
    echo ""; echo "===== 运行结果（qemu-aarch64 -cpu max）====="
    for p in "$BINDIR"/*; do echo "===== $(basename "$p") ====="; "$QEMU" -cpu max "$p"; done
fi
echo "全部完成 ✔"
