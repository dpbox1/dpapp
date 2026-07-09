#!/usr/bin/env bash
# ucoro 安装由 dpapp 从 stage 统一处理，移除子项目全部 install()。
set -euo pipefail
f="${1:?CMakeLists.txt path}"
sed -i 's#share/ucoro#usr/lib/cmake/ucoro#' "$f"
perl -i -0777 -pe 's/install\(\s*\n\s*TARGETS\b.*?\n\)\n//ms' "$f"
perl -i -0777 -pe 's/install\(\s*\n\s*DIRECTORY\b.*?\n\)\n//ms' "$f"
perl -i -0777 -pe 's/install\(\s*\n\s*EXPORT\b.*?\n\)\n//ms' "$f"
