#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."

out=docs/exception-visualizer.html
src=docs/visualizer-src

cat "$src/01-layout.html" \
    "$src/02-steps-boot-svc.js" \
    "$src/03-steps-exceptions.js" \
    "$src/04-engine.js" \
    "$src/05-tail.html" > "$out"

tmp=$(mktemp --suffix=.js)
sed -n '/<script>/,/<\/script>/p' "$out" | sed '1d;$d' > "$tmp"
node --check "$tmp" && rm -f "$tmp"

node scripts/test_visualizer.js
echo "OK: $out 已重新生成并通过自检"
