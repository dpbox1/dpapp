## 多线程

dpapp 使用多线程模型，但不是线程池。所有线程会在启动阶段按“类型 + 数量”一次性创建。

dpapp 首先定义线程类型，你可以把它理解为线程组。每类线程都可以通过命令行参数指定创建数量，不同线程类型可执行不同入口函数。

基于工程经验和实现边界，单个 dpapp 实例最多支持 63 种线程类型、总线程数不超过 255。对大多数应用和 CPU 规模而言这个上限已足够。

线程创建后会先初始化该线程专属事件循环，再初始化协程环境，最后在协程中执行模块初始化函数并进入主逻辑。

如果在命令行中把某类线程数量设为 0，则该类型会被禁用且不会创建。这对临时关闭某些功能很有用。

实际运行中，每个线程都是独立执行体，不共享业务数据。线程间协作主要通过 master 线程进行异步任务传递。

## 异步IO

Linux 下，dpapp 以 epoll + 非阻塞 fd + 协程实现异步 IO：`dpefd` 封装 fd，`dpevp` 驱动事件循环，`dpasc` 描述单次 IO 的 prep/post。

典型流程：

- 发起 IO 时，若 fd 暂无足够数据，prep 挂起协程，`dpefd` 记录偏移等状态；流式 IO 用 `dpbuf` 保存上下文。
- `dpevp` 在 epoll 就绪后调用 post，推进未完成的 IO。
- 完成或出错后唤醒协程，返回结果。

事件通知采用 epoll **边沿触发（ET）**，而非水平触发（LT）。fd 创建时向 epoll 注册 IN|OUT；协程挂起期间由 `want_events` 标记当前关注方向，其余事件在用户态丢弃。ET 使 idle 连接不会因子队列可写而反复进入 epoll 批次——这是本架构能全量注册 interest 的前提。若改 LT，则须为每次 IO 动态 `epoll_ctl` 增删监听，复杂度更高且无性能收益，故不采用。

## 异步任务（线程间过程调用和消息队列）

这部分的核心分歧通常在任务队列实现上，比如 lockfree 队列或 lockfree 环形队列。类似方案在 Boost.Lockfree、Seastar 等项目中都有成熟实践。

dpapp 实现了一个高性能线程间消息队列。

dpapp 使用纯 list + eventfd 设计。除 master 线程外，每个线程额外使用 2 个 eventfd，并与 master 线程连接，形成星型通知网络。

代价是：

- 需要 2 倍线程数的 fd 消耗。

- eventfd 读写本身也有一定开销。

优势是：

- 任意线程都可向其他线程发送任务/消息，也可接收来自其他线程的任务/消息。

- 线程间数据传递不再依赖锁，甚至不需要原子变量（仅任务状态使用原子变量）。

- list 只要内存足够基本无固定长度限制，天然具备削峰能力。

- 对比传统 lockfree 队列，具备更高性能潜力。

- Master ↔ child paths are fastest (~2× child ↔ child in theory).

## 绑定层与异步模型

核心能力在 `lib/dpapp/`，各语言绑定提供等价异步语义：

| 层 | 异步机制 | IO 包装 |
|---|---|---|
| CWC | `dpcwc_await` 挂起有栈协程 | `dpcwc_asc.h` → `dpcwc_aexec` |
| CPP | C++20 `co_await` | `dpcpp_asc.hh` → `dpcpp::aexec` |
| Lua | `dplua_yield` + post 回调 | `dpasc.lua` |

修改核心行为时，应检查各绑定层是否需同步调整（见 `AGENTS.md`）。

## 服务端框架 dpsvr

TCP / domain socket / QUIC 多 listener 服务端逻辑集中在核心模块，应用只实现 per-connection handler：

- Lua: `lib/dplua/dpsvr.lua`，`require("dpsvr")`
- CWC: `dpcwc_server_start()`（`dpcwc_aux.h`）

`params` 数组每项描述一个 listener（type、host、port、handler、ssl、alpn 等）；handler 在新协程中处理单连接。字段说明见 `dpsvr.lua` 文件头注释。

## 脚本模块布局

| 运行时 | 路径 | 加载方式 |
|--------|------|----------|
| Lua | `root_dir/lua/dplua/dp*.lua` | `require("dpasc")` 等短名 |
| Lua 入口 | `root_dir/lua/dplua/dplua.lua` | dplua 引导 |

C 符号命名（`dplua_*`）与脚本短名分离：脚本文件名对齐 `require`，便于阅读与部署。
