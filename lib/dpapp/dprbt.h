/*    $NetBSD: tree.h,v 1.8 2004/03/28 19:38:30 provos Exp $    */
/*    $OpenBSD: tree.h,v 1.7 2002/10/17 21:51:54 art Exp $    */

/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright 2002 Niels Provos <provos@citi.umich.edu>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/** @file dprbt.h
 *  @brief NetBSD/OpenBSD 树宏库：splay 树与 rank-balanced（红黑类）二叉搜索树。
 *
 *  纯 C 宏实现，无运行时 API；通过 `RB_*`/`SPLAY_*` 在编译期生成内联函数。
 *  源自 `sys/tree.h`，供 dpapp 内有序容器使用。 */

// clang-format off

#ifndef _SYS_TREE_H_
#define _SYS_TREE_H_

#include <stddef.h>
#include <stdint.h>

/*
 * 本文件定义了多种树的数据结构：splay 树和 rank-balanced 树。
 *
 * splay 树是一种自组织数据结构。每次操作都会触发一次 splay，
 * 将请求节点移动到树根并部分重新平衡。
 *
 * 其好处在于请求局部性使查找更快——被请求的节点移向树顶。
 * 但每次查找都会产生内存写操作。
 *
 * 平衡定理：对初始空树，m 次操作和 n 次插入的总访问时间
 * 上界为 O((m + n)lg n)。splay 树 m 次访问的摊销成本为 O(lg n)。
 *
 * rank-balanced 树是一种二叉搜索树，以整数 rank-difference
 * 作为从父到子指针的属性。任意节点到 null 路径上的
 * rank-difference 之和相同，定义该节点的 rank。null 节点的 rank 为 -1。
 *
 * 不同的附加条件定义不同类型的平衡树，包括"红黑"和"AVL"树。
 * 此处应用的条件是 Haeupler, Sen 和 Tarjan 在 ACM Transactions on
 * Algorithms Volume 11 Issue 4 June 2015 中提出的"weak-AVL"条件：
 *    - 每个 rank-difference 为 1 或 2。
 *    - 任何叶子的 rank 为 1。
 *
 * 由于历史原因，偶数值 rank-difference 与红色（Rank-Even-Difference）
 * 关联，红色边指向的子节点称为红色子节点。
 *
 * rank-balanced 树的每次操作上界为 O(lg n)。
 * 树的最大高度为 2lg (n+1)。
 */

#define SPLAY_HEAD(name, type)                                                      \
    struct name                                                                     \
    {                                                                               \
        struct type* sph_root; /* 树根 */                               \
    }

#define SPLAY_INITIALIZER(root)                                                     \
    {                                                                               \
        NULL                                                                        \
    }

#define SPLAY_INIT(root)                                                            \
    do {                                                                            \
        (root)->sph_root = NULL;                                                    \
    } while (/*CONSTCOND*/ 0)

#define SPLAY_ENTRY(type)                                                           \
    struct                                                                          \
    {                                                                               \
        struct type* spe_left;  /* 左子节点 */                                   \
        struct type* spe_right; /* 右子节点 */                                 \
    }

#define SPLAY_LEFT(elm, field)  (elm)->field.spe_left
#define SPLAY_RIGHT(elm, field) (elm)->field.spe_right
#define SPLAY_ROOT(head)        (head)->sph_root
#define SPLAY_EMPTY(head)       (SPLAY_ROOT(head) == NULL)

/* SPLAY_ROTATE_{LEFT,RIGHT} 假设 tmp 持有 SPLAY_{RIGHT,LEFT} */
#define SPLAY_ROTATE_RIGHT(head, tmp, field)                                        \
    do {                                                                            \
        SPLAY_LEFT((head)->sph_root, field) = SPLAY_RIGHT(tmp, field);              \
        SPLAY_RIGHT(tmp, field) = (head)->sph_root;                                 \
        (head)->sph_root = tmp;                                                     \
    } while (/*CONSTCOND*/ 0)

#define SPLAY_ROTATE_LEFT(head, tmp, field)                                         \
    do {                                                                            \
        SPLAY_RIGHT((head)->sph_root, field) = SPLAY_LEFT(tmp, field);              \
        SPLAY_LEFT(tmp, field) = (head)->sph_root;                                  \
        (head)->sph_root = tmp;                                                     \
    } while (/*CONSTCOND*/ 0)

#define SPLAY_LINKLEFT(head, tmp, field)                                            \
    do {                                                                            \
        SPLAY_LEFT(tmp, field) = (head)->sph_root;                                  \
        tmp = (head)->sph_root;                                                     \
        (head)->sph_root = SPLAY_LEFT((head)->sph_root, field);                     \
    } while (/*CONSTCOND*/ 0)

#define SPLAY_LINKRIGHT(head, tmp, field)                                           \
    do {                                                                            \
        SPLAY_RIGHT(tmp, field) = (head)->sph_root;                                 \
        tmp = (head)->sph_root;                                                     \
        (head)->sph_root = SPLAY_RIGHT((head)->sph_root, field);                    \
    } while (/*CONSTCOND*/ 0)

#define SPLAY_ASSEMBLE(head, node, left, right, field)                              \
    do {                                                                            \
        SPLAY_RIGHT(left, field) = SPLAY_LEFT((head)->sph_root, field);             \
        SPLAY_LEFT(right, field) = SPLAY_RIGHT((head)->sph_root, field);            \
        SPLAY_LEFT((head)->sph_root, field) = SPLAY_RIGHT(node, field);             \
        SPLAY_RIGHT((head)->sph_root, field) = SPLAY_LEFT(node, field);             \
    } while (/*CONSTCOND*/ 0)

/* 生成原型和内联函数 */

