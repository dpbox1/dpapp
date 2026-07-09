#include "dpapp/dpefd.h"
#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <termios.h>
#include <unistd.h>

typedef struct
{
    struct termios tio; // 最近应用的设置
    int has_tio;
    char device[128];
} dpspd_udata_t;

static speed_t _dpspd_baud_to_flag(int baud)
{
    switch (baud) {
    case 0:
        return B0;
    case 50:
        return B50;
    case 75:
        return B75;
    case 110:
        return B110;
    case 134:
        return B134;
    case 150:
        return B150;
    case 200:
        return B200;
    case 300:
        return B300;
    case 600:
        return B600;
    case 1200:
        return B1200;
    case 1800:
        return B1800;
    case 2400:
        return B2400;
    case 4800:
        return B4800;
    case 9600:
        return B9600;
    case 19200:
        return B19200;
    case 38400:
        return B38400;
#ifdef B57600
    case 57600:
        return B57600;
#endif
#ifdef B115200
    case 115200:
        return B115200;
#endif
#ifdef B230400
    case 230400:
        return B230400;
#endif
    default:
        return 0;
    }
}

static void _dpspd_apply_defaults(int* baud, char* parity, int* databits,
    int* stopbits)
{
    if (*baud <= 0)
        *baud = 115200;
    if (*parity != 'N' && *parity != 'E' && *parity != 'O')
        *parity = 'N';
    if (*databits < 5 || *databits > 8)
        *databits = 8;
    if (*stopbits != 1 && *stopbits != 2)
        *stopbits = 1;
}

static dpret_t _dpspd_configure_fd(int fd, int baud, char parity, int databits,
    int stopbits)
{
    struct termios tio;
    if (tcgetattr(fd, &tio) != 0) {
        return -errno;
    }

    speed_t sp = _dpspd_baud_to_flag(baud);
    if (sp == 0) {
        return DPE_INVAL;
    }
    cfsetispeed(&tio, sp);
    cfsetospeed(&tio, sp);

    tio.c_cflag |= (CLOCAL | CREAD);

    // 数据位
    tio.c_cflag &= ~CSIZE;
    switch (databits) {
    case 5:
        tio.c_cflag |= CS5;
        break;
    case 6:
        tio.c_cflag |= CS6;
        break;
    case 7:
        tio.c_cflag |= CS7;
        break;
    case 8:
    default:
        tio.c_cflag |= CS8;
        break;
    }

    // 校验位
    switch (parity) {
    case 'E':
        tio.c_cflag |= PARENB;
        tio.c_cflag &= ~PARODD;
        tio.c_iflag |= INPCK; // 启用奇偶校验
        break;
    case 'O':
        tio.c_cflag |= (PARENB | PARODD);
        tio.c_iflag |= INPCK;
        break;
    case 'N':
    default:
        tio.c_cflag &= ~PARENB;
        tio.c_iflag &= ~INPCK;
        break;
    }

    // 停止位
    if (stopbits == 2)
        tio.c_cflag |= CSTOPB;
    else
        tio.c_cflag &= ~CSTOPB;

    // 原始模式
    tio.c_lflag &= ~(ICANON | ECHO | ECHOE | ISIG);
    tio.c_oflag &= ~OPOST;
    tio.c_iflag &= ~(IXON | IXOFF | IXANY);

    // 最小读取设置
    tio.c_cc[VMIN] = 0;
    tio.c_cc[VTIME] = 0;

    tcflush(fd, TCIOFLUSH);
    if (tcsetattr(fd, TCSANOW, &tio) != 0) {
        return -errno;
    }
    tcflush(fd, TCIOFLUSH);

    return DPE_OK;
}

static dpret_t _dpspd_vopen(void* udata, va_list varg)
{
    const char* spdfile = va_arg(varg, const char*);
    int baud = va_arg(varg, int);      // 如 115200，≤0 则使用默认值
    int databits = va_arg(varg, int);  // 5~8，默认 8
    int stopbits = va_arg(varg, int);  // 1 或 2，默认 1
    int parity_ch = va_arg(varg, int); // 'N','E','O'（int 型），默认 'N'

    char parity = (char)parity_ch;
    _dpspd_apply_defaults(&baud, &parity, &databits, &stopbits);

    if (spdfile == NULL) {
        return DPE_INVAL;
    }

    int fd = open(spdfile, O_RDWR | O_NOCTTY);
    if (fd < 0) {
        return -errno;
    }

    dpspd_udata_t* spd = (dpspd_udata_t*)udata;
    spd->has_tio = 0;
    strncpy(spd->device, spdfile, sizeof(spd->device) - 1);
    spd->device[sizeof(spd->device) - 1] = '\0';

    dpret_t rc = _dpspd_configure_fd(fd, baud, parity, databits, stopbits);
    if (rc < 0) {
        close(fd);
        return rc;
    }

    // 应用后刷新缓存的 termios
    if (tcgetattr(fd, &spd->tio) == 0) {
        spd->has_tio = 1;
    }

    return fd;
}

