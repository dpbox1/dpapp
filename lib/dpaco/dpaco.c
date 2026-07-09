#include "dpaco/dpaco.h"
#include "dpaco/dpaco_arch.h"
#include <assert.h>
#include <signal.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct dpaco
{
    cocontext_t ctx;
    dpaco_fun_t func;
    dpv64_t msg;
    union
    {
        struct dpaco* next;
        struct dpaco* prev;
    };
    char status;
    bool delater;
    int size;
    char stack[];
};

typedef struct dp_aco_thenv
{
    dpaco_t* comain;
    dpaco_t* cocurr;
    dpaco_t* recycle;
    int recycle_count;
    int stack_size;
} dp_aco_thenv_t;

static __thread dp_aco_thenv_t _aco_thenv = {NULL, NULL, NULL, 0};

bool dpaco_thinit(int stack_size)
{
    dp_aco_thenv_t* env = &_aco_thenv;
    if (env->comain) {
        return false;
    }

    dpaco_t* mainco = calloc(1, sizeof(dpaco_t));
    if (mainco == NULL) {
        return false;
    }
    mainco->status = DPACO_RUNNING;

    env->cocurr = mainco;
    env->comain = mainco;
    env->recycle = NULL;
    env->recycle_count = 0;
    env->stack_size = 128 * 1024;
    if (stack_size > 0) {
        if (stack_size < 1024) {
            env->stack_size = 1024;
        } else {
            env->stack_size = stack_size;
        }
    }
    return true;
}

void dpaco_thfree()
{
    dp_aco_thenv_t* tenv = &_aco_thenv;
    if (tenv->comain) {
        free(tenv->comain);
    }
    dpaco_t* next = NULL;
    while (tenv->recycle) {
        next = tenv->recycle;
        tenv->recycle = next->next;
        free(next);
    }
    memset(tenv, 0, sizeof(dp_aco_thenv_t));
}

static void _mainfunc(dpaco_t* co)
{
    dpv64_t res = co->func(co->msg);
    co->status = DPACO_DEAD;
    dpaco_yield(res);
}

dpaco_t* dpaco_create(dpaco_fun_t fun, int stack_size)
{
    if (fun == NULL) {
        return NULL;
    }

    dpaco_t* co = NULL;
    dp_aco_thenv_t* tenv = (dp_aco_thenv_t*)&_aco_thenv;
    if (stack_size <= 0) {
        stack_size = tenv->stack_size;
    }

    if (tenv->recycle) {
        co = tenv->recycle;
        tenv->recycle = co->next;
        tenv->recycle_count--;
    } else {
        co = calloc(1, sizeof(dpaco_t) + stack_size);
        if (co == NULL)
            return NULL;
        co->size = stack_size;
    }

    co->func = fun;
    co->status = DPACO_CREATED;
    co->next = NULL;
    co->delater = false;

    if (co->size != stack_size) {
        dpaco_t* cotmp = realloc(co, sizeof(dpaco_t) + stack_size);
        if (cotmp != NULL) {
            co = cotmp;
            co->size = stack_size;
        }
        // realloc 失败则使用旧 co
        // else {
        //     return NULL;
        // }
    }

    cocontext_init(&co->ctx, co->stack, co->size, _mainfunc, co);

    return co;
}

dpv64_t dpaco_resume(dpaco_t* co, dpv64_t val)
{
    if (co == NULL)
        return DPV64_NULL;
    dp_aco_thenv_t* tenv = (dp_aco_thenv_t*)&_aco_thenv;
    dpaco_t* curr = tenv->cocurr;

    switch (co->status) {
    case DPACO_CREATED:
    case DPACO_SUSPEND: {
        co->prev = curr;
        co->msg = val;
        co->status = DPACO_RUNNING;
        curr->status = DPACO_SUSPEND;
        tenv->cocurr = co;
        cocontext_swap(&curr->ctx, &co->ctx);
        break;
    }
    default:
        return DPV64_NULL;
    }

    if (co->status == DPACO_DEAD && co->delater) {
        dpaco_delater(co);
    }
    return tenv->cocurr->msg;
}

dpv64_t dpaco_yield(dpv64_t val)
{
    dp_aco_thenv_t* tenv = (dp_aco_thenv_t*)&_aco_thenv;
    dpaco_t* curr = tenv->cocurr;
    dpaco_t* prev = curr->prev;
    if (prev == NULL || curr == NULL) {
        return DPV64_NULL;
    }
    if (curr->status != DPACO_DEAD) {
        curr->status = DPACO_SUSPEND;
    }
    prev->status = DPACO_RUNNING;
    prev->msg = val;
    tenv->cocurr = prev;

    cocontext_swap(&curr->ctx, &prev->ctx);

    return tenv->cocurr->msg;
}

dpaco_status_e dpaco_status(dpaco_t* co)
{
    return co->status;
}

dpaco_t* dpaco_running()
{
    dp_aco_thenv_t* tenv = (dp_aco_thenv_t*)&_aco_thenv;
    return tenv->cocurr;
}

void dpaco_delater(dpaco_t* co)
{
    if (co == NULL) {
        return;
    }

    dp_aco_thenv_t* tenv = (dp_aco_thenv_t*)&_aco_thenv;
    switch (co->status) {
    case DPACO_CREATED:
    case DPACO_DEAD: {
        if (tenv->recycle_count < 1024) {
            co->func = NULL;
            co->msg.ptr = NULL;
            co->delater = false;
            co->next = tenv->recycle;
            tenv->recycle = co;
            tenv->recycle_count++;
        } else {
            free(co);
        }
        break;
    }
    default: {
        co->delater = true;
    }
    }
}

dpv64_t dpaco_wrap(dpaco_fun_t fun, dpv64_t val, int stack_size)
{
    dpaco_t* co = dpaco_create(fun, stack_size);
    if (co) {
        dpv64_t res = dpaco_resume(co, val);
        dpaco_delater(co);
        return res;
    }
    return DPV64_NULL;
}