#define SPLAY_PROTOTYPE(name, type, field, cmp)                                     \
    void name##_SPLAY(struct name*, struct type*);                                  \
    void name##_SPLAY_MINMAX(struct name*, int);                                    \
    struct type* name##_SPLAY_INSERT(struct name*, struct type*);                   \
    struct type* name##_SPLAY_REMOVE(struct name*, struct type*);                   \
                                                                                    \
    /* 查找与 elm 键值相同的节点 */                                   \
    static __unused __inline struct type* name##_SPLAY_FIND(struct name* head,      \
        struct type* elm)                                                           \
    {                                                                               \
        if (SPLAY_EMPTY(head))                                                      \
            return (NULL);                                                          \
        name##_SPLAY(head, elm);                                                    \
        if ((cmp)(elm, (head)->sph_root) == 0)                                      \
            return (head->sph_root);                                                \
        return (NULL);                                                              \
    }                                                                               \
                                                                                    \
    static __unused __inline struct type* name##_SPLAY_NEXT(struct name* head,      \
        struct type* elm)                                                           \
    {                                                                               \
        name##_SPLAY(head, elm);                                                    \
        if (SPLAY_RIGHT(elm, field) != NULL) {                                      \
            elm = SPLAY_RIGHT(elm, field);                                          \
            while (SPLAY_LEFT(elm, field) != NULL) {                                \
                elm = SPLAY_LEFT(elm, field);                                       \
            }                                                                       \
        } else                                                                      \
            elm = NULL;                                                             \
        return (elm);                                                               \
    }                                                                               \
                                                                                    \
    static __unused __inline struct type* name##_SPLAY_MIN_MAX(struct name* head,   \
        int val)                                                                    \
    {                                                                               \
        name##_SPLAY_MINMAX(head, val);                                             \
        return (SPLAY_ROOT(head));                                                  \
    }

/* 主 splay 操作。
 * 将与 elm 键值接近的节点移到顶部
 */
#define SPLAY_GENERATE(name, type, field, cmp)                                      \
    struct type* name##_SPLAY_INSERT(struct name* head, struct type* elm)           \
    {                                                                               \
        if (SPLAY_EMPTY(head)) {                                                    \
            SPLAY_LEFT(elm, field) = SPLAY_RIGHT(elm, field) = NULL;                \
        } else {                                                                    \
            __typeof(cmp(NULL, NULL)) __comp;                                       \
            name##_SPLAY(head, elm);                                                \
            __comp = (cmp)(elm, (head)->sph_root);                                  \
            if (__comp < 0) {                                                       \
                SPLAY_LEFT(elm, field) = SPLAY_LEFT((head)->sph_root, field);       \
                SPLAY_RIGHT(elm, field) = (head)->sph_root;                         \
                SPLAY_LEFT((head)->sph_root, field) = NULL;                         \
            } else if (__comp > 0) {                                                \
                SPLAY_RIGHT(elm, field) = SPLAY_RIGHT((head)->sph_root, field);     \
                SPLAY_LEFT(elm, field) = (head)->sph_root;                          \
                SPLAY_RIGHT((head)->sph_root, field) = NULL;                        \
            } else                                                                  \
                return ((head)->sph_root);                                          \
        }                                                                           \
        (head)->sph_root = (elm);                                                   \
        return (NULL);                                                              \
    }                                                                               \
                                                                                    \
    struct type* name##_SPLAY_REMOVE(struct name* head, struct type* elm)           \
    {                                                                               \
        struct type* __tmp;                                                         \
        if (SPLAY_EMPTY(head))                                                      \
            return (NULL);                                                          \
        name##_SPLAY(head, elm);                                                    \
        if ((cmp)(elm, (head)->sph_root) == 0) {                                    \
            if (SPLAY_LEFT((head)->sph_root, field) == NULL) {                      \
                (head)->sph_root = SPLAY_RIGHT((head)->sph_root, field);            \
            } else {                                                                \
                __tmp = SPLAY_RIGHT((head)->sph_root, field);                       \
                (head)->sph_root = SPLAY_LEFT((head)->sph_root, field);             \
                name##_SPLAY(head, elm);                                            \
                SPLAY_RIGHT((head)->sph_root, field) = __tmp;                       \
            }                                                                       \
            return (elm);                                                           \
        }                                                                           \
        return (NULL);                                                              \
    }                                                                               \
                                                                                    \
    void name##_SPLAY(struct name* head, struct type* elm)                          \
    {                                                                               \
        struct type __node, *__left, *__right, *__tmp;                              \
        __typeof(cmp(NULL, NULL)) __comp;                                           \
                                                                                    \
        SPLAY_LEFT(&__node, field) = SPLAY_RIGHT(&__node, field) = NULL;            \
        __left = __right = &__node;                                                 \
                                                                                    \
        while ((__comp = (cmp)(elm, (head)->sph_root)) != 0) {                      \
            if (__comp < 0) {                                                       \
                __tmp = SPLAY_LEFT((head)->sph_root, field);                        \
                if (__tmp == NULL)                                                  \
                    break;                                                          \
                if ((cmp)(elm, __tmp) < 0) {                                        \
                    SPLAY_ROTATE_RIGHT(head, __tmp, field);                         \
                    if (SPLAY_LEFT((head)->sph_root, field) == NULL)                \
                        break;                                                      \
                }                                                                   \
                SPLAY_LINKLEFT(head, __right, field);                               \
            } else if (__comp > 0) {                                                \
                __tmp = SPLAY_RIGHT((head)->sph_root, field);                       \
                if (__tmp == NULL)                                                  \
                    break;                                                          \
                if ((cmp)(elm, __tmp) > 0) {                                        \
                    SPLAY_ROTATE_LEFT(head, __tmp, field);                          \
                    if (SPLAY_RIGHT((head)->sph_root, field) == NULL)               \
                        break;                                                      \
                }                                                                   \
                SPLAY_LINKRIGHT(head, __left, field);                               \
            }                                                                       \
        }                                                                           \
        SPLAY_ASSEMBLE(head, &__node, __left, __right, field);                      \
    }                                                                               \
                                                                                    \
    /* 以最小或最大元素进行 splay
     * 用于查找树中的最小或最大元素。
     */                                                                             \
    void name##_SPLAY_MINMAX(struct name* head, int __comp)                         \
    {                                                                               \
        struct type __node, *__left, *__right, *__tmp;                              \
                                                                                    \
        SPLAY_LEFT(&__node, field) = SPLAY_RIGHT(&__node, field) = NULL;            \
        __left = __right = &__node;                                                 \
                                                                                    \
        while (1) {                                                                 \
            if (__comp < 0) {                                                       \
                __tmp = SPLAY_LEFT((head)->sph_root, field);                        \
                if (__tmp == NULL)                                                  \
                    break;                                                          \
                if (__comp < 0) {                                                   \
                    SPLAY_ROTATE_RIGHT(head, __tmp, field);                         \
                    if (SPLAY_LEFT((head)->sph_root, field) == NULL)                \
                        break;                                                      \
                }                                                                   \
                SPLAY_LINKLEFT(head, __right, field);                               \
            } else if (__comp > 0) {                                                \
                __tmp = SPLAY_RIGHT((head)->sph_root, field);                       \
                if (__tmp == NULL)                                                  \
                    break;                                                          \
                if (__comp > 0) {                                                   \
                    SPLAY_ROTATE_LEFT(head, __tmp, field);                          \
                    if (SPLAY_RIGHT((head)->sph_root, field) == NULL)               \
                        break;                                                      \
                }                                                                   \
                SPLAY_LINKRIGHT(head, __left, field);                               \
            }                                                                       \
        }                                                                           \
        SPLAY_ASSEMBLE(head, &__node, __left, __right, field);                      \
    }