static dpret_t _dpspd_copy(void* dst_udata, const void* src_udata)
{
    dpspd_udata_t* src = (dpspd_udata_t*)src_udata;
    dpspd_udata_t* dst = (dpspd_udata_t*)dst_udata;

    // 复制缓存的 termios 和设备名，使副本具有相同的视图
    dst->tio = src->tio;
    dst->has_tio = src->has_tio;
    memcpy(dst->device, src->device, sizeof(dst->device));

    return DPE_OK;
}

static void _dpspd_fini(void* udata)
{
    // 空
}

static dpele_type_t _dpspd_type = {
    .name = "spd",
    .type = DPELE_TYPE_EFD,
    .size = sizeof(dpspd_udata_t),
    .iotype = DPAIO_TYPE_GFD,
    .events = DPEVT_IN | DPEVT_OUT,
    .init = _dpspd_vopen,
    .copy = _dpspd_copy,
    .fini = _dpspd_fini,
};

const dpele_type_t* dpspd_type()
{
    return &_dpspd_type;
}

// --- 串口属性助手 ---
static int _dpspd_flag_to_baud(speed_t sp)
{
    switch (sp) {
    case B0:
        return 0;
    case B50:
        return 50;
    case B75:
        return 75;
    case B110:
        return 110;
    case B134:
        return 134;
    case B150:
        return 150;
    case B200:
        return 200;
    case B300:
        return 300;
    case B600:
        return 600;
    case B1200:
        return 1200;
    case B1800:
        return 1800;
    case B2400:
        return 2400;
    case B4800:
        return 4800;
    case B9600:
        return 9600;
    case B19200:
        return 19200;
    case B38400:
        return 38400;
#ifdef B57600
    case B57600:
        return 57600;
#endif
#ifdef B115200
    case B115200:
        return 115200;
#endif
#ifdef B230400
    case B230400:
        return 230400;
#endif
    default:
        return -1;
    }
}

const char* dpspd_device(dpefd_t* self)
{
    if (dpele_type(self) != dpspd_type()) {
        return NULL;
    }
    dpspd_udata_t* spd = (dpspd_udata_t*)dpele_aux_data(self);
    return spd ? spd->device : NULL;
}

// 确保缓存的 termios 可用，若丢失则刷新一次
static inline dpret_t _dpspd_ensure_tio(dpefd_t* self, struct termios** tio)
{
    if (dpele_type(self) != dpspd_type()) {
        return DPE_INVAL;
    }

    dpspd_udata_t* spd = (dpspd_udata_t*)dpele_aux_data(self);
    if (!spd->has_tio) {
        if (tcgetattr(dpefd_fd(self), &spd->tio) != 0)
            return -errno;
        spd->has_tio = 1;
    }
    *tio = &spd->tio;

    return DPE_OK;
}

int dpspd_baud(dpefd_t* self)
{
    struct termios* tio = NULL;
    dpret_t rc = _dpspd_ensure_tio(self, &tio);
    if (rc < 0)
        return rc;
    return _dpspd_flag_to_baud(cfgetispeed(tio));
}

int dpspd_databits(dpefd_t* self)
{
    struct termios* tio = NULL;
    dpret_t rc = _dpspd_ensure_tio(self, &tio);
    if (rc < 0)
        return rc;
    switch (tio->c_cflag & CSIZE) {
    case CS5:
        return 5;
    case CS6:
        return 6;
    case CS7:
        return 7;
    case CS8:
    default:
        return 8;
    }
}

char dpspd_parity(dpefd_t* self)
{
    struct termios* tio = NULL;
    dpret_t rc = _dpspd_ensure_tio(self, &tio);
    if (rc < 0)
        return '?';
    if (!(tio->c_cflag & PARENB))
        return 'N';
    return (tio->c_cflag & PARODD) ? 'O' : 'E';
}

int dpspd_stopbits(dpefd_t* self)
{
    struct termios* tio = NULL;
    dpret_t rc = _dpspd_ensure_tio(self, &tio);
    if (rc < 0)
        return rc;
    return (tio->c_cflag & CSTOPB) ? 2 : 1;
}
