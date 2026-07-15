# dpapp 项目开发指南

## 项目结构

```
.
├── app/          # 应用示例 (cwc/cpp/lua)
├── bin/          # 入口 (dpapp_all.cc 编译为 dpapp 二进制)
├── lib/          # 核心库
│   ├── dpapp/    # C 核心层 (事件循环、dpasc、SSL、QUIC、缓冲区等)
│   ├── dpaco/    # C 有栈协程
│   ├── dpcwc/    # C 协程封装层 (dpcwc_asc.h 内联包装)
│   ├── dpcpp/    # C++20 协程封装层 (dpcpp_asc.hh 内联包装)
│   ├── dplua/    # Lua 绑定；宿主脚本 dp*.lua，运行时 dplua.lua
├── tst/          # 测试用例
├── cmake/        # CMake 模块
└── doc/          # 文档
```

## 多语言绑定架构

核心逻辑在 `lib/dpapp/` (C 层)，各语言绑定层提供异步封装：

| 层 | 目录 | IO 模型 | 异步机制 |
|---|---|---|---|
| C 核心 | `lib/dpapp/` | 事件驱动 | 回调 + MCT/SYC；`dpasc` 统一 syscall |
| CWC (C 协程) | `lib/dpcwc/` | 同步协程 | `dpcwc_await` + `dpcwc_asc.h` |
| CPP (C++20) | `lib/dpcpp/` | 异步协程 | `co_await` + `dpcpp_asc.hh` |
| Lua | `lib/dplua/` | 异步回调 | `dplua_yield` + post-callback；`dpasc.lua` |

修改 BUG 或添加功能后，务必检查所有绑定层是否需要相同修改。

## 命名规范

全项目统一 **snake_case**，拒绝驼峰。函数/方法名 **动词在后**：`<前缀>_<模块>_<动词>`。

### 公用 API 与 internal

| 层级 | 格式 | 声明位置 | 示例 |
|------|------|----------|------|
| 对外入口 | `<层>_<动词>` | 各层主 `.h` | `dplua_start`、`dpcpp::start` |
| 跨模块公用 | `<层>_<模块>_<动词>` | 公共头 / `*_pri.h` | `dplua_yield`、`dpele_del`、`dpcwc_tcp_client` |
| 模块注册/导出 | 各绑定层约定 | 实现 `.c` | `dplua_mod_export_tcp` |
| 文件内 static | `_<层>_<模块>_<动词>` | 仅本翻译单元 | `_dplua_tcp_cfn_listen`、`_dpevp_watch_efd` |

**原则：** 头文件中只声明对外可见符号；`.c` / `.cc` 内 static 及仅文件内链接的实现统一 `_` 前缀，避免与公用 API 混淆。

### 各层前缀

| 层 | 目录 | 公用前缀 | internal 前缀 | 语言绑定回调 |
|---|---|---|---|---|
| C 核心 | `lib/dpapp/` | `dp*` / `dpele_*` / `dptcp_*` 等 | `_dp*` / `_dpele_*` | — |
| CWC | `lib/dpcwc/` | `dpcwc_*` | `_dpcwc_*` 或 `_` + 动词 | — |
| CPP | `lib/dpcpp/` | `dpcpp::` 命名空间 | `static _` + 动词 | — |
| Lua | `lib/dplua/` | `dplua_*`（C API） | `_dplua_*` 或 `_` + 动词 | Lua C 函数 `l_*` |

**Lua 宿主脚本**（源码 `lib/dplua/*.lua`；运行时安装至 `lua/dplua/`）：文件名与 `require` 短名一致（如 `dpasc.lua` → `require("dpasc")`）；运行时入口保留 `dplua.lua`。C 符号仍用 `dplua_*`。

### 类型 / 宏 / 全局

- 类型：`<层>_*_t`（如 `dplua_config_t`），**不加** leading `_`。
- 宏：`DPAPP_*`、`DPLUA_*` 等模块大写前缀。
- 线程/模块全局：`g_*` 或 `_g_*`（仅文件/模块内）。

### 头文件与精简

- `*_pri.h` 或各层私有头：仅保留**跨文件/跨模块**复用的类型、宏、API；其余在 `.c` 内 `static` 或 forward 声明。
- 不写一行式 trivial helper（如 `_jv_i32` 包一层 `JS_ToInt32`）；逻辑直接在调用处展开。
- 不抽仅转发一次的 wrapper。
- 批量重命名 internal 符号时，须排除公用前缀（如 `dplua_mod_*`、`dpele_*`），防止误改对外 API。

## 代码规范

- 不写无用的防御性代码
- 深究问题根源，不写补丁代码
- 不写适配旧代码的兼容性代码或兼容层
- 使用中文注释，对外接口按 Doxygen 标准注释（简要、精准）
- 变量/方法命名见上文「命名规范」
- 重要功能确定、完成且必要时，添加测试用例
- 自动 review：逻辑检查、代码复用、功能对齐、BUG 检查、功能优化、代码精简
- 自动使用 `clang-format` + 项目 `.clang-format` 格式化修改的文件

## 日志规范

- 日志服务于**问题定位、错误记录**与**运行时关键信息**，英文输出
- 在**失败发生处**记录一次，避免多层重复
- 高频 I/O 热路径（读写字节、协程 yield）保持静默
- 合理使用 `dplog.h` 级别分层

## 执行规范

- 输出中文推理，过程，结论等
- 解决问题碰壁时，善用网络搜索，持续自动修改，编译测试3轮后还未完成，即刻停止并输出总结
- 指令有歧义时，直接提出由用户选择或解决
- 修改 BUG 或添加功能后，检查其他语言绑定层是否有相同问题或需要相同功能
- 修改完成后编译验证（使用 `.vscode/settings.json` 中 cmake 选项）
- 用户未指定时不运行测试用例
- 必要时更新 `README.md`、`doc/*.md`
- 执行结束后提供系统性的下一步建议（数字编号）
- 输入数字编号即从上一步选择对应建议继续执行

## GIT 规范

- 使用中文撰写提交说明,控制在 100 字以内，写清「为什么」而非堆砌文件名
- 无关修改分开提交（例如代码修复与 `AGENTS.md` 文档更新应分两次 commit）
- **作者仅保留本地 Git 用户**（`user.name` / `user.email`），不得出现 AI等共同作者,**禁止**在提交说明中加入 `Co-authored-by:`、`Signed-off-by:`（AI）等署名 trailer；
