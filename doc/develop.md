## 概览

### 适用场景

- 网络应用:服务器、客户端、网关
- 一管多:异步 DAG，不同任务映射到不同线程组
- 多管一:临界资源集中管理（类 Redis），主线程原子操作、子线程 IO
- 同构多语言:核心 C 能力 + CWC/C++/Lua 任选绑定层

### 组件

| 组件 | 目录 | 说明 |
|------|------|------|
| `libdpapp` | `lib/dpapp/` | epoll 事件循环、元素模型 `dpele`、异步 syscall `dpasc`、SSL/QUIC、CTC |
| `libdpaco` | `lib/dpaco/` | C 有栈协程；swap 仅保存 ABI callee-saved 寄存器 |
| `libdpcwc` | `lib/dpcwc/` | C 协程绑定；`dpcwc_await` + `dpcwc_asc.h` 内联包装 |
| `libdpcpp` | `lib/dpcpp/` | C++20 `co_await` + `dpcpp_asc.hh` 内联包装 |
| `libdplua` | `lib/dplua/` | LuaJIT 绑定；运行时 `dplua.lua`，模块 `dp*.lua` |
| `dpapp` | `bin/` | 统一入口，按后缀选择绑定层 |

### API 文档

构建启用 `DPAPP_WITH_DOCS`（默认 ON）后生成 Doxygen HTML：

```shell
cmake --build build --target doxygen_doc
```

