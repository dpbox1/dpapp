#include "dpapp/dpefd.h"
#include <errno.h>
#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

typedef struct
{
    char* path;
    int unlink_on_del; // 0 or 1
} dppip_udata_t;

// 以指定模式打开命名管道 (FIFO)
static dpret_t _dppip_vopen(void* udata, va_list varg)
{
    const char* fifo_path = va_arg(varg, const char*);
    int ev = va_arg(varg, int);
    int unlink_on_del = va_arg(varg, int);

    if (!fifo_path) {
        return DPE_INVAL;
    }

    // 确保 FIFO 存在，不存在则创建；路径存在但不是 FIFO 则报错
    struct stat st;
    if (stat(fifo_path, &st) != 0) {
        if (errno == ENOENT) {
            if (mkfifo(fifo_path, 0666) != 0 && errno != EEXIST) {
                return -errno;
            }
        } else {
            return -errno;
        }
    } else {
        if (!S_ISFIFO(st.st_mode)) {
            return DPE_INVAL;
        }
    }

    // 将 DPEVT 映射为打开模式（仅允许 IN 或 OUT 之一）
    int open_mode = 0;
    if (ev == DPEVT_IN) {
        open_mode = O_RDONLY;
    } else if (ev == DPEVT_OUT) {
        open_mode = O_WRONLY;
    } else {
        return DPE_INVAL;
    }

    // 以推导模式打开 FIFO
    int fd = open(fifo_path, open_mode);
    if (fd < 0) {
        return -errno; // 例如 EACCES, ENOENT, ENXIO
    }

    // 保存用户数据以便清理
    dppip_udata_t* pip = (dppip_udata_t*)udata;
    if (pip) {
        pip->path = strdup(fifo_path);
        if (pip->path == NULL) {
            close(fd);
            return -ENOMEM;
        }
        pip->unlink_on_del = unlink_on_del ? 1 : 0;
    }

    return fd;
}

static void _dppip_fini(void* udata)
{
    dppip_udata_t* pip = (dppip_udata_t*)udata;
    if (!pip)
        return;
    if (pip->path) {
        if (pip->unlink_on_del) {
            struct stat st;
            if (lstat(pip->path, &st) == 0 && S_ISFIFO(st.st_mode)) {
                unlink(pip->path);
            }
        }
        free(pip->path);
        pip->path = NULL;
    }
}

static dpret_t _dppip_copy(void* dst_udata, const void* src_udata)
{
    dppip_udata_t* src = (dppip_udata_t*)src_udata;
    dppip_udata_t* dst = (dppip_udata_t*)dst_udata;
    if (!dst) {
        return DPE_INVAL;
    }
    dst->path = NULL;
    dst->unlink_on_del = 0; // 复制时始终忽略 unlink 标志
    if (src && src->path) {
        dst->path = strdup(src->path);
        if (!dst->path) {
            return DPE_NOMEM;
        }
    }
    return DPE_OK;
}

static dpele_type_t _dppip_type = {
    .name = "pip",
    .type = DPELE_TYPE_EFD,
    .size = sizeof(dppip_udata_t),
    .iotype = DPAIO_TYPE_GFD,
    .events = DPEVT_IN | DPEVT_OUT,
    .init = _dppip_vopen,
    .copy = _dppip_copy,
    .fini = _dppip_fini,
};

// 导出类型函数
const dpele_type_t* dppip_type()
{
    return &_dppip_type;
}

const char* dppip_path(dpefd_t* efd)
{
    if (dpele_type(efd) != dppip_type()) {
        return NULL;
    }
    dppip_udata_t* pip = (dppip_udata_t*)dpele_aux_data(efd);
    return pip->path;
}
