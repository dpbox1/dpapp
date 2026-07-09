#!/usr/bin/env bash
# 运行 test_cwc 模块：init 内完成断言，进程常驻至 timeout；检查日志判定通过/失败。
set -euo pipefail

dpapp_bin="$1"
build_dir="$2"
module_so="$3"

output="$(timeout 5 "$dpapp_bin" -n1 2 -l notice -d "$build_dir" "$module_so" 2>&1 || true)"
printf '%s\n' "$output"

if printf '%s\n' "$output" | grep -q 'CTC test FAILED'; then
    exit 1
fi
printf '%s\n' "$output" | grep -q 'CTC test ALL PASSED'
