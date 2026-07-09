#!/usr/bin/env bash
# HTTP 集成测试（统一入口）
#
# 覆盖：协议 http / https / http3 × 绑定 cwc / lua
# 证书（https/http3）：app/crt.pem、app/key.pem（相对项目根目录）
#
# 用法：
#   test_http.sh case <binding> <proto> <dpapp_bin> <build_root>
#   test_http.sh all <dpapp_bin> <build_root>
#   test_http.sh help
#
# http3 需 curl 支持 HTTP/3；不可用时退出码 77（CTest SKIP_RETURN_CODE）。
set -euo pipefail

readonly SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
readonly PROJECT_ROOT="$(cd "${SCRIPT_DIR}/.." && pwd)"
# shellcheck source=tst/test_common.sh
source "${SCRIPT_DIR}/test_common.sh"
readonly CERT="${PROJECT_ROOT}/app/crt.pem"
readonly KEY="${PROJECT_ROOT}/app/key.pem"

readonly HTTP_PORT=4480
readonly TLS_PORT=4443
readonly EXPECT_BODY="Hello World"

usage() {
    sed -n '2,14p' "$0" | sed 's/^# \{0,1\}//'
}

resolve_binding() {
    local binding="$1"
    local build_root="$2"

    case "${binding}" in
    cwc)
        server_mod="cwc_http_svr.so"
        use_n1=0
        use_stack256=1
        ld_path="${build_root}/lib/dpapp:${build_root}/lib/dpcwc:${build_root}/lib/dpaco"
        ;;
    lua)
        server_mod="${PROJECT_ROOT}/app/lua/http_svr.lua"
        use_n1=1
        use_stack256=0
        ld_path="${build_root}/lib/dpapp:${build_root}/lib/dplua:${build_root}/lib/dpcwc:${build_root}/lib/dpcpp:${build_root}/lib/dpaco"
        ;;
    *)
        echo "unknown binding: ${binding}" >&2
        return 1
        ;;
    esac
}

start_server() {
    local dpapp_bin="$1"
    local build_root="$2"
    local proto="$3"
    local server_log="$4"

    local -a server_cmd=("${dpapp_bin}" -d "${build_root}" -l warning -o "${server_log}")
    if [[ ${use_stack256} -eq 1 ]]; then
        server_cmd+=(--stack_size 256)
    fi
    if [[ ${use_n1} -eq 1 ]]; then
        server_cmd+=(-n1 1)
    fi
    server_cmd+=("${server_mod}")
    if [[ "${proto}" == "https" || "${proto}" == "http3" ]]; then
        server_cmd+=("${CERT}" "${KEY}")
    fi

    LD_LIBRARY_PATH="${ld_path}:${LD_LIBRARY_PATH:-}" \
        "${server_cmd[@]}" >/dev/null 2>&1 &
    server_pid=$!

    local i
    for ((i = 1; i <= 15; i++)); do
        if ! kill -0 "${server_pid}" 2>/dev/null; then
            echo "server exited early: ${binding}/${proto}" >&2
            sed -n '1,80p' "${server_log}" 2>/dev/null || true
            return 1
        fi
        sleep 0.2
    done
}

run_curl() {
    local proto="$1"
    local curl_log="$2"

    case "${proto}" in
    http)
        curl -sS --max-time 5 "http://127.0.0.1:${HTTP_PORT}/" >"${curl_log}" 2>&1
        ;;
    https)
        curl -sS --max-time 5 -k "https://127.0.0.1:${TLS_PORT}/" >"${curl_log}" 2>&1
        ;;
    http3)
        if ! have_curl_http3; then
            echo "SKIP http3 (curl without HTTP/3 support)"
            exit 77
        fi
        curl --noproxy '*' -ksS --max-time 5 --http3-only \
            "https://127.0.0.1:${TLS_PORT}/" >"${curl_log}" 2>&1
        ;;
    *)
        echo "unknown proto: ${proto}" >&2
        return 1
        ;;
    esac
}