#define SPLAY_NEGINF -1
#define SPLAY_INF    1

#define SPLAY_INSERT(name, x, y) name##_SPLAY_INSERT(x, y)
#define SPLAY_REMOVE(name, x, y) name##_SPLAY_REMOVE(x, y)
#define SPLAY_FIND(name, x, y)   name##_SPLAY_FIND(x, y)
#define SPLAY_NEXT(name, x, y)   name##_SPLAY_NEXT(x, y)
#define SPLAY_MIN(name, x)                                                          \
    (SPLAY_EMPTY(x) ? NULL : name##_SPLAY_MIN_MAX(x, SPLAY_NEGINF))
#define SPLAY_MAX(name, x)                                                          \
    (SPLAY_EMPTY(x) ? NULL : name##_SPLAY_MIN_MAX(x, SPLAY_INF))

#define SPLAY_FOREACH(x, name, head)                                                \
    for ((x) = SPLAY_MIN(name, head); (x) != NULL; (x) = SPLAY_NEXT(name, head, x))

/* 定义 rank-balanced 树的宏 */
#define RB_HEAD(name, type)                                                         \
    struct name                                                                     \
    {                                                                               \
        struct type* rbh_root; /* 树根 */                               \
    }

#define RB_INITIALIZER(root)                                                        \
    {                                                                               \
        NULL                                                                        \
    }

#define RB_INIT(root)                                                               \
    do {                                                                            \
        (root)->rbh_root = NULL;                                                    \
    } while (/*CONSTCOND*/ 0)

#define RB_ENTRY(type)                                                              \
    struct                                                                          \
    {                                                                               \
        struct type* rbe_link[3];                                                   \
    }

/*
 * 期望 struct type 的对象地址是 4 的倍数，因此指向该类型的
 * 指针最低 2 位始终为 0。本实现利用这些位来标记树节点
 * 的左子节点或右子节点为"红色"。
 */
#define _RB_LINK(elm, dir, field, op) (elm)->field op rbe_link[dir]
#define _RB_UP(elm, field, op)        _RB_LINK(elm, 0, field, op)
#define _RB_L                         ((uintptr_t)1)
#define _RB_R                         ((uintptr_t)2)
#define _RB_LR                        ((uintptr_t)3)
#define _RB_BITS(elm)                 (*(uintptr_t*)&elm)
#define _RB_BITSUP(elm, field, op)    _RB_BITS(_RB_UP(elm, field, op))
#define _RB_PTR(elm)                  (__typeof(elm))((uintptr_t)elm & ~_RB_LR)

#define RB_PARENT(elm, field, op) _RB_PTR(_RB_UP(elm, field, op))
#define RB_LEFT(elm, field, op)   _RB_LINK(elm, _RB_L, field, op)
#define RB_RIGHT(elm, field, op)  _RB_LINK(elm, _RB_R, field, op)
#define RB_ROOT(head)             (head)->rbh_root
#define RB_EMPTY(head)            (RB_ROOT(head) == NULL)

#define RB_SET_PARENT(dst, src, field, op)                                          \
    do {                                                                            \
        _RB_BITSUP(dst, field, op) = (uintptr_t)src                                 \
            | (_RB_BITSUP(dst, field, op) & _RB_LR);                                \
    } while (/*CONSTCOND*/ 0)

#define RB_SET(elm, parent, field, op)                                              \
    do {                                                                            \
        _RB_UP(elm, field, op) = parent;                                            \
        RB_LEFT(elm, field, op) = RB_RIGHT(elm, field, op) = NULL;                  \
    } while (/*CONSTCOND*/ 0)

/*
 * RB_AUGMENT 或 RB_AUGMENT_CHECK 在每个被修改子树的根部以循环方式调用，
 * 从底向上直到根，用于更新增强节点数据。RB_AUGMENT_CHECK 仅在更新
 * 改变了节点数据时返回 true，以便在返回 false 时提前停止更新。
 */
#ifndef RB_AUGMENT_CHECK
#ifndef RB_AUGMENT
#define RB_AUGMENT_CHECK(x) 0
#else
#define RB_AUGMENT_CHECK(x) (RB_AUGMENT(x), 1)
#endif
#endif

#define RB_UPDATE_AUGMENT(elm, field, op)                                           \
    do {                                                                            \
        __typeof(elm) rb_update_tmp = (elm);                                        \
        while (RB_AUGMENT_CHECK(rb_update_tmp)                                      \
            && (rb_update_tmp = RB_PARENT(rb_update_tmp, field, op)) != NULL)       \
            ;                                                                       \
    } while (0)

#define RB_SWAP_CHILD(head, par, out, in, field, op)                                \
    do {                                                                            \
        if (par == NULL)                                                            \
            RB_ROOT(head) = (in);                                                   \
        else if ((out) == RB_LEFT(par, field, op))                                  \
            RB_LEFT(par, field, op) = (in);                                         \
        else                                                                        \
            RB_RIGHT(par, field, op) = (in);                                        \
    } while (/*CONSTCOND*/ 0)

/*
 * RB_ROTATE 宏部分重构树以改善平衡。当 dir 为 _RB_L 时，tmp 是 elm 的右子节点。
 * 旋转后，elm 成为 tmp 的左子节点，原本位于它们之间的子树（原挂载在 tmp 左侧）
 * 现在挂载在 elm 右侧。elm 与其原父节点的父子关系不变；此处宏不再更新那些字段，
 * 留给 RB_ROTATE 的调用者处理，以避免一对旋转操作两次以不同值更新同一对指针字段。
 */
#define RB_ROTATE(elm, tmp, dir, field, op)                                         \
    do {                                                                            \
        if ((_RB_LINK(elm, dir ^ _RB_LR, field, op) = _RB_LINK(tmp, dir, field,     \
                 op))                                                               \
            != NULL)                                                                \
            RB_SET_PARENT(_RB_LINK(tmp, dir, field, op), elm, field, op);           \
        _RB_LINK(tmp, dir, field, op) = (elm);                                      \
        RB_SET_PARENT(elm, tmp, field, op);                                         \
    } while (/*CONSTCOND*/ 0)

/* 生成原型和内联函数 */
#define RB_PROTOTYPE(name, type, field, cmp, op)                                    \
    RB_PROTOTYPE_INTERNAL(name, type, field, cmp, op, )
#define RB_PROTOTYPE_STATIC(name, type, field, cmp, op)                             \
    RB_PROTOTYPE_INTERNAL(name, type, field, cmp, op, __unused static)
#define RB_PROTOTYPE_INTERNAL(name, type, field, cmp, op, attr)                     \
    RB_PROTOTYPE_RANK(name, type, attr)                                             \
    RB_PROTOTYPE_INSERT_COLOR(name, type, attr);                                    \
    RB_PROTOTYPE_REMOVE_COLOR(name, type, attr);                                    \
    RB_PROTOTYPE_INSERT_FINISH(name, type, attr);                                   \
    RB_PROTOTYPE_INSERT(name, type, attr);                                          \
    RB_PROTOTYPE_REMOVE(name, type, attr);                                          \
    RB_PROTOTYPE_FIND(name, type, attr);                                            \
    RB_PROTOTYPE_NFIND(name, type, attr);                                           \
    RB_PROTOTYPE_NEXT(name, type, attr);                                            \
    RB_PROTOTYPE_INSERT_NEXT(name, type, attr);                                     \
    RB_PROTOTYPE_PREV(name, type, attr);                                            \
    RB_PROTOTYPE_INSERT_PREV(name, type, attr);                                     \
    RB_PROTOTYPE_MINMAX(name, type, attr);                                          \
    RB_PROTOTYPE_REINSERT(name, type, attr);
#ifdef _RB_DIAGNOSTIC
#define RB_PROTOTYPE_RANK(name, type, attr) attr int name##_RB_RANK(struct type*);
#else
#define RB_PROTOTYPE_RANK(name, type, attr)
#endif
#define RB_PROTOTYPE_INSERT_COLOR(name, type, attr)                                 \
    attr struct type* name##_RB_INSERT_COLOR(struct name*, struct type*,            \
        struct type*)
#define RB_PROTOTYPE_REMOVE_COLOR(name, type, attr)                                 \
    attr struct type* name##_RB_REMOVE_COLOR(struct name*, struct type*,            \
        struct type*)
