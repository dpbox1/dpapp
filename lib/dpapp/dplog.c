#include "dpapp/dplog.h"
#include <fcntl.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <sys/time.h>
#include <unistd.h>

#define DPLOG_BUFSZ 2048

extern int dpevp_id();

static inline int _iclamp(int v, int l, int h)
{
    int t = v < l ? l : v;
    return t < h ? t : h;
}

typedef struct
{
    dplog_level_e _level;
    int _fd;
    dplog_tsacc_e _ta; // 时间精度：0=秒，1=毫秒，2=微秒
} dplog_t;

static dplog_t* _dplog_instance()
{
    static dplog_t logger = {._level = DPLOG_L_NOTICE, ._fd = -1, ._ta = 0};
    return &logger;
}

void dplog_print(const char* fmt, ...)
{
    dplog_t* logger = _dplog_instance();

    va_list args;
    va_start(args, fmt);
    char txt[DPLOG_BUFSZ] = "";

    int sz = vsnprintf(txt, DPLOG_BUFSZ, fmt, args);
    if (sz < DPLOG_BUFSZ) {
        sz++;
    } else {
        sz = DPLOG_BUFSZ;
    }
    txt[sz - 1] = '\n';

    ssize_t wsz = 0;
    if (logger->_fd >= 0) {
        wsz = write(logger->_fd, txt, sz);
    } else {
        wsz = write(fileno(stdout), txt, sz);
    }
    if (wsz < sz) {
        // 无操作
    }

    va_end(args);
}

void dplog_vprint(dplog_level_e l, const char* domain, const char* fmt, va_list arg)
{
    dplog_t* logger = _dplog_instance();
    if ((int)l < (int)logger->_level || fmt == NULL) {
        return;
    }

    const struct tm* ntm = dplog_nowtm();

    char txt[DPLOG_BUFSZ] = "";
    int tsz = 0;
    tsz = strftime(txt, DPLOG_BUFSZ, "%FT%T", ntm);

    if (logger->_ta != 0) {
        int64_t ts = dplog_nowts(logger->_ta);
        if (logger->_ta == 1) {
            tsz += snprintf(txt + tsz, DPLOG_BUFSZ - tsz, ".%03ld", ts % 1000);
        } else {
            tsz += snprintf(txt + tsz, DPLOG_BUFSZ - tsz, ".%06ld", ts % 1000000);
        }
    }

    int id = dpevp_id();
    if (id < 0) {
        id = 0;
    }

    if (domain) {
        tsz += snprintf(txt + tsz, DPLOG_BUFSZ - tsz, "[%03d-%s](%s) ", id,
            dplog_sname(l), domain);
    } else {
        tsz += snprintf(txt + tsz, DPLOG_BUFSZ - tsz, "[%02d-%s] ", id,
            dplog_sname(l));
    }
    int sz = vsnprintf(txt + tsz, DPLOG_BUFSZ - tsz, fmt, arg);
    if (sz >= DPLOG_BUFSZ - tsz) {
        tsz = DPLOG_BUFSZ - 1;
    } else {
        tsz += sz;
    }
    txt[tsz] = '\n';
    tsz++;

    ssize_t wsz = 0;
    if (logger->_fd >= 0) {
        wsz = write(logger->_fd, txt, tsz);
    } else {
        wsz = write(fileno(stdout), txt, tsz);
    }
    if (wsz < tsz) {
        // 无操作
    }
}

dplog_level_e dplog_curlevel()
{
    return _dplog_instance()->_level;
}

dplog_tsacc_e dplog_curtsacc()
{
    return _dplog_instance()->_ta;
}

const char* dplog_curlname()
{
    return dplog_lname(dplog_curlevel());
}

void dplog_setlevel(dplog_level_e l)
{
    _dplog_instance()->_level = _iclamp(l, 0, 5);
}

void dplog_settsacc(dplog_tsacc_e ta)
{
    _dplog_instance()->_ta = (dplog_tsacc_e)_iclamp(ta, 0, 2);
}

const char* dplog_lname(dplog_level_e l)
{
    static const char* names[6] = {"debug", "info", "notice", "warning", "error",
        "alert"};
    return names[_iclamp(l, 0, 5)];
}

