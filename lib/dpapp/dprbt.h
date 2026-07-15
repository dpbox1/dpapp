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
 *  @brief NetBSD/OpenBSD tree macros: splay trees and rank-balanced BSTs.
 *
 *  Pure C macro implementation with no runtime API; `RB_*`/`SPLAY_*` generate
 *  inline functions at compile time. Derived from `sys/tree.h` for ordered
 *  containers in dpapp. */

// clang-format off

#ifndef _SYS_TREE_H_
#define _SYS_TREE_H_

#include <stddef.h>
#include <stdint.h>

/*
 * This file defines data structures for different types of trees:
 * splay trees and rank-balanced trees.
 *
 * A splay tree is a self-organizing data structure.  Every operation
 * on the tree causes a splay to happen.  The splay moves the requested
 * node to the root of the tree and partly rebalances it.
 *
 * This has the benefit that request locality causes faster lookups as
 * the requested nodes move to the top of the tree.  On the other hand,
 * every lookup causes memory writes.
 *
 * The Balance Theorem bounds the total access time for m operations
 * and n inserts on an initially empty tree as O((m + n)lg n).  The
 * amortized cost for a sequence of m accesses to a splay tree is O(lg n);
 *
 * A rank-balanced tree is a binary search tree with an integer
 * rank-difference as an attribute of each pointer from parent to child.
 * The sum of the rank-differences on any path from a node down to null is
 * the same, and defines the rank of that node. The rank of the null node
 * is -1.
 *
 * Different additional conditions define different sorts of balanced
 * trees, including "red-black" and "AVL" trees.  The set of conditions
 * applied here are the "weak-AVL" conditions of Haeupler, Sen and Tarjan:
 *    - every rank-difference is 1 or 2.
 *    - the rank of any leaf is 1.
 *
 * For historical reasons, rank differences that are even are associated
 * with the color red (Rank-Even-Difference), and the child that a red edge
 * points to is called a red child.
 *
 * Every operation on a rank-balanced tree is bounded as O(lg n).
 * The maximum height of a rank-balanced tree is 2lg (n+1).
 */

#define SPLAY_HEAD(name, type)                                                      \
    struct name                                                                     \
    {                                                                               \
        struct type* sph_root; /* root of the tree */                               \
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
        struct type* spe_left;  /* left element */                                   \
        struct type* spe_right; /* right element */                                 \
    }

#define SPLAY_LEFT(elm, field)  (elm)->field.spe_left
#define SPLAY_RIGHT(elm, field) (elm)->field.spe_right
#define SPLAY_ROOT(head)        (head)->sph_root
#define SPLAY_EMPTY(head)       (SPLAY_ROOT(head) == NULL)

/* SPLAY_ROTATE_{LEFT,RIGHT} expect that tmp hold SPLAY_{RIGHT,LEFT} */
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

/* Generates prototypes and inline functions */

#define SPLAY_PROTOTYPE(name, type, field, cmp)                                     \
    void name##_SPLAY(struct name*, struct type*);                                  \
    void name##_SPLAY_MINMAX(struct name*, int);                                    \
    struct type* name##_SPLAY_INSERT(struct name*, struct type*);                   \
    struct type* name##_SPLAY_REMOVE(struct name*, struct type*);                   \
                                                                                    \
    /* Finds the node with the same key as elm */                                   \
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

/* Main splay operation.
 * Moves node close to the key of elm to top
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
    /* Splay with either the minimum or the maximum element
     * Used to find the minimum or maximum element in the tree.
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

/* Macros that define a rank-balanced tree */
#define RB_HEAD(name, type)                                                         \
    struct name                                                                     \
    {                                                                               \
        struct type* rbh_root; /* root of the tree */                               \
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
 * Expects addresses of struct type objects to be a multiple of 4, so the
 * least significant two bits of pointers to such types are always 0. This
 * implementation uses these bits to mark a tree node's left or right child
 * as "red".
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
 * RB_AUGMENT or RB_AUGMENT_CHECK is called in a loop at the root of each
 * modified subtree, from the bottom up to the root, to update augmented node
 * data. RB_AUGMENT_CHECK returns true only when the update changed node data,
 * so updates can stop early when it returns false.
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
 * The RB_ROTATE macro partially restructures the tree to improve balance.
 * When dir is _RB_L, tmp is elm's right child. After rotation, elm becomes
 * tmp's left child, and the subtree that was between them (formerly attached
 * to the left of tmp) is now attached to the right of elm. The parent-child
 * relationship between elm and its original parent is unchanged; this macro
 * does not update those fields, leaving that to RB_ROTATE's caller, to avoid
 * a pair of rotations updating the same pointer fields twice with different
 * values.
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

/* Generates prototypes and inline functions */
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