#define RB_PROTOTYPE_REMOVE(name, type, attr)                                       \
    attr struct type* name##_RB_REMOVE(struct name*, struct type*)
#define RB_PROTOTYPE_INSERT_FINISH(name, type, attr)                                \
    attr struct type* name##_RB_INSERT_FINISH(struct name*, struct type*,           \
        struct type**, struct type*)
#define RB_PROTOTYPE_INSERT(name, type, attr)                                       \
    attr struct type* name##_RB_INSERT(struct name*, struct type*)
#define RB_PROTOTYPE_FIND(name, type, attr)                                         \
    attr struct type* name##_RB_FIND(struct name*, struct type*)
#define RB_PROTOTYPE_NFIND(name, type, attr)                                        \
    attr struct type* name##_RB_NFIND(struct name*, struct type*)
#define RB_PROTOTYPE_NEXT(name, type, attr)                                         \
    attr struct type* name##_RB_NEXT(struct type*)
#define RB_PROTOTYPE_INSERT_NEXT(name, type, attr)                                  \
    attr struct type* name##_RB_INSERT_NEXT(struct name*, struct type*, struct type*)
#define RB_PROTOTYPE_PREV(name, type, attr)                                         \
    attr struct type* name##_RB_PREV(struct type*)
#define RB_PROTOTYPE_INSERT_PREV(name, type, attr)                                  \
    attr struct type* name##_RB_INSERT_PREV(struct name*, struct type*, struct type*)
#define RB_PROTOTYPE_MINMAX(name, type, attr)                                       \
    attr struct type* name##_RB_MINMAX(struct name*, int)
#define RB_PROTOTYPE_REINSERT(name, type, attr)                                     \
    attr struct type* name##_RB_REINSERT(struct name*, struct type*)

/* 主 RB 操作。
 * 将与 elm 键值接近的节点移到顶部
 */
#define RB_GENERATE(name, type, field, cmp, op)                                     \
    RB_GENERATE_INTERNAL(name, type, field, cmp, op, )
#define RB_GENERATE_STATIC(name, type, field, cmp, op)                              \
    RB_GENERATE_INTERNAL(name, type, field, cmp, op, __unused static)
#define RB_GENERATE_INTERNAL(name, type, field, cmp, op, attr)                      \
    RB_GENERATE_RANK(name, type, field, op, attr)                                   \
    RB_GENERATE_INSERT_COLOR(name, type, field, op, attr)                           \
    RB_GENERATE_REMOVE_COLOR(name, type, field, op, attr)                           \
    RB_GENERATE_INSERT_FINISH(name, type, field, op, attr)                          \
    RB_GENERATE_INSERT(name, type, field, cmp, op, attr)                            \
    RB_GENERATE_REMOVE(name, type, field, op, attr)                                 \
    RB_GENERATE_FIND(name, type, field, cmp, op, attr)                              \
    RB_GENERATE_NFIND(name, type, field, cmp, op, attr)                             \
    RB_GENERATE_NEXT(name, type, field, op, attr)                                   \
    RB_GENERATE_INSERT_NEXT(name, type, field, cmp, op, attr)                       \
    RB_GENERATE_PREV(name, type, field, op, attr)                                   \
    RB_GENERATE_INSERT_PREV(name, type, field, cmp, op, attr)                       \
    RB_GENERATE_MINMAX(name, type, field, op, attr)                                 \
    RB_GENERATE_REINSERT(name, type, field, cmp, op, attr)