dplog_level_e dplog_namel(const char* n)
{
    if (strcasecmp(n, "debug") == 0) {
        return DPLOG_L_DEBUG;
    } else if (strcasecmp(n, "info") == 0) {
        return DPLOG_L_INFO;
    } else if (strcasecmp(n, "notice") == 0) {
        return DPLOG_L_NOTICE;
    } else if (strcasecmp(n, "warning") == 0) {
        return DPLOG_L_WARN;
    } else if (strcasecmp(n, "error") == 0) {
        return DPLOG_L_ERROR;
    } else if (strcasecmp(n, "alert") == 0) {
        return DPLOG_L_ALERT;
    } else {
        return DPLOG_L_ALERT;
    }
}

const char* dplog_sname(dplog_level_e l)
{
    static const char* names[6] = {"D", "I", "N", "W", "E", "A"};
    return names[_iclamp(l, 0, 5)];
}

bool dplog_init(const char* file, dplog_level_e level, dplog_tsacc_e ta)
{
    if (file == NULL) {
        file = "/dev/stdout";
    }
    dplog_t* logger = _dplog_instance();
    logger->_level = _iclamp((int)level, 0, 5);
    logger->_ta = (dplog_tsacc_e)_iclamp(ta, 0, 2);

    int fd = open(file, O_WRONLY | O_APPEND | O_CREAT, 644);
    if (fd >= 0) {
        if (logger->_fd >= 0) {
            close(logger->_fd);
        }
        logger->_fd = fd;
        return true;
    } else {
        return false;
    }
}

#define LOG(l)                                                                      \
    if ((int)l < (int)dplog_curlevel()) {                                           \
        return;                                                                     \
    }                                                                               \
    va_list args;                                                                   \
    va_start(args, fmt);                                                            \
    dplog_vprint(l, domain, fmt, args);                                             \
    va_end(args);

void dplog_write(dplog_level_e level, const char* domain, const char* fmt, ...)
{
    LOG(level);
}

void dplog_debug(const char* domain, const char* fmt, ...)
{
    LOG(DPLOG_L_DEBUG);
}
void dplog_info(const char* domain, const char* fmt, ...)
{
    LOG(DPLOG_L_INFO);
}
void dplog_notice(const char* domain, const char* fmt, ...)
{
    LOG(DPLOG_L_NOTICE);
}
void dplog_warn(const char* domain, const char* fmt, ...)
{
    LOG(DPLOG_L_WARN);
}
void dplog_error(const char* domain, const char* fmt, ...)
{
    LOG(DPLOG_L_ERROR);
}
void dplog_alert(const char* domain, const char* fmt, ...)
{
    LOG(DPLOG_L_ALERT);
}

#define ELOG(l)                                                                     \
    if (l < dplog_curlevel()) {                                                     \
        return;                                                                     \
    }                                                                               \
    va_list args;                                                                   \
    va_start(args, fmt);                                                            \
    dplog_vprint(l, NULL, fmt, args);                                               \
    va_end(args);

void dpelog_write(dplog_level_e level, const char* fmt, ...)
{
    ELOG(level);
}

void dpelog_debug(const char* fmt, ...)
{
    ELOG(DPLOG_L_DEBUG);
}
void dpelog_info(const char* fmt, ...)
{
    ELOG(DPLOG_L_INFO);
}
void dpelog_notice(const char* fmt, ...)
{
    ELOG(DPLOG_L_NOTICE);
}
void dpelog_warn(const char* fmt, ...)
{
    ELOG(DPLOG_L_WARN);
}
void dpelog_error(const char* fmt, ...)
{
    ELOG(DPLOG_L_ERROR);
}
void dpelog_alert(const char* fmt, ...)
{
    ELOG(DPLOG_L_ALERT);
}

struct dplog_time
{
    long nowSec;
    struct tm nowTm;
};

static __thread struct dplog_time _gTime = {.nowSec = 0};

const struct tm* dplog_nowtm()
{
    time_t t = time(NULL);
    if (t > _gTime.nowSec) {
        localtime_r(&t, &_gTime.nowTm);
        _gTime.nowSec = t;
    }
    return &_gTime.nowTm;
}

int64_t dplog_nowts(dplog_tsacc_e ta)
{
    struct timeval now = {0};
    gettimeofday(&now, NULL);
    unsigned long long u = now.tv_sec;

    switch (ta) {
    case DPLOG_TA_SECOND:
        return u;
    case DPLOG_TA_MILLIS:
        u *= 1000;
        u += now.tv_usec / 1000;
        return u;
    case DPLOG_TA_MICROS:
        u *= 1000000;
        u += now.tv_usec;
        return u;
    default:
        u *= 1000;
        u += now.tv_usec / 1000;
        return u;
    }
}
