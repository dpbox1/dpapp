#!/usr/bin/env bash
# tst 集成测试公共检测（由 test_echo.sh / test_http.sh source，勿直接执行）

# 构建是否启用 QUIC（lsquic / dpqic_enable）
have_qic() {
    local build_root="$1"
    local lib="${build_root}/lib/dpapp/libdpapp.so"
    if [[ -f "${lib}" ]] \
        && nm -D "${lib}" 2>/dev/null | grep -qE '[[:space:]]dpqic_enable$'; then
        return 0
    fi
    if compgen -G "${build_root}/lib/dpapp/libdpqic.so*" >/dev/null 2>&1 \
        || compgen -G "${build_root}/lib/dpapp/libdpqic.a" >/dev/null 2>&1; then
        return 0
    fi
    [[ -f "${lib}" ]] && ldd "${lib}" 2>/dev/null | grep -q 'lsquic'
}

# 系统 curl 是否支持 --http3-only（运行时探测，避免 help 假阳性）
have_curl_http3() {
    curl --http3-only --max-time 1 -sS -o /dev/null \
        http://127.0.0.1:1/ 2>/dev/null
    local rc=$?
    [[ ${rc} -eq 0 || ${rc} -eq 7 || ${rc} -eq 28 ]]
}
