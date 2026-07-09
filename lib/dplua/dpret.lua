local E = {}

-- Linux 基础 errno
E.OK = 0 -- 成功
E.PERM = -1 -- 操作不允许
E.NOENT = -2 -- 无此文件或目录
E.SRCH = -3 -- 无此进程
E.INTR = -4 -- 系统调用被中断
E.IO = -5 -- I/O 错误
E.NXIO = -6 -- 无此设备或地址
E.TOOBIG = -7 -- 参数列表过长
E.NOEXEC = -8 -- 执行格式错误
E.BADF = -9 -- 错误的文件号
E.CHILD = -10 -- 无子进程
E.AGAIN = -11 -- 重试
E.NOMEM = -12 -- 内存不足
E.ACCES = -13 -- 权限拒绝
E.FAULT = -14 -- 错误的地址
E.NOTBLK = -15 -- 需要块设备
E.BUSY = -16 -- 设备或资源忙
E.EXIST = -17 -- 文件已存在
E.XDEV = -18 -- 跨设备链接
E.NODEV = -19 -- 无此设备
E.NOTDIR = -20 -- 不是目录
E.ISDIR = -21 -- 是目录
E.INVAL = -22 -- 无效参数
E.NFILE = -23 -- 文件表溢出
E.MFILE = -24 -- 打开文件过多
E.NOTTY = -25 -- 不是打字机
E.TXTBSY = -26 -- 文本文件忙
E.FBIG = -27 -- 文件过大
E.NOSPC = -28 -- 设备无空间
E.SPIPE = -29 -- 非法 seek
E.ROFS = -30 -- 只读文件系统
E.MLINK = -31 -- 链接过多
E.PIPE = -32 -- 管道破裂
E.DOM = -33 -- 数学参数超出函数定义域
E.RANGE = -34 -- 数学结果不可表示

-- Linux 全部 errno
E.DEADLK = -35 -- 资源死锁 */
E.NAMETOOLONG = -36 -- 文件名过长 */
E.NOLCK = -37 -- 无可用记录锁 */
E.NOSYS = -38 -- 无效的系统调用号 */
E.NOTEMPTY = -39 -- 目录非空 */
E.LOOP = -40 -- 遇到过多符号链接 */
E.WOULDBLOCK = -E.AGAIN -- 操作将阻塞 */
E.NOMSG = -42 -- 无所需类型的消息 */
E.IDRM = -43 -- 标识符已删除 */
E.CHRNG = -44 -- 通道号超出范围 */
E.L2NSYNC = -45 -- 第 2 层未同步 */
E.L3HLT = -46 -- 第 3 层已停止 */
E.L3RST = -47 -- 第 3 层已重置 */
E.LNRNG = -48 -- 链接号超出范围 */
E.UNATCH = -49 -- 协议驱动未附加 */
E.NOCSI = -50 -- 无可用 CSI 结构 */
E.L2HLT = -51 -- 第 2 层已停止 */
E.BADE = -52 -- 无效交换 */
E.BADR = -53 -- 无效请求描述符 */
E.XFULL = -54 -- 交换已满 */
E.NOANO = -55 -- 无阳极 */
E.BADRQC = -56 -- 无效请求码 */
E.BADSLT = -57 -- 无效槽 */

E.DEADLOCK = E.DEADLK

