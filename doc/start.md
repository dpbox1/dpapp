## 开始

安装完成后进入 `/opt/dpbox`（默认前缀），在该目录下执行命令。
```shell
bin/dpapp -h
Usage: dpapp [system...] [external...] [module] [module...]
System options:
-h [ --help ]                            Produce help message
-V [ --version ]                         Print version message
-m [ --machine ] arg (0)                 Machine id, max 0 ~ 31
-n<t> arg (1)                            Thread number of type, n1 ~ n63.
                                         Disable thread type where <= 0
-u [ --cpu_off ] arg (1)                 Bind cpu id offset
-d [ --root_dir ] arg                    Root directory where dpapp running
-o [ --log_file ] arg ('/dev/stdout')    Log output file
-l [ --log_level ] arg (notice)          Log level: debug, info, notice, warning, error, alert
-t [ --log_tsacc ] arg (0)               Log time accuracy:
                                         0: second, 1:milliseconds, 2:microseconds
CWC external options:
-s [ --stack_size ] arg (64K)            Coroutine stack size(KB)

Lua external options:
-e [ --emode ]                           Run in embedded mode
```

- `-n<t>` 指定每种类型线程的线程数量，即一种类型可对应多个线程
- `-u --cpu_off` dpapp 支持 CPU 绑定，该参数用于指定 CPU 绑定偏移
- `-d --root_dir` 未指定时，默认为 dpapp 可执行文件所在目录的上一级（如 `bin/dpapp` → `/opt/dpbox`）
- `-s --stack_size` 指定 **C 模块（dpcwc + dpaco 有栈协程）** 默认栈大小，单位为 **KB**（内部会乘以 1024 再交给 `dpaco_thinit`）。缺省等价于 64KB，适合一般场景。

> **HTTP/3 / lsquic 注意：**
> 在发送响应头时会在栈上使用较大临时缓冲区。请务必手动调大 `--stack_size`，建议不小于 `256`（256 KB）。

- `args` 的第一个参数是要加载的模块，后续参数会传递给模块入口。可以指定完整路径；若找不到，dpapp 会在 `root_dir/app` 和 `root_dir/lib` 下查找。
- 模块后缀分流: `.so` → CWC/C++ 符号加载; `.lua` → dplua。

## 示例

```bash
cd /opt/dpbox
```

示例模块与证书位于 `app/example/`。

C++（`app/cpp/`）与 Lua（`app/lua/`）示例与 CWC 用法基本一致：模块名分别改为 `cpp_*` / `*.lua`，端口与协议参数相同，下文不再重复列出。

### CWC echo（`app/cwc/`）

`cwc_echo_svr` 一次启动 TCP / SSL / QUIC 三种 echo 服务（传 cert/key 时启用后两者）。

```bash
# 终端1：服务端（4490/tcp、4491/ssl、4492/quic）
bin/dpapp app/example/cwc_echo_svr.so app/example/crt.pem app/example/key.pem

# 终端2：TCP 客户端
bin/dpapp app/example/cwc_echo_cet.so

# 终端3：SSL 客户端
bin/dpapp app/example/cwc_echo_cet.so ssl

# 终端4：QUIC 客户端（需 lsquic 构建）
bin/dpapp app/example/cwc_echo_cet.so qic
```

### CWC HTTP HelloWorld（`app/cwc/`）

统一提供 HTTP/1.1、HTTPS、HTTP/3（端口 4480 / 4443 / 4443）。

```bash
bin/dpapp --stack_size 256 app/example/cwc_http_svr.so app/example/crt.pem app/example/key.pem
```

验证：

```bash
curl -sS http://127.0.0.1:4480/
curl -ksS https://127.0.0.1:4443/
curl --noproxy '*' -ksS --http3-only --resolve h3.dpapp:4443:127.0.0.1 https://h3.dpapp:4443/
```

### lsquic 工具测试

若编译时启用了 lsquic，可使用 lsquic 自带客户端交叉验证（路径以本地 lsquic 构建目录为准）：

```bash
# HTTP/3
http_client -s 127.0.0.1:4443 -H h3.dpapp -p / -o version=h3

# QUIC echo（ALPN: echo，端口 4492）
echo "hello" | echo_client -s 127.0.0.1:4492
```
