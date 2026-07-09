# dpapp

dpapp 是基于协程的轻量级多线程异步 IO/任务框架，一套 C 核心，内置 C、C++、Lua 三套应用绑定。

---

个人业余项目，聚焦异步 IO/任务，为上层应用与工具链提供统一、轻量的底层基石。

## 功能

- epoll 边沿触发事件循环，协程内同步写法、异步执行
- 多语言绑定：C 有栈协程（dpaco/dpcwc）、C++20 协程（dpcpp）、LuaJIT（dplua）
- 基于事件的高效跨线程异步调用（CTC）
- 异步 IO：TCP、UDP、domain socket、pipe、signalfd、串口、文件变更通知等
- 定时器、sleep 与异步操作超时
- 异步 SSL/TLS、QUIC（lsquic）、基础 HTTP/3
- 统一 IO 抽象（AIO）与异步扩展层（ASC）
- 模块式扩展（`.so` / `.lua`）
- 灵活的多线程类型管理

## 项目结构

```
.
├── app/          # 示例：cwc / cpp / lua
├── bin/          # dpapp 入口
├── lib/
│   ├── dpapp/    # C 核心（事件循环、dpasc、SSL、QUIC…）
│   ├── dpaco/    # C 有栈协程
│   ├── dpcwc/    # C 协程绑定 + dpcwc_asc.h 内联包装
│   ├── dpcpp/    # C++20 协程 + dpcpp_asc.hh 内联包装
│   └── dplua/    # Lua 绑定；宿主脚本 dp*.lua
├── tst/          # 测试
└── doc/          # 文档
```

## 文档

- [构建](doc/build.md)：编译选项、依赖、安装布局
- [开始](doc/start.md)：用法/示例，快速上手
- [开发](doc/develop.md)：架构、模块约定、API 语义（`dpret`/`dpele`）、各语言绑定
- [细节](doc/detail.md)：多线程、异步 IO、线程间消息
- [基准测试](doc/benchmark.md)：性能数据与测试方法

## 许可

本项目基于 **Apache License 2.0** 发布，详情见根目录中的 `LICENSE` 文件。

## AI 辅助说明

本项目部分内容在开发过程中部分借助 AI 工具完成，主要包括：

1. 部分示例、功能与测试代码，以及自动补全代码块。
2. 自动测试、BUG排查等。
3. 部分头文件中的注释、API/Doxygen 说明，以及构建/安装相关的脚本片段。
4. 文档的结构调整、排版与内容整理。
5. 参考[AGENTS](AGENTS.md)。

以上内容仍由维护者审阅与裁剪；若发现错误或不妥之处，欢迎直接提出 issue 或 PR。

![logo](doc/dpapp-logo.svg)