E.BFONT = -59 -- 错误的字体文件格式 */
E.NOSTR = -60 -- 设备不是流 */
E.NODATA = -61 -- 无可用数据 */
E.TIME = -62 -- 定时器到期 */
E.NOSR = -63 -- 流出资源 */
E.NONET = -64 -- 机器不在网络上 */
E.NOPKG = -65 -- 包未安装 */
E.REMOTE = -66 -- 对象是远程的 */
E.NOLINK = -67 -- 链接已断开 */
E.ADV = -68 -- 广告错误 */
E.SRMNT = -69 -- Srmount 错误 */
E.COMM = -70 -- 发送时通信错误 */
E.PROTO = -71 -- 协议错误 */
E.MULTIHOP = -72 -- 尝试多跳 */
E.DOTDOT = -73 -- RFS 特定错误 */
E.BADMSG = -74 -- 不是数据消息 */
E.OVERFLOW = -75 -- 值超出定义的数据类型范围 */
E.NOTUNIQ = -76 -- 网络上的名称不唯一 */
E.BADFD = -77 -- 文件描述符状态异常 */
E.REMCHG = -78 -- 远程地址已变更 */
E.LIBACC = -79 -- 无法访问所需的共享库 */
E.LIBBAD = -80 -- 访问损坏的共享库 */
E.LIBSCN = -81 -- a.out 中 .lib 段损坏 */
E.LIBMAX = -82 -- 试图链接过多共享库 */
E.LIBEXEC = -83 -- 无法直接执行共享库 */
E.ILSEQ = -84 -- 非法字节序列 */
E.RESTART = -85 -- 中断的系统调用应重启 */
E.STRPIPE = -86 -- 流管道错误 */
E.USERS = -87 -- 用户过多 */
E.NOTSOCK = -88 -- 对非套接字执行套接字操作 */
E.DESTADDRREQ = -89 -- 需要目标地址 */
E.MSGSIZE = -90 -- 消息过长 */
E.PROTOTYPE = -91 -- 协议类型与套接字不符 */
E.NOPROTOOPT = -92 -- 协议不可用 */
E.PROTONOSUPPORT = -93 -- 协议不支持 */
E.SOCKTNOSUPPORT = -94 -- 套接字类型不支持 */
E.OPNOTSUPP = -95 -- 传输端点不支持该操作 */
E.PFNOSUPPORT = -96 -- 协议族不支持 */
E.AFNOSUPPORT = -97 -- 协议不支持地址族 */
E.ADDRINUSE = -98 -- 地址已被使用 */
E.ADDRNOTAVAIL = -99 -- 无法分配请求的地址 */
E.NETDOWN = -100 -- 网络已断开 */
E.NETUNREACH = -101 -- 网络不可达 */
E.NETRESET = -102 -- 网络因重置而断开连接 */
E.CONNABORTED = -103 -- 软件导致连接中止 */
E.CONNRESET = -104 -- 对端重置连接 */
E.NOBUFS = -105 -- 无可用缓冲区空间 */
E.ISCONN = -106 -- 传输端点已连接 */
E.NOTCONN = -107 -- 传输端点未连接 */
E.SHUTDOWN = -108 -- 传输端点关闭后无法发送 */
E.TOOMANYREFS = -109 -- 引用过多：无法 splice */
E.TIMEDOUT = -110 -- 连接超时 */
E.CONNREFUSED = -111 -- 连接被拒绝 */
E.HOSTDOWN = -112 -- 主机已关闭 */
E.HOSTUNREACH = -113 -- 无路由到主机 */
E.ALREADY = -114 -- 操作已在进行中 */
E.INPROGRESS = -115 -- 操作正在进行中 */
E.STALE = -116 -- 过期的文件句柄 */
E.UCLEAN = -117 -- 结构需要清理 */
E.NOTNAM = -118 -- 不是 XENIX 命名类型文件 */
E.NAVAIL = -119 -- 无可用 XENIX 信号量 */
E.ISNAM = -120 -- 是命名类型文件 */
E.REMOTEIO = -121 -- 远程 I/O 错误 */
E.DQUOT = -122 -- 超出配额 */

E.NOMEDIUM = -123 -- 未找到介质 */
E.MEDIUMTYPE = -124 -- 介质类型错误 */
E.CANCELED = -125 -- 操作已取消 */
E.NOKEY = -126 -- 所需密钥不可用 */
E.KEYEXPIRED = -127 -- 密钥已过期 */
E.KEYREVOKED = -128 -- 密钥已被撤销 */
E.KEYREJECTED = -129 -- 密钥被服务拒绝 */

-- 健壮互斥锁
E.OWNERDEAD = -130 -- 所属者已死亡 */
E.NOTRECOVERABLE = -131 -- 状态不可恢复 */
E.RFKILL = -132 -- 因 RF-kill 无法操作 */
E.HWPOISON = -133 -- 内存页存在硬件错误 */

E.PARSE = E.PROTO
E.WAIT = E.AGAIN

-- 15x 自定义错误
E.UNKNOWN = -150 -- 未知错误
E.REPEAT = -151 -- 重复操作
E.UNINIT = -152 -- 未初始化
E.OPEN = -153 -- 打开失败
E.CLOSED = -154 -- 文件或源已关闭
E.SETATTR = -155 -- 设置属性失败
E.EOF = -156 -- 数据或文件末尾
E.NOTEXISTS = -157 -- 不存在
E.INITED = -158 -- 已初始化
E.BEINITED = -159 -- 因无效参数而初始化
E.PARAMTYPE = -160 -- 参数类型不匹配
E.PARAMMISS = -166 -- 缺少参数
E.PREPARE = -167 -- 准备或绑定参数错误
E.NOTAUTH = -168 -- 需要认证
E.NOTOPEN = -169 -- 源未打开
E.INVCMD = -170 -- 无效命令
E.AUTHED = -171 -- 已认证
E.DATATYPE = -172 -- 无效数据类型
E.NOEVENT = -173 -- 无事件监听
E.AUTHERR = -174 -- 认证错误
E.UNSUPPORT = -175 -- 不支持
E.NOSOURCE = -176 -- 无源
E.DELETED = -177 -- 对象或源已删除
E.NOTENOUGH = -178 -- 数据或缓冲区空间不足
E.PARTIALOK = -179 -- 部分成功
E.IGNORE = -180 -- 忽略操作

