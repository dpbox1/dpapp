#!/usr/bin/env bash
# echo 集成测试（统一入口）
#
# 覆盖：协议 tcp / ssl / qic × 绑定 cwc / cpp / lua × 载荷 short / large
# 证书（ssl/qic）：app/crt.pem、app/key.pem（相对项目根目录）
#
# large 载荷：TTY stdin 规范模式单行上限 4096（含换行），测试输入不得超过此值。
#
# 用法：
#   test_echo.sh case <binding> <proto> <short|large> <dpapp_bin> <build_root>
#   test_echo.sh all <dpapp_bin> <build_root>          # 运行当前构建可用的全部用例
#   test_echo.sh help
#
# 环境：与其它 tst 脚本一致，设置 LD_LIBRARY_PATH，服务端后台运行，trap 清理。
set -euo pipefail

readonly SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
readonly PROJECT_ROOT="$(cd "${SCRIPT_DIR}/.." && pwd)"
# shellcheck source=tst/test_common.sh
source "${SCRIPT_DIR}/test_common.sh"
readonly CERT="${PROJECT_ROOT}/app/crt.pem"
readonly KEY="${PROJECT_ROOT}/app/key.pem"

# TTY stdin 规范模式单行上限 4096（含换行）；large 载荷另受 dpstm_recv_until
# 默认 batch_size=1024 约束，单行可读数据须 < 1024。
readonly MAX_LINE_SIZE=4096
readonly LARGE_ROUNDS=1
# large 每行: payload + '-' + tag + '\n'；末尾发 \q 令客户端退出（避免 stdin 等待打满 timeout）
readonly LARGE_TAG_RESERVE=24
readonly LARGE_SIZE=1010

usage() {
    sed -n '2,14p' "$0" | sed 's/^# \{0,1\}//'
}

# echo 固定端口；并行/残留进程会导致 SO_REUSEPORT 多监听，连接随机落到僵死实例
free_echo_ports() {
    fuser -k 4490/tcp 4491/tcp 4492/tcp 4492/udp 2>/dev/null || true
    sleep 0.2
}

wait_listen() {
    local port="$1"
    local proto="${2:-tcp}"
    local i
    for ((i = 0; i < 20; i++)); do
        if [[ "${proto}" == "qic" ]]; then
            if ss -uln "sport = :${port}" 2>/dev/null | grep -q UNCONN; then
                return 0
            fi
        elif ss -tln "sport = :${port}" 2>/dev/null | grep -q LISTEN; then
            return 0
        fi
        sleep 0.05
    done
    return 1
}

resolve_binding() {
    local binding="$1"
    local build_root="$2"

    case "${binding}" in
    cwc)
        server_mod="cwc_echo_svr.so"
        client_mod="cwc_echo_cet.so"
        server_extra=()
        use_n1=0
        ld_path="${build_root}/lib/dpapp:${build_root}/lib/dpcwc:${build_root}/lib/dpaco"
        short_payload="hello dpapp"
        expect_timing=1
        short_timeout=3
        large_timeout=3
        ;;
    cpp)
        server_mod="cpp_echo_svr.so"
        client_mod="cpp_echo_cet.so"
        server_extra=()
        use_n1=0
        ld_path="${build_root}/lib/dpapp:${build_root}/lib/dpcwc:${build_root}/lib/dpcpp:${build_root}/lib/dpaco"
        short_payload="hello dpapp"
        expect_timing=1
        short_timeout=3
        large_timeout=3
        ;;
    lua)
        server_mod="${PROJECT_ROOT}/app/lua/echo_svr.lua"
        client_mod="${PROJECT_ROOT}/app/lua/echo_cet.lua"
        server_extra=()
        use_n1=1
        ld_path="${build_root}/lib/dpapp:${build_root}/lib/dplua:${build_root}/lib/dpcwc:${build_root}/lib/dpcpp:${build_root}/lib/dpaco"
        short_payload="hello all"
        expect_timing=0
        short_timeout=3
        large_timeout=3
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

    free_echo_ports

    local -a server_cmd=("${dpapp_bin}" -d "${build_root}" -l warning -o "${server_log}")
    if [[ ${use_n1} -eq 1 ]]; then
        server_cmd+=(-n1 1)
    fi
    server_cmd+=("${server_mod}")
    if [[ "${proto}" == "ssl" || "${proto}" == "qic" ]]; then
        server_cmd+=("${CERT}" "${KEY}")
    fi
    server_cmd+=("${server_extra[@]}")

    LD_LIBRARY_PATH="${ld_path}:${LD_LIBRARY_PATH:-}" \
        "${server_cmd[@]}" >/dev/null 2>&1 &
    server_pid=$!

    local listen_port=4490
    if [[ "${proto}" == "ssl" ]]; then
        listen_port=4491
    elif [[ "${proto}" == "qic" ]]; then
        listen_port=4492
    fi
    if ! wait_listen "${listen_port}" "${proto}"; then
        echo "server not listening on ${listen_port}: ${binding}/${proto}" >&2
        sed -n '1,80p' "${server_log}" 2>/dev/null || true
        return 1
    fi
    if ! kill -0 "${server_pid}" 2>/dev/null; then
        echo "server exited early: ${binding}/${proto}" >&2
        sed -n '1,80p' "${server_log}" 2>/dev/null || true
        return 1
    fi
}

