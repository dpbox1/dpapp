#!/usr/bin/env bash
# 运行 fsm/sig/pip 测试模块（cwc/cpp .so 或 lua 脚本）
set -euo pipefail

dpapp_bin="$1"
build_dir="$2"
module="$3"

output="$(timeout 10 "$dpapp_bin" -n1 2 -l notice -d "$build_dir" "$module" 2>&1 || true)"
printf '%s\n' "$output"

if printf '%s\n' "$output" | grep -q 'FSM SIG PIP test FAILED'; then
    exit 1
fi
printf '%s\n' "$output" | grep -q 'FSM SIG PIP test ALL PASSED'