run_case() {
    local binding="$1"
    local proto="$2"
    local dpapp_bin="$3"
    local build_root="$4"

    if [[ "${proto}" == "http3" ]] && ! have_qic "${build_root}"; then
        echo "SKIP http3 (no lsquic in build)"
        exit 77
    fi

    if [[ "${proto}" == "http3" ]] && ! have_curl_http3; then
        echo "SKIP http3 (curl without HTTP/3 support)"
        exit 77
    fi

    if [[ "${proto}" == "https" || "${proto}" == "http3" ]]; then
        if [[ ! -f "${CERT}" || ! -f "${KEY}" ]]; then
            echo "missing cert/key under app/" >&2
            exit 1
        fi
    fi

    resolve_binding "${binding}" "${build_root}" || exit 1

    local log_dir="${build_root}/test-logs"
    mkdir -p "${log_dir}"

    local tag="${binding}-${proto}-${$}"
    _http_server_log="${log_dir}/http-${tag}-svr.log"
    _http_curl_log="${log_dir}/http-${tag}-curl.log"

    cleanup() {
        if [[ -n "${server_pid:-}" ]]; then
            kill "${server_pid}" 2>/dev/null || true
            wait "${server_pid}" 2>/dev/null || true
        fi
        rm -f "${_http_server_log}" "${_http_curl_log}"
    }
    trap cleanup EXIT

    start_server "${dpapp_bin}" "${build_root}" "${proto}" "${_http_server_log}"

    set +e
    run_curl "${proto}" "${_http_curl_log}"
    local curl_rc=$?
    set -e

    if [[ ${curl_rc} -eq 77 ]]; then
        exit 77
    fi

    local response=""
    if [[ -s "${_http_curl_log}" ]]; then
        response="$(<"${_http_curl_log}")"
    fi

    if [[ ${curl_rc} -ne 0 ]]; then
        if [[ "${proto}" == "http3" && "${response}" == *"${EXPECT_BODY}"* ]]; then
            curl_rc=0
        else
            echo "curl failed rc=${curl_rc}: ${binding}/${proto}" >&2
            sed -n '1,80p' "${_http_curl_log}" 2>/dev/null || true
            exit 1
        fi
    fi

    if [[ "${response}" != *"${EXPECT_BODY}"* ]]; then
        echo "unexpected response: ${binding}/${proto}" >&2
        echo "${response}" >&2
        exit 1
    fi

    echo "OK ${binding}/${proto}"
}

run_all() {
    local dpapp_bin="$1"
    local build_root="$2"

    local -a protos=(http https)
    if have_qic "${build_root}"; then
        protos+=(http3)
    fi

    local -a bindings=()
    [[ -f "${build_root}/app/cwc_http_svr.so" ]] && bindings+=(cwc)
    [[ -f "${PROJECT_ROOT}/app/lua/http_svr.lua" ]] && bindings+=(lua)

    local binding proto
    for binding in "${bindings[@]}"; do
        for proto in "${protos[@]}"; do
            echo "== ${binding} ${proto} =="
            if ! (
                run_case "${binding}" "${proto}" "${dpapp_bin}" "${build_root}"
            ); then
                exit 1
            fi
        done
    done
}

main() {
    local cmd="${1:-help}"
    shift || true

    case "${cmd}" in
    help|-h|--help)
        usage
        ;;
    case)
        [[ $# -ge 4 ]] || {
            echo "usage: $0 case <binding> <proto> <dpapp_bin> <build_root>" >&2
            exit 1
        }
        run_case "$1" "$2" "$3" "$4"
        ;;
    all)
        [[ $# -ge 2 ]] || {
            echo "usage: $0 all <dpapp_bin> <build_root>" >&2
            exit 1
        }
        run_all "$1" "$2"
        ;;
    *)
        echo "unknown command: ${cmd}" >&2
        usage >&2
        exit 1
        ;;
    esac
}

main "$@"
