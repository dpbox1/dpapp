-- yxfangcs<yxfangcs@yeah.net> / 20241127 / Linux 文件系统通知
local dpasc = require("dpasc")
local ffi = require("ffi")
local mpath = require("path")
local dplog = require("dplog")
local dpret = require("dpret")
require("dpele")

ffi.cdef([[
struct inotify_event
{
    int wd;
    uint32_t mask;
    uint32_t cookie;
    uint32_t len;
    char name[1024];
};

typedef struct inotify_event notinfo;

const dpele_type_t* dpfsm_type();
dpret_t dpfsm_addev(dpefd_t* self, const char* path, uint32_t mask);
dpret_t dpfsm_delev(dpefd_t* self, int wd);
]])

local C = ffi.C

local DPE_OK = dpret.OK
local DPE_INVAL = dpret.INVAL
local DPE_NOTEXISTS = dpret.NOTEXISTS
local dpret_isok = dpret.isok
local ffi_sizeof = ffi.sizeof

local bit = require("bit")

local M = {
    -- 支持的事件，适用于 INOTIFY_ADD_WATCH 的 MASK 参数。
    ["ACCESS"] = 0x00000001, -- 文件被访问。
    ["MODIFY"] = 0x00000002, -- 文件被修改。
    ["ATTRIB"] = 0x00000004, -- 元数据变更。
    ["CLOSE_WRITE"] = 0x00000008, -- 可写文件被关闭。
    ["CLOSE_NOWRITE"] = 0x00000010, -- 不可写文件被关闭。
    ["OPEN"] = 0x00000020, -- 文件被打开。
    ["MOVED_FROM"] = 0x00000040, -- 文件从 X 移出。
    ["MOVED_TO"] = 0x00000080, -- 文件移入 Y。
    ["CREATE"] = 0x00000100, -- 子文件被创建。
    ["DELETE"] = 0x00000200, -- 子文件被删除。
    ["DELETE_SELF"] = 0x00000400, -- 自身被删除。
    ["MOVE_SELF"] = 0x00000800, -- 自身被移动。

    -- 内核发送的事件。
    ["UNMOUNT"] = 0x00002000, -- 后备文件系统被卸载。
    ["Q_OVERFLOW"] = 0x00004000, -- 事件队列溢出。
    ["IGNORED"] = 0x00008000, -- 文件被忽略。

    -- 辅助事件。
    ["CLOSE"] = bit.bor(0x00000008, 0x00000010), -- (CLOSE_WRITE | CLOSE_NOWRITE), -- 关闭。
    ["MOVE"] = bit.bor(0x00000040, 0x00000080), -- (MOVED_FROM | MOVED_TO), -- 移动。

    -- 特殊标志。
    ["ONLYDIR"] = 0x01000000, -- 仅当路径为目录时监视。
    ["DONT_FOLLOW"] = 0x02000000, -- 不跟随符号链接。
    ["EXCL_UNLINK"] = 0x04000000, -- 排除已解除链接对象的事件。
    ["MASK_CREATE"] = 0x10000000, -- 仅创建监视。
    ["MASK_ADD"] = 0x20000000, -- 添加到已有监视的掩码。
    ["ISDIR"] = 0x40000000, -- 事件发生于目录。
    ["ONESHOT"] = 0x80000000, -- 仅发送一次事件。

    -- 程序可等待的所有事件。
    ["ALL_EVENTS"] = bit.bor(0x00000001, 0x00000002, 0x00000004, 0x00000008,
        0x00000010, 0x00000020, 0x00000040, 0x00000080, 0x00000100, 0x00000200,
        0x00000400, 0x00000800),
    --[[(ACCESS | MODIFY | ATTRIB | CLOSE_WRITE
        | CLOSE_NOWRITE | OPEN | MOVED_FROM | MOVED_TO | CREATE | DELETE
        | DELETE_SELF | MOVE_SELF),--]]
}

M.type = C.dpfsm_type()

local fsm_epfd = nil
local fsm_path_wd = {}
local fsm_wd_path = {}

local function _check_init_fsm()
    if not fsm_epfd then
        fsm_epfd = C.dpele_new(M.type)
        fsm_epfd:setgc(true)
    end
    return fsm_epfd
end

function M.append(path, mask)
    if type(path) == 'string' and type(mask) == 'number' and mpath.exists(path) then
        path = mpath.fullpath(path)

        local epfd = _check_init_fsm()
        local wd = fsm_path_wd[path]
        if wd then
            C.dpfsm_delev(epfd, wd)
            fsm_path_wd[path] = nil
            fsm_wd_path[wd] = nil
        end

        wd = C.dpfsm_addev(epfd, path, mask)
        if wd >= 0 then
            fsm_path_wd[path] = wd
            fsm_wd_path[wd] = path
            return DPE_OK
        else
            return wd
        end
    else
        return DPE_INVAL
    end
end

function M.remove(path)
    path = mpath.fullpath(path)
    if path == nil then
        return DPE_INVAL
    end

    local wd = fsm_path_wd[path]
    if wd then
        local ret = C.dpfsm_delev(_check_init_fsm(), wd)
        fsm_path_wd[path] = nil
        fsm_wd_path[wd] = nil
        return ret
    else
        return DPE_NOTEXISTS
    end
end

local fsm_info = ffi.new("notinfo")
function M.read()
    local efd = _check_init_fsm()
    local ret = dpasc.gfd_read(efd, fsm_info, ffi_sizeof("notinfo"))
    if not dpret_isok(ret) then
        return nil, ret
    end

    return {
        mask = fsm_info.mask,
        cookie = fsm_info.cookie,
        name = ffi.string(fsm_info.name, math.min(1024, fsm_info.len)),
        path = fsm_wd_path[fsm_info.wd],
    }, DPE_OK
end

return M