#ifdef _RB_DIAGNOSTIC
#ifndef RB_AUGMENT
#define _RB_AUGMENT_VERIFY(x) RB_AUGMENT_CHECK(x)
#else
#define _RB_AUGMENT_VERIFY(x) 0
#endif
#define RB_GENERATE_RANK(name, type, field, op, attr)                               \
    /*                                                                              \
     * 返回以 elm 为根的子树 rank，如果子树不平衡或                                      \
     * 增强数据不一致则返回 -1。                                                         \
     */                                                                             \
    attr int name##_RB_RANK(struct type* elm)                                       \
    {                                                                               \
        struct type *left, *right, *up;                                             \
        int left_rank, right_rank;                                                  \
                                                                                    \
        if (elm == NULL)                                                            \
            return (0);                                                             \
        up = _RB_UP(elm, field, op);                                                \
        left = RB_LEFT(elm, field, op);                                             \
        left_rank = ((_RB_BITS(up) & _RB_L) ? 2 : 1) + name##_RB_RANK(left);        \
        right = RB_RIGHT(elm, field, op);                                           \
        right_rank = ((_RB_BITS(up) & _RB_R) ? 2 : 1) + name##_RB_RANK(right);      \
        if (left_rank != right_rank                                                 \
            || (left_rank == 2 && left == NULL && right == NULL)                    \
            || _RB_AUGMENT_VERIFY(elm))                                             \
            return (-1);                                                            \
        return (left_rank);                                                         \
    }
#else
#define RB_GENERATE_RANK(name, type, field, op, attr)
#endif

#define RB_GENERATE_INSERT_COLOR(name, type, field, op, attr)                       \
    attr struct type* name##_RB_INSERT_COLOR(struct name* head,                     \
        struct type* parent, struct type* elm)                                      \
    {                                                                               \
        /*                                                                          \
         * 初始时，elm 是叶子。其父节点之前可能是叶子（有两个黑色 null 子节点），          \
         * 或者是有一个黑色非 null 子节点和一个红色 null 子节点的内部节点。                  \
         * 平衡准则"任何叶子的 rank 为 1"排除了初始父节点有两个红色 null 子节点的可能性。       \
         * 因此第一次循环迭代不会访问未初始化的 'child'，后续迭代仅在前一次                         \
         * 已为 'child' 赋值时才会发生。                                                         \
         */                                                                         \
        struct type *child = NULL, *child_up = NULL, *gpar = NULL;                  \
        uintptr_t elmdir, sibdir;                                                   \
                                                                                    \
        do {                                                                        \
            /* 以 elm 为根的树 rank 增长了 */                           \
            gpar = _RB_UP(parent, field, op);                                       \
            elmdir = RB_RIGHT(parent, field, op) == elm ? _RB_R : _RB_L;            \
            if (_RB_BITS(gpar) & elmdir) {                                          \
                /* 缩短 parent-elm 边以重新平衡 */                      \
                _RB_BITSUP(parent, field, op) ^= elmdir;                            \
                return (NULL);                                                      \
            }                                                                       \
            sibdir = elmdir ^ _RB_LR;                                               \
            /* 另一条边必须改变长度 */                                 \
            _RB_BITSUP(parent, field, op) ^= sibdir;                                \
            if ((_RB_BITS(gpar) & _RB_LR) == 0) {                                   \
                /* 两条边都变短了，从 parent 重试 */                       \
                child = elm;                                                        \
                elm = parent;                                                       \
                continue;                                                           \
            }                                                                       \
            _RB_UP(parent, field, op) = gpar = _RB_PTR(gpar);                       \
            if (_RB_BITSUP(elm, field, op) & elmdir) {                              \
                RB_ROTATE(elm, child, elmdir, field, op);                           \
                child_up = _RB_UP(child, field, op);                                \
                if (_RB_BITS(child_up) & sibdir)                                    \
                    _RB_BITSUP(parent, field, op) ^= elmdir;                        \
                if (_RB_BITS(child_up) & elmdir)                                    \
                    _RB_BITSUP(elm, field, op) ^= _RB_LR;                           \
                else                                                                \
                    _RB_BITSUP(elm, field, op) ^= elmdir;                           \
                /* child 是叶子则不更新 elm 的增强数据，                         \
                 * 因为之后它会被恢复为叶子。 */                                     \
                if ((_RB_BITS(child_up) & _RB_LR) == 0)                             \
                    elm = child;                                                    \
            } else                                                                  \
                child = elm;                                                        \
                                                                                    \
            RB_ROTATE(parent, child, sibdir, field, op);                            \
            _RB_UP(child, field, op) = gpar;                                        \
            RB_SWAP_CHILD(head, gpar, parent, child, field, op);                    \
            /*                                                                      \
             * 向下旋转的元素获得了新的、更小的子树，                                    \
             * 因此更新它们的增强数据。                                                   \
             */                                                                     \
            if (elm != child)                                                       \
                (void)RB_AUGMENT_CHECK(elm);                                        \
            (void)RB_AUGMENT_CHECK(parent);                                         \
            return (child);                                                         \
        } while ((parent = gpar) != NULL);                                          \
        return (NULL);                                                              \
    }

#ifndef RB_STRICT_HST
/*
 * 在 REMOVE_COLOR 中，HST 论文图 3 的单旋转情况下，'parent' 的 rank 高 1，
 * 如果 'parent' 变成叶子则降低其 rank。本实现始终让 parent 在新位置具有
 * 较低的 rank，以避免叶子检查。定义 RB_STRICT_HST 为 1 可获取 HST 描述的行为。
 */
#define RB_STRICT_HST 0
#endif

