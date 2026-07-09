/** @file dpdoxy_groups.h
 *  @brief Doxygen 主题树（无编译意义，仅供文档生成）。
 *
 *  层级约定：
 *  - C 核心 3 级：`dpapi` → `dpapp` → `dpapp_*`
 *  - 绑定层 2 级：`dpapi` → `dpcwc` / `dpcwc_asc` / `dpcpp_asc` / …（文件级 `@ingroup`）
 *
 *  头文件内小节用 `@name`，不嵌套 `@defgroup`。 */

/** @defgroup dpapi dpapp API
 *  @brief 事件循环、异步 I/O、多语言绑定。
 *  @{
 */

/** @defgroup dpapp C 核心（libdpapp）
 *  @brief 事件循环、`dpele`、dpasc、SSL/QUIC、CTC。
 *  @ingroup dpapi
 *  @{
 */

/** @defgroup dpapp_start 进程启动
 *  @brief `dpapp_arg_*`、`dpapp_start`、`dpapp_info`。
 *  @ingroup dpapp
 */

/** @defgroup dpapp_def 公用定义
 *  @brief `dpv32_t`/`dpv64_t`、`DPEVT_*`、`dprole_e`。
 *  @ingroup dpapp
 */

/** @defgroup dpapp_pret 返回值与错误码
 *  @brief `dpret_t`、`DPE_*`、`dpret_isok`。
 *  @ingroup dpapp
 */

/** @defgroup dpapp_log 日志
 *  @brief `dplog_*`、`dpelog_*`。
 *  @ingroup dpapp
 */

/** @defgroup dpapp_buf 缓冲区
 *  @brief `dpbuf_*`。
 *  @ingroup dpapp
 */

/** @defgroup dpapp_evp 事件循环与元素
 *  @brief `dpele_*`、`dpevp_*`、CTC/定时器。
 *  @ingroup dpapp
 */

/** @defgroup dpapp_efd 事件 fd
 *  @brief TCP/UDP/UDS/串口/signalfd/inotify/FIFO。
 *  @ingroup dpapp
 */

/** @defgroup dpapp_asc 异步 prep
 *  @brief `dpasc_t` 与 `dpevp_add` prep 描述符。
 *  @ingroup dpapp
 */

/** @defgroup dpapp_ssl TLS
 *  @brief 证书/ALPN 配置与 SSL 元素。
 *  @ingroup dpapp
 */

/** @defgroup dpapp_qic QUIC / HTTP3
 *  @brief lsquic engine、连接/流、HTTP/3 头集。
 *  @ingroup dpapp
 */

/** @} */ /* dpapp */

/** @defgroup dpaco 有栈协程（libdpaco）
 *  @brief `dpaco_*`。
 *  @ingroup dpapi
 */

/** @defgroup dpcwc C 协程运行时
 *  @brief `dpcwc_start`、`dpcwc_await`、`dpcwc_aexec`。
 *  @ingroup dpapi
 */

/** @defgroup dpcwc_asc 异步 IO（CWC）
 *  @brief 对齐 `dpasc.h` 的 `dpcwc_*` 包装。
 *  @ingroup dpapi
 */

/** @defgroup dpcwc_aux 扩展（CWC）
 *  @brief Snowflake、server、endpoint 工厂。
 *  @ingroup dpapi
 */

/** @defgroup dpcpp C++ 协程运行时
 *  @brief `dpcpp::start`、`await`、`aexec`。
 *  @ingroup dpapi
 */

/** @defgroup dpcpp_asc 异步 IO（C++）
 *  @brief 对齐 `dpasc.h` 的 `dpcpp::*` 包装。
 *  @ingroup dpapi
 */

/** @defgroup dpcpp_aux 扩展（C++）
 *  @brief server、endpoint、Snowflake。
 *  @ingroup dpapi
 */

/** @defgroup dpcpp_aco 协程桥接
 *  @brief ucoro awaitable、`aco_ele_awaitable`。
 *  @ingroup dpapi
 */

/** @defgroup dpcpp_ele 元素 RAII
 *  @brief `dpcpp::ele` / `efd` / `ctc` 包装。
 *  @ingroup dpapi
 */

/** @defgroup dpcpp_buf 缓冲区 RAII
 *  @brief `dpcpp::buf` 包装 `dpbuf_t`。
 *  @ingroup dpapi
 */

/** @defgroup dplua Lua 绑定（libdplua）
 *  @brief `dplua_start` 与 `dp*.lua`。
 *  @ingroup dpapi
 */

/** @defgroup dplua_ext Lua C 扩展
 *  @brief Snowflake ID、`dptask_comspec`。
 *  @ingroup dpapi
 */

/** @} */ /* dpapi */