client_proto_args() {
    local proto="$1"
    if [[ "${binding}" == "cwc" || "${binding}" == "cpp" ]]; then
        if [[ "${proto}" != "tcp" ]]; then
            printf '%s\n' "${proto}"
        fi
    else
        printf '%s\n' "${proto}"
    fi
}

# pipe 直连 stdin 时 GFD aio_read_until 不可靠；script 分配 PTY，与交互式 echo 一致
run_client_with_stdin() {
    local log="$1"
    local timeout_sec="$2"
    shift 2
    local -a cmd=("$@")
    local cmd_q
    cmd_q=$(printf '%q ' "${cmd[@]}")
    script -q -c "stdbuf -o0 timeout ${timeout_sec}s ${cmd_q}" /dev/null >"${log}" 2>&1
}

run_short() {
    local dpapp_bin="$1"
    local build_root="$2"
    local proto="$3"
    local log_dir="${build_root}/test-logs"
    mkdir -p "${log_dir}"

    local tag="${binding}-${proto}-short-${$}"
    _echo_server_log="${log_dir}/echo-${tag}-svr.log"
    _echo_client_log="${log_dir}/echo-${tag}-cet.log"
    _echo_client_app_log="${log_dir}/echo-${tag}-cet-app.log"

    cleanup() {
        if [[ -n "${server_pid:-}" ]]; then
            kill "${server_pid}" 2>/dev/null || true
            wait "${server_pid}" 2>/dev/null || true
        fi
        rm -f "${_echo_server_log}" "${_echo_client_log}" "${_echo_client_app_log}"
    }
    trap cleanup EXIT

    start_server "${dpapp_bin}" "${build_root}" "${proto}" "${_echo_server_log}"

    mapfile -t client_args < <(client_proto_args "${proto}")

    local -a client_cmd=("${dpapp_bin}" -d "${build_root}" -l warning)
    client_cmd+=("${client_mod}" "${client_args[@]}")

    export LD_LIBRARY_PATH="${ld_path}:${LD_LIBRARY_PATH:-}"
    set +e
    printf '%s\n' "${short_payload}" | run_client_with_stdin "${_echo_client_log}" \
        "${short_timeout}" "${client_cmd[@]}"
    local client_rc=$?
    set -e

    if [[ ${client_rc} -ne 0 && ${client_rc} -ne 124 ]]; then
        echo "client failed rc=${client_rc}: ${binding}/${proto}/short" >&2
        cat "${_echo_client_log}" 2>/dev/null || true
        return 1
    fi

    local client_output
    client_output="$(<"${_echo_client_log}")"
    if [[ "${client_output}" != *"${short_payload}"* ]]; then
        echo "short echo mismatch: ${binding}/${proto}" >&2
        cat "${_echo_client_log}" 2>/dev/null || true
        return 1
    fi
    if [[ ${expect_timing} -eq 1 && "${client_output}" != *"(time taken:"* ]]; then
        echo "missing timing: ${binding}/${proto}/short" >&2
        cat "${_echo_client_log}" 2>/dev/null || true
        return 1
    fi
}

_echo_fail_keep_logs() {
    _echo_keep_logs=1
    echo "  server log: ${_echo_server_log}" >&2
    echo "  client log: ${_echo_client_log}" >&2
    cat "${_echo_client_log}" 2>/dev/null || true
    echo "--- server ---" >&2
    cat "${_echo_server_log}" 2>/dev/null || true
}