#define RB_GENERATE_REMOVE_COLOR(name, type, field, op, attr)                       \
    attr struct type* name##_RB_REMOVE_COLOR(struct name* head,                     \
        struct type* parent, struct type* elm)                                      \
    {                                                                               \
        struct type *gpar, *sib, *up;                                               \
        uintptr_t elmdir, sibdir;                                                   \
                                                                                    \
        if (RB_RIGHT(parent, field, op) == elm                                      \
            && RB_LEFT(parent, field, op) == elm) {                                 \
            /* 删除作为独子节点的叶子会创建一个                                      \
             * rank-2 叶子。降低该叶子的 rank。 */                                    \
            _RB_UP(parent, field, op) = _RB_PTR(_RB_UP(parent, field, op));         \
            elm = parent;                                                           \
            if ((parent = _RB_UP(elm, field, op)) == NULL)                          \
                return (NULL);                                                      \
        }                                                                           \
        do {                                                                        \
            /* 以 elm 为根的树 rank 缩小了 */                         \
            gpar = _RB_UP(parent, field, op);                                       \
            elmdir = RB_RIGHT(parent, field, op) == elm ? _RB_R : _RB_L;            \
            _RB_BITS(gpar) ^= elmdir;                                               \
            if (_RB_BITS(gpar) & elmdir) {                                          \
                /* 延长 parent-elm 边以重新平衡 */                     \
                _RB_UP(parent, field, op) = gpar;                                   \
                return (NULL);                                                      \
            }                                                                       \
            if (_RB_BITS(gpar) & _RB_LR) {                                          \
                /* 缩短另一条边，从 parent 重试 */                         \
                _RB_BITS(gpar) ^= _RB_LR;                                           \
                _RB_UP(parent, field, op) = gpar;                                   \
                gpar = _RB_PTR(gpar);                                               \
                continue;                                                           \
            }                                                                       \
            sibdir = elmdir ^ _RB_LR;                                               \
            sib = _RB_LINK(parent, sibdir, field, op);                              \
            up = _RB_UP(sib, field, op);                                            \
            _RB_BITS(up) ^= _RB_LR;                                                 \
            if ((_RB_BITS(up) & _RB_LR) == 0) {                                     \
                /* 缩短从 sib 延伸的边，重试 */                      \
                _RB_UP(sib, field, op) = up;                                        \
                continue;                                                           \
            }                                                                       \
            if ((_RB_BITS(up) & sibdir) == 0) {                                     \
                elm = _RB_LINK(sib, elmdir, field, op);                             \
                /* elm 是 1-child，先对 elm 旋转。 */                       \
                RB_ROTATE(sib, elm, sibdir, field, op);                             \
                up = _RB_UP(elm, field, op);                                        \
                _RB_BITSUP(parent, field, op) ^= (_RB_BITS(up) & elmdir) ? _RB_LR   \
                                                                         : elmdir;  \
                _RB_BITSUP(sib, field, op) ^= (_RB_BITS(up) & sibdir) ? _RB_LR      \
                                                                      : sibdir;     \
                _RB_BITSUP(elm, field, op) |= _RB_LR;                               \
            } else {                                                                \
                if ((_RB_BITS(up) & elmdir) == 0 && RB_STRICT_HST && elm != NULL) { \
                    /* 如果 parent 不会变成叶子，                                    \
                       暂时不降低 parent 的 rank。 */                                 \
                    _RB_BITSUP(parent, field, op) ^= sibdir;                        \
                    _RB_BITSUP(sib, field, op) ^= _RB_LR;                           \
                } else if ((_RB_BITS(up) & elmdir) == 0) {                          \
                    /* 降低 parent 的 rank。 */                                            \
                    _RB_BITSUP(parent, field, op) ^= elmdir;                        \
                    _RB_BITSUP(sib, field, op) ^= sibdir;                           \
                } else                                                              \
                    _RB_BITSUP(sib, field, op) ^= sibdir;                           \
                elm = sib;                                                          \
            }                                                                       \
                                                                                    \
            RB_ROTATE(parent, elm, elmdir, field, op);                              \
            RB_SET_PARENT(elm, gpar, field, op);                                    \
            RB_SWAP_CHILD(head, gpar, parent, elm, field, op);                      \
            /*                                                                      \
             * An element rotated down, but not into the search                     \
             * path has a new, smaller subtree, so update                           \
             * augmentation for it.                                                 \
             */                                                                     \
            if (sib != elm)                                                         \
                (void)RB_AUGMENT_CHECK(sib);                                        \
            return (parent);                                                        \
        } while (elm = parent, (parent = gpar) != NULL);                            \
        return (NULL);                                                              \
    }

#define _RB_AUGMENT_WALK(elm, match, field, op)                                     \
    do {                                                                            \
        if (match == elm)                                                           \
            match = NULL;                                                           \
    } while (RB_AUGMENT_CHECK(elm) && (elm = RB_PARENT(elm, field, op)) != NULL)

#define RB_GENERATE_REMOVE(name, type, field, op, attr)                             \
    attr struct type* name##_RB_REMOVE(struct name* head, struct type* out)         \
    {                                                                               \
        struct type *child, *in, *opar, *parent;                                    \
                                                                                    \
        child = RB_LEFT(out, field, op);                                            \
        in = RB_RIGHT(out, field, op);                                              \
        opar = _RB_UP(out, field, op);                                              \
        if (in == NULL || child == NULL) {                                          \
            in = child = (in == NULL ? child : in);                                 \
            parent = opar = _RB_PTR(opar);                                          \
        } else {                                                                    \
            parent = in;                                                            \
            while (RB_LEFT(in, field, op))                                          \
                in = RB_LEFT(in, field, op);                                        \
            RB_SET_PARENT(child, in, field, op);                                    \
            RB_LEFT(in, field, op) = child;                                         \
            child = RB_RIGHT(in, field, op);                                        \
            if (parent != in) {                                                     \
                RB_SET_PARENT(parent, in, field, op);                               \
                RB_RIGHT(in, field, op) = parent;                                   \
                parent = RB_PARENT(in, field, op);                                  \
                RB_LEFT(parent, field, op) = child;                                 \
            }                                                                       \
            _RB_UP(in, field, op) = opar;                                           \
            opar = _RB_PTR(opar);                                                   \
        }                                                                           \
        RB_SWAP_CHILD(head, opar, out, in, field, op);                              \
        if (child != NULL)                                                          \
            _RB_UP(child, field, op) = parent;                                      \
        if (parent != NULL) {                                                       \
            opar = name##_RB_REMOVE_COLOR(head, parent, child);                     \
            /* 如果旋转使 'parent' 成为与原来相同子树的根，                           \
             * 不再更新其增强数据。 */                           \
            if (parent == in && RB_LEFT(parent, field, op) == NULL) {               \
                opar = NULL;                                                        \
                parent = RB_PARENT(parent, field, op);                              \
            }                                                                       \
            _RB_AUGMENT_WALK(parent, opar, field, op);                              \
            if (opar != NULL) {                                                     \
                /*                                                                  \
                 * 旋转到搜索路径中的元素子树已变化，                                    \
                 * 如果 AUGMENT_WALK 未更新则更新增强数据。                               \
                 */                                                                 \
                (void)RB_AUGMENT_CHECK(opar);                                       \
                (void)RB_AUGMENT_CHECK(RB_PARENT(opar, field, op));                 \
            }                                                                       \
        }                                                                           \
        return (out);                                                               \
    }