-- Http 错误码扩展

-- 1xx 信息（调整到 19x 范围）
E.CONTINUE = -190 -- 继续
E.SWITCHING_PROTOCOLS = -191 -- 切换协议
E.PROCESSING = -192 -- 处理中
E.EARLY_HINTS = -193 -- 早期提示

-- 2xx 成功
E.RESOK = -200 -- 成功
E.CREATED = -201 -- 已创建
E.ACCEPTED = -202 -- 已接受
E.NON_AUTHORITATIVE = -203 -- 非权威信息
E.NO_CONTENT = -204 -- 无内容
E.RESET_CONTENT = -205 -- 重置内容
E.PARTIAL_CONTENT = -206 -- 部分内容
E.MULTI_STATUS = -207 -- 多状态
E.ALREADY_REPORTED = -208 -- 已报告
E.IM_USED = -226 -- IM 已使用

-- 3xx 重定向
E.MULTIPLE_CHOICES = -300 -- 多项选择
E.MOVED_PERMANENTLY = -301 -- 永久移动
E.FOUND = -302 -- 已找到
E.SEE_OTHER = -303 -- 参见其他
E.NOT_MODIFIED = -304 -- 未修改
E.USE_PROXY = -305 -- 使用代理
E.TEMPORARY_REDIRECT = -307 -- 临时重定向
E.PERMANENT_REDIRECT = -308 -- 永久重定向

-- 4xx 客户端错误
E.BAD_REQUEST = -400 -- 错误请求
E.UNAUTHORIZED = -401 -- 未授权
E.PAYMENT_REQUIRED = -402 -- 需要付款
E.FORBIDDEN = -403 -- 禁止
E.NOT_FOUND = -404 -- 未找到
E.METHOD_NOT_ALLOWED = -405 -- 方法不允许
E.NOT_ACCEPTABLE = -406 -- 不可接受
E.PROXY_AUTH_REQUIRED = -407 -- 需要代理认证
E.REQUEST_TIMEOUT = -408 -- 请求超时
E.CONFLICT = -409 -- 冲突
E.GONE = -410 -- 已删除
E.LENGTH_REQUIRED = -411 -- 需要长度
E.PRECONDITION_FAILED = -412 -- 前提条件失败
E.POST_TOO_LARGE = -413 -- 载荷过大
E.URI_TOO_LONG = -414 -- URI 过长
E.UNSUPPORTED_MEDIA = -415 -- 不支持的媒体类型
E.RANGE_INVALID = -416 -- 范围不可满足
E.EXPECTATION_FAILED = -417 -- 期望失败
E.TEAPOT = -418 -- 我是茶壶
E.UNPROCESSABLE = -422 -- 无法处理的实体
E.TOO_EARLY = -425 -- 太早
E.UPGRADE_REQUIRED = -426 -- 需要升级
E.PRECONDITION_REQ = -428 -- 需要前提条件
E.TOO_MANY_REQUESTS = -429 -- 请求过多
E.HEADER_TOO_LARGE = -431 -- 请求头字段过大
E.LEGAL_UNAVAILABLE = -451 -- 因法律原因不可用

-- 5xx 服务器错误
E.INTERNAL_ERROR = -500 -- 内部服务器错误
E.NOT_IMPLEMENTED = -501 -- 未实现
E.BAD_GATEWAY = -502 -- 错误网关
E.SERVICE_UNAVAILABLE = -503 -- 服务不可用
E.GATEWAY_TIMEOUT = -504 -- 网关超时
E.HTTP_NOT_SUPPORTED = -505 -- HTTP 版本不支持
E.VARIANT_NEGOTIATES = -506 -- 变体也协商
E.INSUFFICIENT_STORAGE = -507 -- 存储不足
E.LOOP_DETECTED = -508 -- 检测到循环
E.NOT_EXTENDED = -510 -- 未扩展
E.NETWORK_AUTH_REQ = -511 -- 需要网络认证

local ffi = require("ffi")

ffi.cdef([[
typedef int dpret_t;
const char* dperr_detail(int err);
const char* dperr_http_detail(int http_status);
]])

local M = {}

function M.detail(e)
    return ffi.string(ffi.C.dperr_detail(tonumber(e)))
end

function M.http_detail(http_status)
    return ffi.string(ffi.C.dperr_http_detail(tonumber(http_status)))
end

function M.isok(e)
    return e ~= nil and e >= 0
end

function M.iserr(e)
    return e == nil or e < 0
end

return setmetatable(E, {
    __index = M,
    __newindex = function(k, v)
        error("Not allowed")
    end,
})
