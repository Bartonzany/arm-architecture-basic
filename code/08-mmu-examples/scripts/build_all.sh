#!/usr/bin/env bash
# build_all.sh — 一键交叉汇编 MMU 初始化示例 + 反汇编
#
# 说明：
#   原示例用 ARM Compiler 6（armclang/armlink）构建、目标平台为 ARM FVP
#   Base Platform AEM。本脚本改用 GNU 工具链（aarch64-linux-gnu-as/ld）
#   做等价汇编 + 链接 + 反汇编，产物输出到 build/<示例名>/。
#
# 用法：
#   ./scripts/build_all.sh            # 汇编 + 链接 + 反汇编全部 10 个示例
#   ./scripts/build_all.sh clean      # 清理
#
# 依赖：aarch64-linux-gnu-as / aarch64-linux-gnu-ld / aarch64-linux-gnu-objdump

set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
SRC="$ROOT/src"
BUILD="$ROOT/build"

AS="aarch64-linux-gnu-as"
LD="aarch64-linux-gnu-ld"
OBJDUMP="aarch64-linux-gnu-objdump"

# .equ 等语法用 ARM Compiler 6 的 armasm GNU 语法子集，GNU as 完全兼容；
# 直接汇编不需要 -march，仅在用到 ARMv8.1-A 特性（E2H、硬件 AF 更新）的两个
# 示例上显式声明 arch。
declare -A ARCH=(
  [el2_stage1_l1only_e2h]="-march=armv8.1-a"
  [el3_AF_update]="-march=armv8.1-a"
)

EXAMPLES=(
  el1_stage1
  el1_stage1_and_stage2
  el2_stage1_l1andl2
  el2_stage1_l1only
  el2_stage1_l1only_e2h
  el3_AF_update
  el3_stage1_full_mem_map
  el3_stage1_l0_to_l2
  el3_stage1_l1andl2
  el3_stage1_l1only
)

if [[ "${1:-}" == "clean" ]]; then
    rm -rf "$BUILD"
    echo "已清理 $BUILD"
    exit 0
fi

echo "交叉汇编 + 链接 + 反汇编 MMU 初始化示例..."
echo "工具链：$AS / $LD / $OBJDUMP"
echo

for name in "${EXAMPLES[@]}"; do
    out="$BUILD/$name"
    mkdir -p "$out"
    arch="${ARCH[$name]:-}"
    # shellcheck disable=SC2086
    "$AS" $arch -o "$out/startup.o" "$SRC/$name/startup.S"
    "$LD" -Ttext=0x80000000 --entry=start64 -o "$out/image.elf" "$out/startup.o"
    "$OBJDUMP" -d --no-show-raw-insn "$out/image.elf" > "$out/image.dis"
    printf '  %-26s -> %s/image.elf + image.dis\n' "$name" "$name"
done

echo
echo "产物目录：$BUILD"