- 入口：`/opt/dpbox/usr/share/dpapp/doxygen/html/index.html`
- 中间产物：`build/doc/doxygen/html/`（Doxygen 生成后同步到 stage）
- **Topics** 页：`dpapi` → `dpapp`/*（C 核心，3 级）或 `dpcwc_asc` / `dpcpp` 等（绑定层，2 级）；主题骨架见 `doc/dpdoxy_groups.h`
- 亦可直接阅读 `lib/*` 头文件中的 `@brief` 注释

### 示例应用

```
app/
├── cwc/     # C 协程 .so:cwc_echo_svr、cwc_http_svr …
├── cpp/     # C++ .so:cpp_echo_svr、cpp_echo_cet
├── lua/     # 脚本:echo_svr.lua、http_svr.lua …
```

安装至 `/opt/dpbox`（默认）；示例 `.so`、`.lua` 与证书在 `app/example/`。

## 使用模块开发应用

推荐将业务编译为 `.so` 或由脚本实现，由 `dpapp` 动态加载。

- 共享库命名: `your_module.so`（建议不带 `lib` 前缀）。
- 后缀分流: `.so` → `dpcpp__*` / `dpcwc__*`; `.lua` → dplua。
- 模块搜索: `root_dir/app/`（默认 `/opt/dpbox/app/`）。

可运行示例见 [start.md](start.md)。C++/Lua 与 CWC 端口及协议参数一致，仅模块名不同。

### C++ 模块（dpcpp）

入口:

```c
extern "C" dpret_t dpcpp__your_module(int argc, char** argv, dpcpp::app_hdr* hdrs);
```

约定:

- 返回值是线程「类型数」`count`。
- 填充 `hdrs[0..count-1]` 的 `init/exit/step` 及初始化参数。
- 线程 `init` 协程: `dpcpp::aco_v64 fn(dpv64_t arg1, dpv64_t arg2);`

参考: `app/cpp/echo_svr.cc`、`app/cpp/echo_cet.cc`。

### C 模块（dpcwc）

入口:

```c
extern dpret_t dpcwc__your_module(int argc, char** argv, dpapp_hdr_t* hdrs);
```

约定:

- 返回值是线程类型数 `count`。
- 填充 `hdrs[0..count-1]` 的 `init/exit/step` 与 `init_arg1/init_arg2`。
- 线程 `init`: `dpv64_t fn(dpv64_t arg1, dpv64_t arg2);`，内部 `dpcwc_await` 等待异步完成。

参考: `app/cwc/echo_svr.c`、`app/cwc/http_svr.c`。

### Lua 模块（dplua）

文件末尾 **`return M`**，`M` 为表。必须提供:

```lua
local M = {}

function M.your_function()

end

-- 配置线程类型数，并为每个 type 指定启动函数与参数
function M.__main__(args, hdrs)
    hdrs[0] = { init = "__init00", args = { host = "127.0.0.1", port = 4490 } }
    return 1   -- 线程 type 个数，对应 hdrs[0] .. hdrs[type_count-1]
end

-- 各 worker 启动时在协程中执行（函数名与 hdrs[t].init 一致）
function M.__init00(arg)
    -- 监听、客户端主循环等
end

return M
```

- 其与一般lua模块并无差异，M表中仅__开头的为保留字段，目前只用了__main__
- `args`:命令行参数表（`args[0]` 为脚本路径，`args[1]` 起为用户参数）。
- `hdrs[t]`:`t` 为线程 type 索引（从 0 起）；`init` 为 `M` 上的函数名字符串，`args` 原样传给该函数。
- `arg`: 已经展开的hdrs[0].args
- 以 `__` 开头的键视为内部保留，其余函数名（比如 your_function）字符串，均可直接作为ctc的topic。

参考: `app/lua` 下的示例

## API 语义约定

`dpret_t` 与 `dpele_t` 是各绑定层最易混淆的两类约定，详见 `lib/dpapp/dpret.h`、`dpele.h`。

### dpret_t 返回值

`dpret_t` 为带符号 `int`:**负值为错误（`DPE_*`），非负为成功或其他非错误语义。**

| 区间 | 含义 | 判断 |
|------|------|------|
| `< 0` | 错误码 | `dpret_iserr(r)` |
| `≥ 0` | 成功或非错误 | `dpret_isok(r)` |

错误码分系统 errno 映射、`[-150,-189)` dpapp 专用（如 `DPE_EOF`、`DPE_WAIT`）、HTTP 状态映射三类。

非负返回值须结合 API 文档解读，常见情形:

- **纯成功**:`DPE_OK`（0）
- **I/O 字节数**:`dpskt_recv`、`dpgfd_read` 等（0 表示 EOF，仍属 `dpret_isok`）
- **特殊正数**:如 `dpapp_arg_parse` 返回 `1`/`2` 表示已打印 help/version，不可仅用 `dpret_isok` 判断业务成败
- **inotify wd**:`dpfsm_addev` 成功返回 `≥ 0` 的 watch descriptor

`DPE_WAIT` 与 `DPE_AGAIN` 数值相同（均为 `-EAGAIN`）:框架内部用 `DPE_WAIT` 挂起协程，业务层 `DPE_AGAIN` 表示可重试；绑定层 `aexec`/`await` 会消费 `DPE_WAIT`，应用通常无需手动区分。

### dpele_t 生命周期

`dpele_t` 统一表示异步工作项（EFD / USD / CTC / TMR），类型别名 `dpefd_t`、`dpusd_t`、`dpctc_t` 等与 `dpele_t` 相同，无编译期区分。

- **USD**:用户自驱动槽（QIC 等），业务或 `dpevp_end` 推进
- **CTC**:跨线程调用；`dpctc_init_type()` + `dpctc_submit(dpv64_t, dpv64_t)` 派发用户回调，路径类 syscall 使用 prep/post

| API | 作用 |
|-----|------|
| `dpele_new` | 创建，refcount = 1 |
| `dpele_ref` / `dpele_del` | refcount ±1，为 0 时销毁 |
| `dpele_dup` | 复制元素（EFD 会 dup fd），与 ref 独立 |

**谁创建谁释放**:`dpele_new` 或工厂得到的元素最终须 `dpele_del`（C++ `dpcpp::ele` 析构时自动 del）。

- **doing**:`dpevp_add` 提交后至 post 完成前 `dpele_is_doing` 为 true；期间不可对同一元素并发提交另一 asc（除非 prep 带 `DPASC_FLAG_ALLOW_DOING`）
- **detach**:`dpele_set_detach(ele, true)` 后异步完成不通过 `dpevp_pop` 交付，但仍须 `dpele_del`，detach 不替代 refcount
- **fd 所有权**:`dpefd_set_close(ele, false)` 时销毁元素不 close fd，由调用方接管
- **跨线程**:同一 `dpele_t*` 仅在其所属 worker 上 `dpevp_add`/`dpevp_pop`（CTC 除外）

推荐所有权模式:`创建 → dpevp_add / aexec → [DPE_WAIT → await] → dpele_ret → dpele_del`。C++ `co_await aexec` 不转移元素所有权。

### 数据区

`dpele_t` 上有**两块独立内存**，不可混用:

| API | 存储位置 | 生命周期 | 典型用途 |
|-----|----------|----------|----------|
| `dpele_aux_data(ele)` | 元素体内嵌区 | 与元素同寿 | TCP/UDP 的 `dpsockaddr_t`、SSL session、QUIC stream 状态等**类型私有数据** |
| `dpele_asc_data(ele)` | 堆上 scratch（按 asc 大小分配） | 随 asc 复用 | **当前/最近一次** asc 参数区，如 CTC/TMR 的回调与参数 |

要点:

- `aux_data` 随元素类型初始化/销毁；CTC/TMR 无有效 aux 载荷。
- `asc_data` 在 `dpevp_add` 的 prep 阶段写入；`dpele_asc_data` 为只读视图。
- `skt_connect` 省略 `addr` 时读的是 **`aux_data` 中的 sockaddr**，不是 `asc_data`。

Lua 映射:

| 方法 | 对应 C API | 适用 |
|------|------------|------|
| `ele:aux_data(ctype_)` | `dpele_aux_data` | EFD（TCP client 等） |
| `ctc:asc_data()` | `dpele_asc_data` | CTC topic/arg scratch |
| `tmr:asc_data()` | `dpele_asc_data` | timer 回调 cache id / arg（**非** aux_data） |


## dpasc:统一异步

C 核心在 `dpasc.h` 声明全部异步 IO / syscall 入口（`dpread`、`dpaio_read_some`、`dpskt_accept` 等）。各绑定层不重复实现语义，而是内联转发:

| 绑定 | 包装头 | 调用方式 |
|------|--------|----------|
| CWC | `dpcwc/dpcwc_asc.h` | `dpcwc_aio_read_some(efd, buf, 0)` → `dpcwc_aexec(ele, dpaio_read_some(), …)` |
| CPP | `dpcpp/dpcpp_asc.hh` | `co_await dpcpp::aio_read_some(efd, buf)` |
| Lua | `lib/dplua/dpasc.lua` | `dpasc.aio_read_some(peer, buf)` |

带默认参数的操作（如 `aio_*` 的 flags、超时）在 CWC/CPP 头文件与 Lua 侧单独实现，与 `dpasc.lua` 默认值对齐。

### dpsyc:跨线程路径 syscall

`dpsyc_*`（`open`/`statx`/`rename`/`xattr` 等）在 **CTC 元素**上通过 `dpevp_add` 异步执行:发起 worker 解析参数，目标 worker 执行 syscall。

其价值不在于「把一次 syscall 做得更快」，而在于:发起线程在返回 `DPE_WAIT` 后即可挂起或继续处理其他就绪元素，**不会长时间阻塞在 syscall 上**，从而不拖累同线程上的网络 I/O、定时器及其他异步操作。适合与事件循环 / 协程并发混用。

| 场景 | 建议 |
|------|------|
| 与 TCP/QUIC 等同 worker 并发、偶发文件访问 | `dpsyc_*` + `await` |
| 启动阶段批量建目录、同步配置加载 | 直接 libc syscall 或专用线程 |
| 大文件连续读写热路径 | 评估是否独立线程或 mmap，而非每读一次跨线程 |

Lua:`dpasc.syc_open(ele, …)` 等；CWC/CPP:`dpcwc_asc.h` / `dpcpp_asc.hh` 中同名包装。元素须 `dpele_new(dpctc_init_type(), toid, detach)`。

### CTC:`dpctc_submit` 与带 post 的 asc

CTC 元素上可挂两类 asc，差别在于 **`post` 是否为空** 以及目标 worker 上由谁干活。

| | `dpctc_submit` | `dpsyc_*` 等 |
|--|----------------|--------------|
| post | 无 | 有，在目标 worker 执行 syscall |
| prep（发起方） | 写入回调与参数到 `asc_data` | 解析路径等到 `asc_data` |
| 目标 worker | 绑定层执行用户回调 → `dpevp_end_ctc_` | asc `post` 完成 syscall，结果自动回传 |
| 典型用法 | `dpcwc_ctc_once` / `dpcpp::ctc_once` | `dpsyc_open` / `dpasc.syc_open` |

流程概览:发起方 `dpevp_add` → prep 写 `asc_data` → 投递目标 worker → 完成后回到发起方 `dpevp_pop` / `await`（`detach` 元素不 pop）。`toid` 在创建 CTC 时指定；`dpctc_reto` 可改投其他 worker。

**选型**:框架内路径 syscall 用带 post 的 `dpsyc_*`；在目标线程跑业务逻辑用 `dpctc_submit`（post 留空，参数走 `asc_data`）。

### 扩展 dpasc_t

`dpasc_t` 描述一次 `dpevp_add(ele, asc, …)`:**prep** 解析参数，**post** 执行 I/O / syscall（可为 `NULL`）。对外入口为 `dp*_xxx()`（见 `dpasc.h`）。

#### `dpasc_t` 字段（扩展 asc 时关注）

| 字段 | 含义 |
|------|------|
| `prep` / `post` | 参数解析与实际执行；`post` 可空（如 `dpctc_submit`、纯定时器） |
| `types` / `iotypes` | 适用的元素类型与 I/O 子类型 |
| `datasz` | `asc_data` scratch 大小；0 表示无 scratch |
| `flags` | 如 `DPASC_FLAG_ALLOW_DOING`:元素已 doing 时仍允许提交 |

prep/post 共享 `asc_data`（经 `dpasc_out_t.data`），**不要**写入 `aux_data`。

#### `dpasc_out_t`

prep/post 的出参结构，与 `dpele_asc_data` 配合使用:

| 部分 | 用途 |
|------|------|
| `data` | 指向本次 asc 的 scratch，prep 写入、post 读取 |
| 事件字段（`want_events` 等） | EFD 异步 I/O:登记/消耗 epoll（或 kqueue）就绪事件；CTC/TMR 通常忽略 |
| `step_ret` | 少数分步 asc 的中间结果（与事件字段互斥） |

#### `dpevp_add` 流程

1. 校验 asc 与元素类型、是否允许并发提交。
2. **prep**:解析 `dpevp_add` 后续参数到 `asc_data`。
3. prep 返回 `DPE_CONTINUE` 时按元素类型继续:EFD 等待 fd 就绪并调 post；CTC 投递目标 worker；定时器挂起等待到期。
4. 完成后 `dpevp_pop` / `await` 取结果；`dpele_ret` 为返回值。

prep 需异步继续时**统一返回 `DPE_CONTINUE`**，**不返回** `DPE_WAIT`（`DPE_WAIT` 留给 post 分步 I/O）。prep 同步完成可返回 `DPE_OK`/正数或错误码。

post 成功多为字节数或 `DPE_OK`；EFD 上未做完可返回 `DPE_WAIT` 并更新 `want_events`。
