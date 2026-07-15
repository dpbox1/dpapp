/** @file dpcpp_buf.hh
 *  @ingroup dpcpp_buf
 *  @brief `dpbuf_t` 的 C++ RAII 封装。
 *
 *  提供追加、解析、搜索、格式化等便捷 API，同时保留引用计数存储。
 *
 *  拷贝仅增加引用计数而不复制字节——使用 `dup_r()` / `dup_e()` 进行深拷贝。
 *  `get()` / `take()` 暴露底层 C 指针；转换为 %std::string_view / %std::string 在
 *  流式传输协议数据时可避免额外拷贝。热路径上建议复用 `buf` 实例。 */
#pragma once

#include "dpapp/dpbuf.h"
#include <compare>
#include <cstdarg>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <functional>
#include <iostream>
#include <string_view>
#include <utility>

namespace dpcpp
{
/** @brief 带共享所有权和便捷访问器的 RAII `dpbuf_t`。 */
struct buf final
{
    /** 内容类型（对应 `dpbuf_utype_e`）。 */
    enum
    {
        UT_TEXT = DPBUF_UT_TEXT,
        UT_BLOB = DPBUF_UT_BLOB,
        UT_ERRO = DPBUF_UT_ERRO,
        UT_JSON = DPBUF_UT_JSON,
        UT_USER = DPBUF_UT_USER,
    };

    /** 用户标志位（对应 `dpbuf_uflag_e`）。 */
    enum
    {
        UF_SHORT = DPBUF_UF_SHORT,
        UF_ALL = DPBUF_UF_ALL,
    };

    dpbuf_t* _buf = nullptr;

    /** @brief 包装已有 `dpbuf_t*`（接管所有权）。 */
    buf(dpbuf_t* b = nullptr) : _buf(b)
    {}

    /** @brief 共享已有缓冲（引用计数++）。 */
    explicit buf(const dpbuf_t* b) : _buf(b ? dpbuf_new_r(b) : nullptr)
    {}

    /** @brief 分配空缓冲，初始可写容量 `size` 字节。 */
    explicit buf(int size) : _buf(dpbuf_new(size))
    {}

    /** @brief 包装调用方内存（`dpbuf_new_d`）。 */
    buf(void* data, int size, int mode) : _buf(dpbuf_new_d(data, size, mode))
    {}

    /** @brief printf 风格格式化创建（`dpbuf_new_f`）。 */
    template <typename... Args>
    buf(const char* fmt, Args&&... args)
        : _buf(dpbuf_new_f(fmt, std::forward<Args>(args)...))
    {}

    /** @brief 拷贝构造（共享底层存储，refcount++）。 */
    buf(const buf& other) : _buf(dpbuf_new_r(other._buf))
    {}

    /** @brief 移动构造（转移所有权）。 */
    buf(buf&& other) noexcept : _buf(other._buf)
    {
        other._buf = nullptr;
    }

    /** @brief 析构并减引用（为零时释放）。 */
    ~buf()
    {
        if (_buf) {
            dpbuf_del(_buf);
        }
    }

    /** @brief 拷贝赋值（共享底层存储）。 */
    buf& operator=(const buf& other)
    {
        if (this != &other) {
            if (_buf) {
                dpbuf_del(_buf);
            }
            _buf = dpbuf_new_r(other._buf);
        }
        return *this;
    }

    /** @brief 移动赋值。 */
    buf& operator=(buf&& other) noexcept
    {
        if (this != &other) {
            if (_buf) {
                dpbuf_del(_buf);
            }
            _buf = other._buf;
            other._buf = nullptr;
        }
        return *this;
    }

    /** @brief 深拷贝可读窗口（`dpbuf_dup_r`）。 */
    buf dup_r() const
    {
        return buf(dpbuf_dup_r(_buf));
    }

    /** @brief 深拷贝尾部 slack（`dpbuf_dup_e`）。 */
    buf dup_e() const
    {
        return buf(dpbuf_dup_e(_buf));
    }

    /** @brief 复制 utype/uflag 到 `dst`。 */
    void cpusr(buf& dst) const
    {
        dpbuf_cpusr(_buf, dst._buf);
    }

    /** @brief 共享底层存储的引用计数。 */
    size_t refc() const
    {
        return dpbuf_refc(_buf);
    }

    /** @brief 已分配总容量。 */
    int size() const
    {
        return dpbuf_size(_buf);
    }

    /** @brief 底层数组基址。 */
    void* data() const
    {
        return dpbuf_data(_buf);
    }

    /** @brief 逻辑载荷类型（`utype`）。 */
    int utype() const
    {
        return _buf->utype;
    }