/* Main RB operation.
 * Moves node close to the key of elm to top
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
     * Returns the rank of the subtree rooted at elm, or -1 if the subtree is          \
     * unbalanced or augmented data is inconsistent.                                   \
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
         * Initially, elm is a leaf. Its parent was either a leaf (with two black null   \
         * children) or an internal node with one black non-null child and one red null  \
         * child. The balance rule "rank of any leaf is 1" excludes the case where the   \
         * initial parent has two red null children. Therefore the first loop iteration  \
         * does not access uninitialized 'child'; later iterations only occur after       \
         * 'child' was assigned in a previous iteration.                                 \
         */                                                                         \
        struct type *child = NULL, *child_up = NULL, *gpar = NULL;                  \
        uintptr_t elmdir, sibdir;                                                   \
                                                                                    \
        do {                                                                        \
            /* The tree rooted at elm grew in rank */                           \
            gpar = _RB_UP(parent, field, op);                                       \
            elmdir = RB_RIGHT(parent, field, op) == elm ? _RB_R : _RB_L;            \
            if (_RB_BITS(gpar) & elmdir) {                                          \
                /* Shorten parent-elm edge to rebalance */                      \
                _RB_BITSUP(parent, field, op) ^= elmdir;                            \
                return (NULL);                                                      \
            }                                                                       \
            sibdir = elmdir ^ _RB_LR;                                               \
            /* The other edge must change length */                                 \
            _RB_BITSUP(parent, field, op) ^= sibdir;                                \
            if ((_RB_BITS(gpar) & _RB_LR) == 0) {                                   \
                /* Both edges shortened, retry from parent */                       \
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
                /* Do not update elm's augmented data if child is a leaf,        \
                 * since it will be restored to a leaf afterward. */                                     \
                if ((_RB_BITS(child_up) & _RB_LR) == 0)                             \
                    elm = child;                                                    \
            } else                                                                  \
                child = elm;                                                        \
                                                                                    \
            RB_ROTATE(parent, child, sibdir, field, op);                            \
            _RB_UP(child, field, op) = gpar;                                        \
            RB_SWAP_CHILD(head, gpar, parent, child, field, op);                    \
            /*                                                                      \
             * Elements rotated down have a new, smaller subtree, so update their        \
             * augmented data.                                                         \
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
 * In REMOVE_COLOR, for the single-rotation case in HST paper Figure 3, 'parent'
 * has rank 1 higher and its rank is lowered if 'parent' becomes a leaf. This
 * implementation always gives parent the lower rank at its new position to avoid
 * leaf checks. Define RB_STRICT_HST as 1 to get the behavior described by HST.
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
            /* Deleting a leaf that was an only child creates a                      \
             * rank-2 leaf. Lower that leaf's rank. */                                    \
            _RB_UP(parent, field, op) = _RB_PTR(_RB_UP(parent, field, op));         \
            elm = parent;                                                           \
            if ((parent = _RB_UP(elm, field, op)) == NULL)                          \
                return (NULL);                                                      \
        }                                                                           \
        do {                                                                        \
            /* The tree rooted at elm shrank in rank */                         \
            gpar = _RB_UP(parent, field, op);                                       \
            elmdir = RB_RIGHT(parent, field, op) == elm ? _RB_R : _RB_L;            \
            _RB_BITS(gpar) ^= elmdir;                                               \
            if (_RB_BITS(gpar) & elmdir) {                                          \
                /* Lengthen parent-elm edge to rebalance */                     \
                _RB_UP(parent, field, op) = gpar;                                   \
                return (NULL);                                                      \
            }                                                                       \
            if (_RB_BITS(gpar) & _RB_LR) {                                          \
                /* Shorten the other edge, retry from parent */                         \
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
                /* Shorten edge from sib, retry */                      \
                _RB_UP(sib, field, op) = up;                                        \
                continue;                                                           \
            }                                                                       \
            if ((_RB_BITS(up) & sibdir) == 0) {                                     \
                elm = _RB_LINK(sib, elmdir, field, op);                             \
                /* elm is 1-child, rotate elm first. */                       \
                RB_ROTATE(sib, elm, sibdir, field, op);                             \
                up = _RB_UP(elm, field, op);                                        \
                _RB_BITSUP(parent, field, op) ^= (_RB_BITS(up) & elmdir) ? _RB_LR   \
                                                                         : elmdir;  \
                _RB_BITSUP(sib, field, op) ^= (_RB_BITS(up) & sibdir) ? _RB_LR      \
                                                                      : sibdir;     \
                _RB_BITSUP(elm, field, op) |= _RB_LR;                               \
            } else {                                                                \
                if ((_RB_BITS(up) & elmdir) == 0 && RB_STRICT_HST && elm != NULL) { \
                    /* If parent will not become a leaf,                            \
                       do not lower parent's rank yet. */                                 \
                    _RB_BITSUP(parent, field, op) ^= sibdir;                        \
                    _RB_BITSUP(sib, field, op) ^= _RB_LR;                           \
                } else if ((_RB_BITS(up) & elmdir) == 0) {                          \
                    /* Lower parent's rank. */                                            \
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
            /* If rotation made 'parent' the root of the same subtree as before,      \
             * do not update its augmented data. */                           \
            if (parent == in && RB_LEFT(parent, field, op) == NULL) {               \
                opar = NULL;                                                        \
                parent = RB_PARENT(parent, field, op);                              \
            }                                                                       \
            _RB_AUGMENT_WALK(parent, opar, field, op);                              \
            if (opar != NULL) {                                                     \
                /*                                                                  \
                 * An element rotated into the search path has a changed subtree; if       \
                 * AUGMENT_WALK did not update it, update augmented data.                  \
                 */                                                                 \
                (void)RB_AUGMENT_CHECK(opar);                                       \
                (void)RB_AUGMENT_CHECK(RB_PARENT(opar, field, op));                 \
            }                                                                       \
        }                                                                           \
        return (out);                                                               \
    }

#define RB_GENERATE_INSERT_FINISH(name, type, field, op, attr)                      \
    /* Inserts a node into the RB tree */                                           \
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
    /* Inserts a node into the RB tree */                                           \
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
    /* Finds the node with the same key as elm */                                   \
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
    /* Finds the first node greater than or equal to the search key */              \
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
    /* Inserts a node at the next position in the RB tree */                      \
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
    /* Inserts a node at the previous position in the RB tree */                      \
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
            /* XXXLAS: Remove/insert is heavy handed. */                            \
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
