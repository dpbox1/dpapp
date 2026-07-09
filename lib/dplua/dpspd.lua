-- yxfangcs<yxfangcs@yeah.net> / 20251013 / 串口（SPD）支持
--
local dpret = require("dpret")
local ffi = require("ffi")

ffi.cdef([[
/* dpele_new(dpspd_type(), const char* spdfile, int baud, int databits, int stopbits,
 * int parity)
 * - spdfile: 设备路径，例如 "/dev/ttyS0", "/dev/ttyUSB0"
 * - baud: 标准波特率（如 9600, 115200）。不支持的取值返回 DPE_INVAL
 * - databits: 5..8（默认 8）
 * - stopbits: 1 或 2（默认 1）
 * - parity: 'N'（无校验）, 'E'（偶校验）, 'O'（奇校验）（默认 'N'） */
const dpele_type_t* dpspd_type();

const char* dpspd_device(dpefd_t* self);
int dpspd_baud(dpefd_t* self);
int dpspd_databits(dpefd_t* self);
char dpspd_parity(dpefd_t* self);
int dpspd_stopbits(dpefd_t* self);
]])

local C = ffi.C
local ffi_cast = ffi.cast

local DPE_OK = dpret.OK
local DPE_INVAL = dpret.INVAL

local M = {}
M.BAUD_9600 = 9600
M.BAUD_19200 = 19200
M.BAUD_38400 = 38400
M.BAUD_57600 = 57600
M.BAUD_115200 = 115200
M.BAUD_230400 = 230400
M.BAUD_460800 = 460800
M.BAUD_921600 = 921600

-- 数据位
M.DATABITS_5 = 5
M.DATABITS_6 = 6
M.DATABITS_7 = 7
M.DATABITS_8 = 8

-- 停止位
M.STOPBITS_1 = 1
M.STOPBITS_2 = 2

-- 校验位
M.PARITY_NONE = string.byte('N')
M.PARITY_EVEN = string.byte('E')
M.PARITY_ODD = string.byte('O')

M.type = C.dpspd_type()

-- 打开串口设备
-- @param device: string - 设备路径，例如 "/dev/ttyS0", "/dev/ttyUSB0"
-- @param baud: number - 波特率（默认 115200）
-- @param databits: number - 数据位 5-8（默认 8）
-- @param stopbits: number - 停止位 1 或 2（默认 1）
-- @param parity: number 或 string - 校验 'N', 'E', 'O'（默认 'N'）
-- @return spd: dpele_t* - 串口元素，或 nil 及错误码
function M.open(device, baud_, databits_, stopbits_, parity_)
    if not device or type(device) ~= 'string' then
        return nil, DPE_INVAL
    end

    local parity = M.PARITY_NONE
    if type(parity_) == 'string' then
        parity = string.byte(parity_:upper())
    elseif type(parity_) == 'number' then
        parity = parity_
    end

    local baud = ffi_cast("int", tonumber(baud_) or 115200)
    local databits = ffi_cast("int", tonumber(databits_) or 8)
    local stopbits = ffi_cast("int", tonumber(stopbits_) or 1)
    parity = ffi_cast("int", parity)
    local spd = C.dpele_new(M.type, tostring(device), baud, databits, stopbits,
        parity)
    if spd == nil then
        return nil, ffi.errno()
    end

    return spd, DPE_OK
end

-- 获取串口设备路径
function M.device(spd)
    if not spd then
        return nil
    end

    local dev = C.dpspd_device(spd)
    if dev == nil then
        return nil
    end

    return ffi.string(dev)
end

-- 获取串口波特率
function M.baud(spd)
    if not spd then
        return DPE_INVAL
    end

    return C.dpspd_baud(spd)
end

-- 获取串口数据位
function M.databits(spd)
    if not spd then
        return DPE_INVAL
    end

    return C.dpspd_databits(spd)
end

-- 获取串口校验位
-- @return string - 'N', 'E', 'O', 或 '?'
function M.parity(spd)
    if not spd then
        return '?'
    end

    local p = C.dpspd_parity(spd)
    return string.char(p)
end

-- 获取串口停止位
function M.stopbits(spd)
    if not spd then
        return DPE_INVAL
    end

    return C.dpspd_stopbits(spd)
end

return M