    /** @brief 设置逻辑载荷类型。 */
    void set_utype(int utype)
    {
        _buf->utype = utype;
    }

    /** @brief 查询用户标志位（`uflag & mask`）。 */
    int uflag(int uflag = DPBUF_UF_ALL) const
    {
        return _buf->uflag & uflag;
    }

    /** @brief 清除用户标志位。 */
    void rmv_uflag(int uflag)
    {
        _buf->uflag &= ~uflag;
    }

    /** @brief 设置用户标志位。 */
    void add_uflag(int uflag)
    {
        _buf->uflag |= uflag;
    }

    /** @brief compact 回收空间；`force` 强制回收。 */
    void recycle(bool force = false)
    {
        dpbuf_recycle(_buf, force);
    }

    /** @brief 启用/禁用读后 compact（`DPBUF_NORECYCLE`）。 */
    void set_recycle(bool b)
    {
        dpbuf_set_recycle(_buf, b);
    }

    /** @brief 按 `mode` 重置游标（`DPBUF_INIT_R/W`）。 */
    void reset(int mode)
    {
        dpbuf_reset(_buf, mode);
    }

    /** @brief 扩容/缩容，保证可写区至少 `s` 字节。 */
    bool resizew(int s)
    {
        return dpbuf_resizew(_buf, s);
    }

    /** @brief 可读区指针 [rbeg, rend)。 */
    char* crdata() const
    {
        return dpbuf_crdata(_buf);
    }

    /** @brief 可读区字节数。 */
    int crsize() const
    {
        return dpbuf_crsize(_buf);
    }

    /** @brief 可写尾部指针 [wbeg, size)。 */
    char* cwdata() const
    {
        return dpbuf_cwdata(_buf);
    }

    /** @brief 可写尾部剩余字节数。 */
    int cwsize() const
    {
        return dpbuf_cwsize(_buf);
    }

    /** @brief slack 区指针 [rend, wbeg)。 */
    char* cedata() const
    {
        return dpbuf_cedata(_buf);
    }

    /** @brief slack 区字节数。 */
    int cesize() const
    {
        return dpbuf_cesize(_buf);
    }

    /** @brief slack 区是否为空。 */
    bool cempty() const
    {
        return dpbuf_cempty(_buf);
    }

    /** @brief 可读区是否与 `other` 前 `len` 字节相等。 */
    bool cequalc(const char* other, int len) const
    {
        return dpbuf_cequalc(_buf, other, len);
    }

    /** @brief 可读区字典序比较。 */
    int ccmp(const buf& other) const
    {
        return dpbuf_ccmp(_buf, other._buf);
    }

    /** @brief 从偏移 `left` 查找 `match`。 */
    int cfind(const char* match, int len = -1, int left = 0) const
    {
        return dpbuf_cfind(_buf, match, len, left);
    }

    /** @brief 可读区 C 字符串长度（至 '\0' 或 rend）。 */
    int cstrlen() const
    {
        return dpbuf_cstrlen(_buf);
    }

    /** @brief 可读区是否以 `sub` 开头。 */
    bool cbegwith(const char* sub, int len = -1, bool skip_begws = false) const
    {
        return dpbuf_cbegwith(_buf, sub, len, skip_begws);
    }

    /** @brief 移动读游标 `rbeg`。 */
    int rseek(int offset, int seek = SEEK_CUR)
    {
        return dpbuf_rseek(_buf, offset, seek);
    }

    /** @brief 移动写游标 `wbeg`。 */
    int wseek(int offset, int seek = SEEK_CUR)
    {
        return dpbuf_wseek(_buf, offset, seek);
    }

    /** @brief 移动 `rend`（划分可读与 slack）。 */
    int eseek(int offset, int seek = SEEK_CUR)
    {
        return dpbuf_eseek(_buf, offset, seek);
    }

    /** @brief 追加 `b` 可读区；`len` ≤0 时用 `crsize(b)`。 */
    int wbuf(const buf& b, int len = -1)
    {
        return dpbuf_wbuf(_buf, b._buf, len);
    }

    /** @brief 同 `wbuf`，并推进源 `b` 读游标。 */
    int wbuf_r(buf& b, int len = -1)
    {
        return dpbuf_wbuf_r(_buf, b._buf, len);
    }

    /** @brief 用字节 `v` 填充 `len` 字节到可写区。 */
    int wfill(int len, int8_t v)
    {
        return dpbuf_wfill(_buf, len, v);
    }

    /** @brief 追加原始数据到可写区。 */
    int wdata(const void* data, int len)
    {
        return dpbuf_wdata(_buf, data, len);
    }

