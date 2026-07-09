/** @file dpcpp_ele.hh
 *  @ingroup dpcpp_ele
 *  @brief `dpele_t` 的 C++ RAII 封装。
 *
 *  构造时将参数转发给 `dpele_new`；拷贝/shared 语义经 `dpele_ref`。
 *  `efd` / `ctc` / `tmr` / `asc` / `mct` 均为同一 `ele` 类型的别名。 */
#pragma once

#include "dpapp/dpevp.h"
#include <utility>

namespace dpcpp
{

/** @brief 带引用计数的 RAII `dpele_t*`；`efd` / `ctc` / … 别名均为同一类型。 */
struct ele final
{
    dpele_t* _ele = nullptr;

    /** @brief 按 `dpele_type_t` 创建新元素。 */
    template <typename... Args>
    ele(const dpele_type_t* type, Args... args)
    {
        _ele = dpele_new(type, std::forward<Args>(args)...);
    }

    /** @brief 包装已有 `dpele_t*`（不增引用）。 */
    ele(dpele_t* e = nullptr) : _ele(e)
    {}

    /** @brief 拷贝构造（`dpele_ref` 共享所有权）。 */
    ele(const ele& other) : _ele(other._ele ? dpele_ref(other._ele) : nullptr)
    {}

    /** @brief 移动构造（转移所有权）。 */
    ele(ele&& other) noexcept : _ele(other._ele)
    {
        other._ele = nullptr;
    }

    /** @brief 析构并 `dpele_del`。 */
    ~ele()
    {
        dpele_del(_ele);
    }

    /** @brief 拷贝赋值（先释放旧句柄再 ref）。 */
    ele& operator=(const ele& other)
    {
        if (this != &other) {
            dpele_del(_ele);
            _ele = other._ele ? dpele_ref(other._ele) : nullptr;
        }
        return *this;
    }

    /** @brief 移动赋值。 */
    ele& operator=(ele&& other) noexcept
    {
        if (this != &other) {
            dpele_del(_ele);
            _ele = other._ele;
            other._ele = nullptr;
        }
        return *this;
    }

    /** @brief 接管裸指针（先释放旧句柄）。 */
    ele& operator=(dpele_t* e)
    {
        if (_ele != e) {
            dpele_del(_ele);
            _ele = e;
        }
        return *this;
    }

    // -- lifetime --

    /** @brief 克隆句柄（共享完成语义）。 */
    ele dup() const
    {
        return ele(_ele ? dpele_dup(_ele, false) : nullptr);
    }

    /** @brief 当前引用计数。 */
    uint32_t refc() const
    {
        return _ele ? dpele_refc(_ele) : 0;
    }

    // -- type metadata --

    /** @brief 元素类型描述符。 */
    const dpele_type_t* type() const
    {
        return dpele_type(_ele);
    }

    /** @brief 类型绑定辅助数据区指针。 */
    void* aux_data() const
    {
        return dpele_aux_data(_ele);
    }

    // -- timeout --

    /** @brief 相对截止时间（秒）。
     *  @return 成功返回 0，失败返回负值。 */
    dpret_t set_timeout(double sec)
    {
        return dpele_set_timeout(_ele, sec);
    }

    /** @brief 当前相对超时（秒）。 */
    double timeout() const
    {
        return dpele_timeout(_ele);
    }

    // -- completion status --

    /** @brief 元素完成返回值。 */
    dpret_t ret() const
    {
        return dpele_ret(_ele);
    }

    /** @brief 设置完成返回值。 */
    void set_ret(dpret_t r)
    {
        dpele_set_ret(_ele, r);
    }

    // -- detach mode --

    /** @brief 为 true 时，元素完成后不 pop 交付（见 dpevp.h `dpele_set_detach`）。
     */
    dpret_t set_detach(bool detach)
    {
        return dpele_set_detach(_ele, detach);
    }

    /** @brief 是否处于 detach 模式。 */
    bool is_detach() const
    {
        return dpele_is_detach(_ele);
    }

    // -- runtime flags --

    /** @brief 元素是否仍有未完成的异步操作。 */
    bool is_doing() const
    {
        return dpele_is_doing(_ele);
    }

    // -- coroutine linkage --

    /** @brief 挂起于此元素的协程标识（`dpv64_t`）。 */
    dpv64_t cop() const
    {
        return dpele_cop(_ele);
    }

    /** @brief 挂起 `cop` 标识的协程，直到此元素触发完成。 */
    bool wait(dpv64_t cop)
    {
        return dpele_wait(_ele, cop);
    }

    // -- typed attachment --
    // spec/set_spec removed — use type-bound user data (dpv64_t) instead

    // -- pollable fd fields --

    /** @brief 可 poll 的底层 fd（EFD 元素）。 */
    int fd() const
    {
        return dpefd_fd(_ele);
    }

    /** @brief 完成返回值（同 `ret()`，int64 视图）。 */
    int64_t res() const
    {
        return dpele_ret(_ele);
    }

    void set_close(bool cl = true)
    {
        dpefd_set_close(_ele, cl);
    }

    // -- cross-thread call fields --

    /** @brief CTC 源线程 id。 */
    int ctc_fromid() const
    {
        return dpctc_fromid(_ele);
    }

    /** @brief CTC 目标线程 id。 */
    int ctc_toid() const
    {
        return dpctc_toid(_ele);
    }

    // -- timer fields --
    // remark: aux1/aux2 removed — use aux_data() instead

    // -- C interop --

    /** @brief 裸 `dpele_t*` 指针（不转移所有权）。 */
    dpele_t* get() const
    {
        return _ele;
    }

    /** @brief 释放所有权并将此 wrapper 置空。 */
    dpele_t* take()
    {
        dpele_t* e = _ele;
        _ele = nullptr;
        return e;
    }

    /** @brief 隐式转换为裸指针。 */
    operator dpele_t*() const
    {
        return _ele;
    }

    /** @brief 成员访问运算符。 */
    dpele_t* operator->() const
    {
        return _ele;
    }

    /** @brief 是否持有非空元素。 */
    operator bool() const
    {
        return _ele != nullptr;
    }
};

using efd = ele;
using ctc = ele;
using tmr = ele;
using asc = ele;
using mct = ele;

} // namespace dpcpp
