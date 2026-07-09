## 测试环境

| 项 | 值 |
|---|---|
| 主机 | Linux x86_64，AMD Ryzen 7 7840HS，16 核，15 GiB RAM，epoll |
| 构建 | `CMAKE_BUILD_TYPE=Release`，`/opt/dpbox` |
| OpenSSL / lsquic | 启用 |

### CTC 跨线程调用

`test_ctc_perf.so` 随测试构建，位于源码 `build/tst/`，未安装：

```bash
cd /opt/dpbox
export LD_LIBRARY_PATH=usr/lib
bin/dpapp -n2 2 /path/to/dpapp/build/tst/test_ctc_perf.so 10000 10000
```

2666666 task/s

### C 协程切换（`perf.coroutine`）

| 场景 | 切换耗时 | 吞吐 |
|---|---:|---:|
| 单协程 100 万次 | ~0.06 μs/次 | ~16.2M ops/s |
| 10 协程各 10 万次 | ~0.06 μs/次 | ~16.1M ops/s |
| 嵌套 50 万次 | ~0.06 μs/次 | ~16.5M ops/s |

### TCP echo（tcpkali，三绑定 echo server）

大多数同类型库都提供 echo server 的 example，因此仅对单线程 echo server 的性能进行对比测试。服务端回显固定载荷 `'hello world'`（11 字节），监听 `127.0.0.1:4490`。

测试客户端统一使用 [tcpkali](https://github.com/akumuli/tcpkali.git)：

```bash
tcpkali -c 100 -T 30s -w 8 -m 'hello world' 127.0.0.1:4490
```

服务端（每次测试前释放 4490 端口，确认 `ss -tln sport = :4490` 处于 LISTEN）：

```bash
cd /opt/dpbox
export LD_LIBRARY_PATH=usr/lib

bin/dpapp app/example/cwc_echo_svr.so
bin/dpapp app/example/cpp_echo_svr.so
bin/dpapp app/example/echo_svr.lua
```

记录 tcpkali 输出中的 `Aggregate bandwidth` 下行（↓）与上行（↑）值（单位 Mbps）。

| 绑定 | 模块 | 下行 (Mbps) | 上行 (Mbps) | 说明 |
|---|---|---:|---:|---|
| cwc | `cwc_echo_svr.so` | 15378.002 | 15378.803 | C 协程（dpcwc） |
| cpp | `cpp_echo_svr.so` | 15833.886 | 15834.997 | C++20 协程（dpcpp） |
| lua | `echo_svr.lua` | 13981.118 | 13982.785 | lua协程 |

```text
下行带宽 (Mbps)            █ = 300 Mbps
dpapp cpp (epoll)    █████████████████████████████████████████████████████ 15833.886↓
dpapp cwc (epoll)    ███████████████████████████████████████████████████ 15378.002↓
dpapp lua (epoll)    ███████████████████████████████████████████████ 13981.118↓
libuv                █████████████████████████████████████████ 12410
async_simple async   ██████████████ 4333
async_simple block   ██████████████████████████ 7782
coroio               ████ 1309
asio c++20           ███████ 2187

上行带宽 (Mbps)            █ = 300 Mbps
dpapp cpp (epoll)    █████████████████████████████████████████████████████ 15834.997↑
dpapp cwc (epoll)    ███████████████████████████████████████████████████ 15378.803↑
dpapp lua (epoll)    ███████████████████████████████████████████████ 13982.785↑
libuv                ██████████████████████████████████████████ 12451
async_simple async   ██████████████ 4333
async_simple block   ██████████████████████████ 7784
coroio               ████ 1310
asio c++20           ███████ 2188
```

第三方库（libuv、async_simple、coroio、asio）为同条件下早期参考数据，单位 Mbps。