#define RB_GENERATE_INSERT_FINISH(name, type, field, op, attr)                      \
    /* 插入节点到 RB 树 */                                           \
    attr struct type* name##_RB_INSERT_FINISH(struct name* head,                    \
        struct type* parent, struct type** pptr, struct type* elm)                  \
    {                                                                               \
        struct type* tmp = NULL;                                                    \
                                                                                    \
        RB_SET(elm, parent, field, op);                                             \
        *pptr = elm;                                                                \
        if (parent != NULL)                                                         \
            tmp = name##_RB_INSERT_COLOR(head, parent, elm);                        \
        _RB_AUGMENT_WALK(elm, tmp, field, op);                                      \
        if (tmp != NULL)                                                            \
            /*                                                                      \
             * An element rotated into the search path has a                        \
             * changed subtree, so update augmentation for it if                    \
             * AUGMENT_WALK didn't.                                                 \
             */                                                                     \
            (void)RB_AUGMENT_CHECK(tmp);                                            \
        return (NULL);                                                              \
    }

#define RB_GENERATE_INSERT(name, type, field, cmp, op, attr)                        \
    /* 插入节点到 RB 树 */                                           \
    attr struct type* name##_RB_INSERT(struct name* head, struct type* elm)         \
    {                                                                               \
        struct type* tmp;                                                           \
        struct type** tmpp = &RB_ROOT(head);                                        \
        struct type* parent = NULL;                                                 \
                                                                                    \
        while ((tmp = *tmpp) != NULL) {                                             \
            parent = tmp;                                                           \
            __typeof(cmp(NULL, NULL)) comp = (cmp)(elm, parent);                    \
            if (comp < 0)                                                           \
                tmpp = &RB_LEFT(parent, field, op);                                 \
            else if (comp > 0)                                                      \
                tmpp = &RB_RIGHT(parent, field, op);                                \
            else                                                                    \
                return (parent);                                                    \
        }                                                                           \
        return (name##_RB_INSERT_FINISH(head, parent, tmpp, elm));                  \
    }

#define RB_GENERATE_FIND(name, type, field, cmp, op, attr)                          \
    /* 查找与 elm 键值相同的节点 */                                   \
    attr struct type* name##_RB_FIND(struct name* head, struct type* elm)           \
    {                                                                               \
        struct type* tmp = RB_ROOT(head);                                           \
        __typeof(cmp(NULL, NULL)) comp;                                             \
        while (tmp) {                                                               \
            comp = cmp(elm, tmp);                                                   \
            if (comp < 0)                                                           \
                tmp = RB_LEFT(tmp, field, op);                                      \
            else if (comp > 0)                                                      \
                tmp = RB_RIGHT(tmp, field, op);                                     \
            else                                                                    \
                return (tmp);                                                       \
        }                                                                           \
        return (NULL);                                                              \
    }

#define RB_GENERATE_NFIND(name, type, field, cmp, op, attr)                         \
    /* 查找键值大于等于搜索键的第一个节点 */              \
    attr struct type* name##_RB_NFIND(struct name* head, struct type* elm)          \
    {                                                                               \
        struct type* tmp = RB_ROOT(head);                                           \
        struct type* res = NULL;                                                    \
        __typeof(cmp(NULL, NULL)) comp;                                             \
        while (tmp) {                                                               \
            comp = cmp(elm, tmp);                                                   \
            if (comp < 0) {                                                         \
                res = tmp;                                                          \
                tmp = RB_LEFT(tmp, field, op);                                      \
            } else if (comp > 0)                                                    \
                tmp = RB_RIGHT(tmp, field, op);                                     \
            else                                                                    \
                return (tmp);                                                       \
        }                                                                           \
        return (res);                                                               \
    }

#define RB_GENERATE_NEXT(name, type, field, op, attr)                               \
    /* ARGSUSED */                                                                  \
    attr struct type* name##_RB_NEXT(struct type* elm)                              \
    {                                                                               \
        if (RB_RIGHT(elm, field, op)) {                                             \
            elm = RB_RIGHT(elm, field, op);                                         \
            while (RB_LEFT(elm, field, op))                                         \
                elm = RB_LEFT(elm, field, op);                                      \
        } else {                                                                    \
            while (RB_PARENT(elm, field, op)                                        \
                && (elm == RB_RIGHT(RB_PARENT(elm, field, op), field, op)))         \
                elm = RB_PARENT(elm, field, op);                                    \
            elm = RB_PARENT(elm, field, op);                                        \
        }                                                                           \
        return (elm);                                                               \
    }

#if defined(_KERNEL) && defined(DIAGNOSTIC)
#define _RB_ORDER_CHECK(cmp, lo, hi)                                                \
    do {                                                                            \
        KASSERT((cmp)(lo, hi) < 0, ("out of order insertion"));                     \
    } while (0)
#else
#define _RB_ORDER_CHECK(cmp, lo, hi)                                                \
    do {                                                                            \
    } while (0)
#endif

#define RB_GENERATE_INSERT_NEXT(name, type, field, cmp, op, attr)                   \
    /* 在 RB 树的下一个位置插入节点 */                      \
    attr struct type* name##_RB_INSERT_NEXT(struct name* head, struct type* elm,    \
        struct type* next)                                                          \
    {                                                                               \
        struct type* tmp;                                                           \
        struct type** tmpp = &RB_RIGHT(elm, field, op);                             \
                                                                                    \
        _RB_ORDER_CHECK(cmp, elm, next);                                            \
        if (name##_RB_NEXT(elm) != NULL)                                            \
            _RB_ORDER_CHECK(cmp, next, name##_RB_NEXT(elm));                        \
        while ((tmp = *tmpp) != NULL) {                                             \
            elm = tmp;                                                              \
            tmpp = &RB_LEFT(elm, field, op);                                        \
        }                                                                           \
        return (name##_RB_INSERT_FINISH(head, elm, tmpp, next));                    \
    }

#define RB_GENERATE_PREV(name, type, field, op, attr)                               \
    /* ARGSUSED */                                                                  \
    attr struct type* name##_RB_PREV(struct type* elm)                              \
    {                                                                               \
        if (RB_LEFT(elm, field, op)) {                                              \
            elm = RB_LEFT(elm, field, op);                                          \
            while (RB_RIGHT(elm, field, op))                                        \
                elm = RB_RIGHT(elm, field, op);                                     \
        } else {                                                                    \
            while (RB_PARENT(elm, field, op)                                        \
                && (elm == RB_LEFT(RB_PARENT(elm, field, op), field, op)))          \
                elm = RB_PARENT(elm, field, op);                                    \
            elm = RB_PARENT(elm, field, op);                                        \
        }                                                                           \
        return (elm);                                                               \
    }

#define RB_GENERATE_INSERT_PREV(name, type, field, cmp, op, attr)                   \
    /* 在 RB 树的前一个位置插入节点 */                      \
    attr struct type* name##_RB_INSERT_PREV(struct name* head, struct type* elm,    \
        struct type* prev)                                                          \
    {                                                                               \
        struct type* tmp;                                                           \
        struct type** tmpp = &RB_LEFT(elm, field, op);                              \
                                                                                    \
        _RB_ORDER_CHECK(cmp, prev, elm);                                            \
        if (name##_RB_PREV(elm) != NULL)                                            \
            _RB_ORDER_CHECK(cmp, name##_RB_PREV(elm), prev);                        \
        while ((tmp = *tmpp) != NULL) {                                             \
            elm = tmp;                                                              \
            tmpp = &RB_RIGHT(elm, field, op);                                       \
        }                                                                           \
        return (name##_RB_INSERT_FINISH(head, elm, tmpp, prev));                    \
    }

#define RB_GENERATE_MINMAX(name, type, field, op, attr)                             \
    attr struct type* name##_RB_MINMAX(struct name* head, int val)                  \
    {                                                                               \
        struct type* tmp = RB_ROOT(head);                                           \
        struct type* parent = NULL;                                                 \
        while (tmp) {                                                               \
            parent = tmp;                                                           \
            if (val < 0)                                                            \
                tmp = RB_LEFT(tmp, field, op);                                      \
            else                                                                    \
                tmp = RB_RIGHT(tmp, field, op);                                     \
        }                                                                           \
        return (parent);                                                            \
    }

#define RB_GENERATE_REINSERT(name, type, field, cmp, op, attr)                      \
    attr struct type* name##_RB_REINSERT(struct name* head, struct type* elm)       \
    {                                                                               \
        struct type* cmpelm;                                                        \
        if (((cmpelm = RB_PREV(name, head, elm)) != NULL && cmp(cmpelm, elm) >= 0)  \
            || ((cmpelm = RB_NEXT(name, head, elm)) != NULL                         \
                && cmp(elm, cmpelm) >= 0)) {                                        \
            /* XXXLAS: Remove/insert 代价较高。 */                            \
            RB_REMOVE(name, head, elm);                                             \
            return (RB_INSERT(name, head, elm));                                    \
        }                                                                           \
        return (NULL);                                                              \
    }

#define RB_NEGINF -1
#define RB_INF    1

#define RB_INSERT(name, x, y)         name##_RB_INSERT(x, y)
#define RB_INSERT_NEXT(name, x, y, z) name##_RB_INSERT_NEXT(x, y, z)
#define RB_INSERT_PREV(name, x, y, z) name##_RB_INSERT_PREV(x, y, z)
#define RB_REMOVE(name, x, y)         name##_RB_REMOVE(x, y)
#define RB_FIND(name, x, y)           name##_RB_FIND(x, y)
#define RB_NFIND(name, x, y)          name##_RB_NFIND(x, y)
#define RB_NEXT(name, x, y)           name##_RB_NEXT(y)
#define RB_PREV(name, x, y)           name##_RB_PREV(y)
#define RB_MIN(name, x)               name##_RB_MINMAX(x, RB_NEGINF)
#define RB_MAX(name, x)               name##_RB_MINMAX(x, RB_INF)
#define RB_REINSERT(name, x, y)       name##_RB_REINSERT(x, y)

#define RB_FOREACH(x, name, head)                                                   \
    for ((x) = RB_MIN(name, head); (x) != NULL; (x) = name##_RB_NEXT(x))

#define RB_FOREACH_FROM(x, name, y)                                                 \
    for ((x) = (y); ((x) != NULL) && ((y) = name##_RB_NEXT(x), (x) != NULL);        \
         (x) = (y))

#define RB_FOREACH_SAFE(x, name, head, y)                                           \
    for ((x) = RB_MIN(name, head);                                                  \
         ((x) != NULL) && ((y) = name##_RB_NEXT(x), (x) != NULL); (x) = (y))

#define RB_FOREACH_REVERSE(x, name, head)                                           \
    for ((x) = RB_MAX(name, head); (x) != NULL; (x) = name##_RB_PREV(x))

#define RB_FOREACH_REVERSE_FROM(x, name, y)                                         \
    for ((x) = (y); ((x) != NULL) && ((y) = name##_RB_PREV(x), (x) != NULL);        \
         (x) = (y))

#define RB_FOREACH_REVERSE_SAFE(x, name, head, y)                                   \
    for ((x) = RB_MAX(name, head);                                                  \
         ((x) != NULL) && ((y) = name##_RB_PREV(x), (x) != NULL); (x) = (y))

#endif /* _SYS_TREE_H_ */

// clang-format on