    /** @brief printf 风格追加写入。 */
    template <typename... Args>
    int wstrf(const char* fmt, Args&&... args)
    {
        return dpbuf_wstrf(_buf, fmt, std::forward<Args>(args)...);
    }

    /** @brief `wstrf` 的 va_list 版。 */
    int wstrv(const char* fmt, va_list args)
    {
        return dpbuf_wstrv(_buf, fmt, args);
    }

    /** @brief 跳过 slack 前导空白，扩展 rend。 */
    int rws()
    {
        return dpbuf_rws(_buf);
    }

    /** @brief 从 slack 搬最多 `size` 字节到 `det`。 */
    int readto(buf& det, int size)
    {
        return dpbuf_readto(_buf, det._buf, size);
    }

    /** @brief 将 rend 推进 `len` 进入 slack。 */
    int rdata(int len)
    {
        return dpbuf_rdata(_buf, len);
    }

    /** @brief slack 不足 `len` 字节则失败。 */
    int rmust(int len)
    {
        return dpbuf_rmust(_buf, len);
    }

    /** @brief 消费至含 `until` 为止。 */
    int runtil(const char* until, int until_sz)
    {
        return dpbuf_runtil(_buf, until, until_sz);
    }

    /** @brief 消费 slack 中 C 字符串段。 */
    int rcstr()
    {
        return dpbuf_rcstr(_buf);
    }

    /** @brief 将全部 slack 并入可读区。 */
    int rall()
    {
        return dpbuf_rall(_buf);
    }

    /** @brief 裸 `dpbuf_t*`（不转移所有权）。 */
    dpbuf_t* get() const
    {
        return _buf;
    }

    /** @brief 释放所有权并将 wrapper 置空。 */
    dpbuf_t* take()
    {
        dpbuf_t* b = _buf;
        _buf = nullptr;
        return b;
    }

    /** @brief 成员访问运算符。 */
    dpbuf_t* operator->() const
    {
        return _buf;
    }

    /** @brief 隐式转换为裸指针。 */
    operator dpbuf_t*() const
    {
        return _buf;
    }

    /** @brief 是否持有非空缓冲。 */
    operator bool() const
    {
        return _buf != nullptr;
    }

    /** @brief 隐式转换为可读区 C 字符串。 */
    operator const char*() const
    {
        return crdata();
    }

    /** @brief 隐式转换为可读区 `string_view`。 */
    operator std::string_view() const
    {
        return std::string_view(crdata(), crsize());
    }

    /** @brief 隐式转换为可读区 `string` 副本。 */
    operator std::string() const
    {
        return std::string(crdata(), crsize());
    }

    /** @brief 从偏移 `left` 截取可读区为 `string_view`。 */
    std::string_view to_string_view(int left = 0) const
    {
        int sz = crsize();
        return sz > left ? std::string_view(crdata() + left, sz - left)
                         : std::string_view();
    }

    /** @brief 从偏移 `left` 截取可读区为 `string` 副本。 */
    std::string to_string(int left = 0) const
    {
        int sz = crsize();
        return sz > left ? std::string(crdata() + left, sz - left) : std::string();
    }

    /** @brief 三路比较（基于 `ccmp`）。 */
    std::partial_ordering operator<=>(const buf& other) const noexcept
    {
        int r = ccmp(other);
        if (r == 0) {
            return std::partial_ordering::equivalent;
        } else if (r > 0) {
            return std::partial_ordering::greater;
        } else {
            return std::partial_ordering::less;
        }
    }

    /** @brief 可读区内容相等比较。 */
    bool operator==(const buf& other) const noexcept
    {
        return ccmp(other) == 0;
    }

    /** @brief 可读区是否为空。 */
    bool empty() const
    {
        return crsize() == 0;
    }

    /** @brief 与 C 字符串比较相等。 */
    bool operator==(const char* other) const
    {
        return cequalc(other, -1);
    }

    /** @brief 与 `string_view` 比较相等。 */
    bool operator==(const std::string_view& other) const
    {
        return cequalc(other.data(), other.size());
    }
};

// std::ostream 流输出
/** @brief 将可读区写入输出流。 */
inline std::ostream& operator<<(std::ostream& os, const buf& b)
{
    if (!b || b.crsize() == 0) {
        return os;
    }
    const char* data = b.crdata();
    int len = b.crsize();
    os.write(data, len);
    return os;
}

} // namespace dpcpp

// 供无序容器使用的 std::hash 特化
namespace std
{
template <>
struct hash<dpcpp::buf>
{
    size_t operator()(const dpcpp::buf& b) const
    {
        if (!b || b.crsize() == 0) {
            return 0;
        }
        const char* data = b.crdata();
        return std::hash<std::string_view>{}(std::string_view(data, b.crsize()));
    }
};
} // namespace std
