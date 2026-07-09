-- yxfangcs<yxfangcs@yeah.net> / 20241127 / 日志模块
--
local ffi = require("ffi")

ffi.cdef([[
typedef enum
{
    DPLOG_TA_SECOND = 0,
    DPLOG_TA_MILLIS = 1,
    DPLOG_TA_MICROS = 2,
} dplog_tsacc_e;

typedef enum
{
    DPLOG_L_DEBUG = 0,
    DPLOG_L_INFO = 1,
    DPLOG_L_NOTICE = 2,
    DPLOG_L_WARN = 3,
    DPLOG_L_ERROR = 4,
    DPLOG_L_ALERT = 5,
} dplog_level_e;

int64_t dplog_nowts(dplog_tsacc_e ta);
const struct tm* dplog_nowtm();

bool dplog_init(const char* file, dplog_level_e level, dplog_tsacc_e ta);
void dplog_print(const char* fmt, ...);
void dplog_write(dplog_level_e level, const char* domain, const char* fmt, ...);
void dpelog_write(dplog_level_e level, const char* fmt, ...);
dplog_level_e dplog_curlevel();
const char* dplog_curlname();
dplog_tsacc_e dplog_curtsacc();
void dplog_setlevel(dplog_level_e l);
void dplog_settsacc(dplog_tsacc_e ta);

void dplog_debug(const char* domain, const char* fmt, ...);
void dplog_info(const char* domain, const char* fmt, ...);
void dplog_notice(const char* domain, const char* fmt, ...);
void dplog_warn(const char* domain, const char* fmt, ...);
void dplog_error(const char* domain, const char* fmt, ...);
void dplog_alert(const char* domain, const char* fmt, ...);

void dpelog_debug(const char* fmt, ...);
void dpelog_info(const char* fmt, ...);
void dpelog_notice(const char* fmt, ...);
void dpelog_warn(const char* fmt, ...);
void dpelog_error(const char* fmt, ...);
void dpelog_alert(const char* fmt, ...);
]])

local C = ffi.C
local select = select
local string_format = string.format

local M = {}

local function _fmt(fmt, ...)
    if select("#", ...) == 0 then
        return fmt
    end
    return string_format(fmt, ...)
end

function M.debug(domain, fmt, ...)
    C.dplog_debug(domain, _fmt(fmt, ...))
end

function M.info(domain, fmt, ...)
    C.dplog_info(domain, _fmt(fmt, ...))
end

function M.notice(domain, fmt, ...)
    C.dplog_notice(domain, _fmt(fmt, ...))
end

function M.warn(domain, fmt, ...)
    C.dplog_warn(domain, _fmt(fmt, ...))
end

function M.error(domain, fmt, ...)
    C.dplog_error(domain, _fmt(fmt, ...))
end

function M.alert(domain, fmt, ...)
    C.dplog_alert(domain, _fmt(fmt, ...))
end

function M.e_debug(fmt, ...)
    C.dpelog_debug(_fmt(fmt, ...))
end

function M.e_info(fmt, ...)
    C.dpelog_info(_fmt(fmt, ...))
end

function M.e_notice(fmt, ...)
    C.dpelog_notice(_fmt(fmt, ...))
end

function M.e_warn(fmt, ...)
    C.dpelog_warn(_fmt(fmt, ...))
end

function M.e_error(fmt, ...)
    C.dpelog_error(_fmt(fmt, ...))
end

function M.e_alert(fmt, ...)
    C.dpelog_alert(_fmt(fmt, ...))
end

M.L_DEBUG = 0
M.L_INFO = 1
M.L_NOTICE = 2
M.L_WARN = 3
M.L_ERROR = 4
M.L_ALERT = 5

M.TA_SECOND = C.DPLOG_TA_SECOND
M.TA_MILLIS = C.DPLOG_TA_MILLIS
M.TA_MICROS = C.DPLOG_TA_MICROS

function M.init(file, level_, ta_)
    level_ = ffi.cast("dplog_level_e",
        tonumber(level_) or tonumber(C.dplog_curlevel()))
    ta_ = ffi.cast("dplog_tsacc_e",
        tonumber(ta_) or tonumber(C.dplog_curtsacc()))
    return C.dplog_init(tostring(file), level_, ta_)
end

function M.print(fmt, ...)
    C.dplog_print(_fmt(fmt, ...))
end

function M.level(level, domain, fmt, ...)
    C.dplog_write(ffi.cast("dplog_level_e", tonumber(level)), domain,
        _fmt(fmt, ...))
end

function M.e_level(level, fmt, ...)
    C.dpelog_write(ffi.cast("dplog_level_e", tonumber(level)), _fmt(fmt, ...))
end

function M.curlevel()
    return tonumber(C.dplog_curlevel())
end

function M.curlname()
    local s = C.dplog_curlname()
    return s == nil and nil or ffi.string(s)
end

function M.curtsacc()
    return tonumber(C.dplog_curtsacc())
end

function M.setlevel(level)
    C.dplog_setlevel(ffi.cast("dplog_level_e", tonumber(level)))
end

function M.settsacc(ta)
    C.dplog_settsacc(ffi.cast("dplog_tsacc_e", tonumber(ta)))
end

function M.timestamp(ta_)
    ta_ = ffi.cast("dplog_tsacc_e", tonumber(ta_) or M.TA_MILLIS)
    return tonumber(C.dplog_nowts(ta_))
end

function M.now(fmt_)
    local stm = C.dplog_nowtm()
    if stm == nil then
        return nil
    end
    if fmt_ == nil then
        return os.date("%c", os.time({
            year = stm.tm_year + 1900,
            month = stm.tm_mon + 1,
            day = stm.tm_mday,
            hour = stm.tm_hour,
            min = stm.tm_min,
            sec = stm.tm_sec,
        }))
    end
    if fmt_ == "*t" then
        return {
            sec = stm.tm_sec,
            min = stm.tm_min,
            hour = stm.tm_hour,
            day = stm.tm_mday,
            month = stm.tm_mon + 1,
            year = stm.tm_year + 1900,
            wday = stm.tm_wday + 1,
            yday = stm.tm_yday + 1,
            isdst = stm.tm_isdst == 1,
        }
    end
    return os.date(fmt_, os.time({
        year = stm.tm_year + 1900,
        month = stm.tm_mon + 1,
        day = stm.tm_mday,
        hour = stm.tm_hour,
        min = stm.tm_min,
        sec = stm.tm_sec,
    }))
end

return M