run_large() {
    local dpapp_bin="$1"
    local build_root="$2"
    local proto="$3"
    local log_dir="${build_root}/test-logs"
    mkdir -p "${log_dir}"

    local tag="${binding}-${proto}-large-${$}"
    _echo_keep_logs=0
    _echo_server_log="${log_dir}/echo-${tag}-svr.log"
    _echo_client_log="${log_dir}/echo-${tag}-cet.log"

    cleanup() {
        if [[ -n "${server_pid:-}" ]]; then
            kill "${server_pid}" 2>/dev/null || true
            wait "${server_pid}" 2>/dev/null || true
        fi
        if [[ ${_echo_keep_logs:-0} -eq 0 ]]; then
            rm -f "${_echo_server_log}" "${_echo_client_log}"
        fi
    }
    trap cleanup EXIT

    start_server "${dpapp_bin}" "${build_root}" "${proto}" "${_echo_server_log}"

    mapfile -t client_args < <(client_proto_args "${proto}")
    local payload
    payload="$(python3 -c "print('P' * ${LARGE_SIZE})")"

    local i tag line_len
    for ((i = 1; i <= LARGE_ROUNDS; i++)); do
        tag="${binding}-${proto}-r${i}"
        line_len=$((${#payload} + 1 + ${#tag} + 1))
        if (( line_len > MAX_LINE_SIZE )); then
            echo "line too long (${line_len} > ${MAX_LINE_SIZE}): ${binding}/${proto}" >&2
            return 1
        fi
    done

    export LD_LIBRARY_PATH="${ld_path}:${LD_LIBRARY_PATH:-}"
    local -a large_client_cmd=("${dpapp_bin}" -d "${build_root}" -l warning)
    large_client_cmd+=("${client_mod}" "${client_args[@]}")
    set +e
    {
        local i tag
        for ((i = 1; i <= LARGE_ROUNDS; i++)); do
            tag="${binding}-${proto}-r${i}"
            printf '%s-%s\n' "${payload}" "${tag}"
        done
        printf '\\q\n'
    } | run_client_with_stdin "${_echo_client_log}" "${large_timeout}" \
        "${large_client_cmd[@]}"
    local client_rc=$?
    set -e

    if [[ ${client_rc} -ne 0 && ${client_rc} -ne 124 ]]; then
        echo "large client failed rc=${client_rc}: ${binding}/${proto}" >&2
        _echo_fail_keep_logs
        return 1
    fi

    local client_output
    client_output="$(<"${_echo_client_log}")"
    local i tag needle
    for ((i = 1; i <= LARGE_ROUNDS; i++)); do
        tag="${binding}-${proto}-r${i}"
        needle="${payload}-${tag}"
        if [[ "${client_output}" != *"${needle}"* ]]; then
            echo "missing large round ${i}: ${binding}/${proto}" >&2
            _echo_fail_keep_logs
            return 1
        fi
    done
}

run_case() {
    local binding="$1"
    local proto="$2"
    local size="$3"
    local dpapp_bin="$4"
    local build_root="$5"

    if [[ "${proto}" == "qic" ]] && ! have_qic "${build_root}"; then
        echo "qic disabled (no lsquic in build): ${binding}/${size}" >&2
        exit 1
    fi

    if [[ ! -f "${CERT}" || ! -f "${KEY}" ]]; then
        if [[ "${proto}" == "ssl" || "${proto}" == "qic" ]]; then
            echo "missing cert/key under app/" >&2
            exit 1
        fi
    fi

    resolve_binding "${binding}" "${build_root}" || exit 1

    case "${size}" in
    short) run_short "${dpapp_bin}" "${build_root}" "${proto}" || return 1 ;;
    large) run_large "${dpapp_bin}" "${build_root}" "${proto}" || return 1 ;;
    *)
        echo "unknown size: ${size} (use short or large)" >&2
        exit 1
        ;;
    esac

    echo "OK ${binding}/${proto}/${size}"
}

run_all() {
    local dpapp_bin="$1"
    local build_root="$2"
    local protos=(tcp ssl)
    if have_qic "${build_root}"; then
        protos+=(qic)
    fi

    local -a bindings=()
    [[ -f "${build_root}/app/cwc_echo_svr.so" ]] && bindings+=(cwc)
    [[ -f "${build_root}/app/cpp_echo_svr.so" ]] && bindings+=(cpp)
    [[ -f "${PROJECT_ROOT}/app/lua/echo_svr.lua" ]] && bindings+=(lua)

    local binding proto size
    for binding in "${bindings[@]}"; do
        for proto in "${protos[@]}"; do
            for size in short large; do
                echo "== ${binding} ${proto} ${size} =="
                if ! (
                    run_case "${binding}" "${proto}" "${size}" \
                        "${dpapp_bin}" "${build_root}"
                ); then
                    exit 1
                fi
            done
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
    [[ $# -ge 5 ]] || {
      echo "usage: $0 case <binding> <proto> <short|large> <dpapp_bin> <build_root>" >&2
      exit 1
    }
    run_case "$1" "$2" "$3" "$4" "$5"
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
